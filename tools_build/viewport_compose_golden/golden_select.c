/*
 * golden_select.c
 *
 * Self-contained golden oracle for Dragon Wars *viewport COMPOSE step 2* — the
 * COMPONENT SELECTION + DRAW SEQUENCE stage of refresh_viewport. Step 1
 * (golden_compose.c) already covered the FOV sampling (data_5A56). This oracle
 * continues from there and reproduces, VERBATIM (modulo whitespace), the
 * selection chain of refresh_viewport (engine.c:5618) plus the helpers it
 * depends on:
 *
 *   - read_level_metadata    (engine.c:5061)  -> data_56C6 / data_56E5 / data_5897
 *   - cache_level_components  (engine.c:5037) -> data_56E5[0..3 / 4..6 / 8..]
 *   - cache_resources         (engine.c:5468) -> data_5897[i+0xF], data_59E4[]
 *   - draw_viewport_sky       (engine.c:5533) -> sky component selection
 *   - the ground loop         (engine.c:5701..5728)
 *   - the "other" loop        (engine.c:5732..5776) — has the
 *     `Drawing component %d (tag: %d)` printf we pin against.
 *
 * IMPORTANT scope boundary (VIEWPORT_COMPOSE.md step 2): we DO NOT load real
 * DATA1 sprite bytes and we DO NOT blit pixels. We only need:
 *   - the SELECTION (which resource index `al` each slot picks), and
 *   - the resulting `tag` for the `Drawing component` log.
 *
 * The full resource subsystem reduces to one fact for selection purposes:
 *   resource_load(sec).tag == sec      (game_memory_alloc sets tag = sec)
 * and the level wires sec via  sec = (data_5897[i] & 0x7F) + 0x6E.
 * So every "draw" we record carries a tag computable WITHOUT touching DATA1.
 *
 * We model the resource table just enough to mirror cache_resources:
 *   - resource_load(sec):  if a slot with tag==sec exists return it,
 *                          else allocate the next free slot, tag=sec.
 *   - resource_get_by_index(i): allocations[i].
 *   - allocations[0] and [1] are statically reserved (rm_init), so the first
 *     dynamic load lands at index 2 — this matters because data_5897[i+0xF]
 *     stores the *index*, and data_59E4[i] is indexed by that.
 *
 * draw_sprite_to_viewport is STUBBED to record (component, tag, sprite_offset,
 * xpos, ypos, byte_104E) instead of decoding. Because refresh_viewport resets
 * word_104F=0 before every component's single draw call, stubbing the size /
 * word_104F accumulation does not perturb the selection sequence.
 *
 * Output (golden): one line per recorded draw call, in opendw emission order
 *   SKY (if drawn), then GROUND counter 8..0, then OTHER counter 23..0.
 *
 * Usage:
 *   golden_select <level.lvl> <facing> <x> <y> [out.golden] [-v]
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* ------------------------------------------------------------------ */
/* cpu model (subset of opendw's cpu struct)                           */
/* ------------------------------------------------------------------ */
static struct { uint16_t ax, bx, cx, dx, si, di; int cf; } cpu;

static uint16_t cpu_stack[64];
static int cpu_sp = 0;
static void push_word(uint16_t v) { cpu_stack[cpu_sp++] = v; }
static uint16_t pop_word(void) { return cpu_stack[--cpu_sp]; }

/* ------------------------------------------------------------------ */
/* game state                                                          */
/* ------------------------------------------------------------------ */
static struct { unsigned char unknown[256]; } game_state;

/* ------------------------------------------------------------------ */
/* resource model (stub of resource.c, selection-only)                 */
/* ------------------------------------------------------------------ */
struct resource { unsigned char *bytes; size_t len; int usage_type; int tag; int index; };
static struct resource allocations[128];

/* rm_init: slots 0/1 are statically reserved (usage_type 0xFF). */
static void rm_init(void)
{
  memset(allocations, 0, sizeof(allocations));
  allocations[0].usage_type = 0xFF; allocations[0].tag = 0xFFFF; allocations[0].index = 0;
  allocations[1].usage_type = 0xFF; allocations[1].tag = 0xFFFE; allocations[1].index = 1;
}

static int find_index_by_tag(int tag)
{
  for (int i = 0; i < 128; i++)
    if (allocations[i].tag == tag && allocations[i].usage_type != 0) return i;
  return -1;
}

/* resource_load(sec): tag = sec; allocate next free slot if cache miss.
 * We do NOT read DATA1 — bytes stays NULL (selection doesn't need it). */
static struct resource *resource_load(int sec)
{
  int idx = find_index_by_tag(sec);
  if (idx != -1) return &allocations[idx];
  int i;
  for (i = 0; i < 128; i++) if (allocations[i].usage_type == 0) break;
  allocations[i].usage_type = 1;
  allocations[i].tag = sec;
  allocations[i].index = i;
  allocations[i].bytes = NULL;
  allocations[i].len = 0;
  return &allocations[i];
}

static struct resource *resource_get_by_index(int index) { return &allocations[index]; }

/* ------------------------------------------------------------------ */
/* level globals (mirror engine.c)                                     */
/* ------------------------------------------------------------------ */
static unsigned char *data_5521;   /* level resource bytes              */
static uint8_t  byte_551E;
static uint16_t word_551F;
static uint16_t word_11C6, word_11C8, word_11CA, word_11CC;
static uint16_t word_5864;
static uint16_t data_5A04[128];
static unsigned char data_5A56[128];

static unsigned char data_5897[256];
static unsigned char data_56C6[128];
static unsigned char data_56E5[128];
static struct resource *data_59E4[128];

/* refresh_viewport draw-call scratch */
static struct resource *word_1051;
static uint16_t word_104F;
static uint8_t  byte_104E;

/* the level resource (== resource_load(area + 0x46)). We pre-load it as
 * allocations[2] from the .lvl file (already decompressed on disk). */
static struct resource *level_res;

/* FOV step table (engine.c:191/197), copied verbatim. */
static unsigned short data_5303[] = { 0x0016, 0x002E, 0x0046, 0x005E };
static unsigned char data_530B[] = {
  0xff, 0x03, 0x00, 0x03, 0x01, 0x03, 0xff, 0x02, 0x00, 0x02, 0x01, 0x02,
  0xff, 0x01, 0x00, 0x01, 0x01, 0x01, 0xff, 0x00, 0x00, 0x00, 0x01, 0x00,
  0x03, 0x01, 0x03, 0x00, 0x03, 0xff, 0x02, 0x01, 0x02, 0x00, 0x02, 0xff,
  0x01, 0x01, 0x01, 0x00, 0x01, 0xff, 0x00, 0x01, 0x00, 0x00, 0x00, 0xff,
  0x01, 0xfd, 0x00, 0xfd, 0xff, 0xfd, 0x01, 0xfe, 0x00, 0xfe, 0xff, 0xfe,
  0x01, 0xff, 0x00, 0xff, 0xff, 0xff, 0x01, 0x00, 0x00, 0x00, 0xff, 0x00,
  0xfd, 0xff, 0xfd, 0x00, 0xfd, 0x01, 0xfe, 0xff, 0xfe, 0x00, 0xfe, 0x01,
  0xff, 0xff, 0xff, 0x00, 0xff, 0x01, 0x00, 0xff, 0x00, 0x00, 0x00, 0x01
};

/* Static viewport placement tables (engine.c:213..291), copied verbatim. */
struct ui_point { short x, y; };
static unsigned short data_558F[] = {
  0x0020, 0x0000, 0x0080, 0xFFC0, 0x0080, 0x0020, 0xFFC0, 0x0080,
  0x0030, 0x0020, 0x0070, 0xFFF0, 0x0070, 0x0030, 0xFFF0, 0x0070,
  0x0040, 0x0030, 0x0060, 0x0020, 0x0060, 0x0040, 0x0020, 0x0060
};
static unsigned short data_55BF[] = {
  0x0010, 0x0000, 0x0000, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010,
  0x0020, 0x0010, 0x0010, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020,
  0x0030, 0x0020, 0x0020, 0x0030, 0x0030, 0x0030, 0x0030, 0x0030
};
static unsigned short data_55EF[] = {
  0x0016, 0x000A, 0x000B, 0x0015, 0x0017, 0x008A, 0x0089, 0x008B,
  0x0013, 0x0007, 0x0008, 0x0012, 0x0014, 0x0087, 0x0086, 0x0088,
  0x0010, 0x0004, 0x0005, 0x000F, 0x0011, 0x0084, 0x0083, 0x0085
};
static unsigned short data_561F[] = {
  0x0004, 0x000C, 0x000C, 0x0004, 0x0004, 0x0004, 0x0004, 0x0004,
  0x0006, 0x000E, 0x000E, 0x0006, 0x0006, 0x0006, 0x0006, 0x0006,
  0x0008, 0x0010, 0x0010, 0x0008, 0x0008, 0x0008, 0x0008, 0x0008
};
static unsigned short data_564F[] = {
  0x0001, 0x0000, 0x0080, 0x0001, 0x0001, 0x0000, 0x0000, 0x0000,
  0x0001, 0x0000, 0x0080, 0x0001, 0x0001, 0x0000, 0x0000, 0x0000,
  0x0001, 0x0000, 0x0080, 0x0001, 0x0001, 0x0000, 0x0000, 0x0000
};
static struct ui_point ground_points[] = {
  { 16, 120 }, { 0, 120 }, { 128, 120 },
  { 32, 104 }, { 0, 104 }, { 112, 104},
  { 48, 88 }, { 0, 88 }, { 96, 88 }
};
static unsigned short data_56A3[] = {
  0x000A, 0x0009, 0x000B, 0x0007, 0x0006, 0x0008, 0x0004, 0x0003, 0x0005
};
static unsigned short sprite_indices[] = { 18, 16, 20, 12, 10, 14, 6, 4, 8 };
static unsigned short data_575C[] = { 0x4040, 0x0404, 0, 0 };

static int verbose = 0;

/* ---- recorded draw-call log (golden output) -------------------------- */
struct draw_rec {
  const char *batch;   /* "SKY" / "GND" / "OTH" */
  int counter;         /* loop counter (sky=-1) */
  int tag;             /* r->tag */
  int sprite_offset;   /* sprite_offset arg */
  short xpos, ypos;
  int byte_104E;
};
static struct draw_rec draws[64];
static int ndraws = 0;
static void record_draw(const char *batch, int counter, struct resource *r,
                        int sprite_offset, short xpos, short ypos, int b104e)
{
  draws[ndraws].batch = batch;
  draws[ndraws].counter = counter;
  draws[ndraws].tag = r ? r->tag : -1;
  draws[ndraws].sprite_offset = sprite_offset;
  draws[ndraws].xpos = xpos;
  draws[ndraws].ypos = ypos;
  draws[ndraws].byte_104E = b104e;
  ndraws++;
}

/* ================================================================== */
/* VERBATIM ports from engine.c — boundary / sampling (same as step1)  */
/* ================================================================== */
static void check_map_boundary_x(void)
{
  uint8_t bl = cpu.bx & 0xFF;
  if (bl < game_state.unknown[0x22]) return;
  if ((game_state.unknown[0x23] & 0x2) != 0) { printf("bx unimpl\n"); exit(1); }
  byte_551E = 0x80;
  if (bl < 0x80) { bl = game_state.unknown[0x22]; bl--; cpu.bx = (cpu.bx & 0xFF00) | bl; return; }
  cpu.bx = cpu.bx & 0xFF00;
}
static void check_map_boundary_y(void)
{
  uint8_t dl = cpu.dx & 0xFF;
  if (dl < game_state.unknown[0x21]) return;
  if ((game_state.unknown[0x23] & 2) != 0) { printf("by unimpl\n"); exit(1); }
  byte_551E = 0x80;
  if (dl > 0x80) { cpu.dx = 0xFF00; }
  else { dl = game_state.unknown[0x21]; dl--; cpu.dx = (cpu.dx & 0xFF00) | dl; }
}
static void get_map_tile_data(void)
{
  uint8_t al;
  check_map_boundary_x();
  check_map_boundary_y();
  cpu.dx = cpu.dx & 0xFF;
  cpu.ax = cpu.dx;
  cpu.ax = cpu.ax << 1;
  cpu.ax += cpu.dx;
  cpu.bx = cpu.bx & 0xFF;
  cpu.ax += data_5A04[cpu.bx + 1];
  word_551F = cpu.ax;
  cpu.di = cpu.ax;
  cpu.ax = data_5521[cpu.di];
  cpu.ax += data_5521[cpu.di + 1] << 8;
  word_11C6 = cpu.ax;
  al = data_5521[cpu.di + 2];
  word_11C8 = al;
  cpu.cf = 0;
  if ((byte_551E & 0x80) == 0x80) {
    cpu.ax = 0; byte_551E = 0; word_11C8 = 0; word_11C6 &= 0x3000; cpu.cf = 1;
  }
}
static void move_player_on_map(void)
{
  uint8_t al, bl, dl;
  push_word(cpu.dx); push_word(cpu.bx);
  get_map_tile_data();
  cpu.bx = pop_word(); cpu.dx = pop_word();
  word_11CC = word_11C8;
  cpu.ax = word_11C6;
  word_11CA = cpu.ax;
  if (game_state.unknown[3] != 0) {
    push_word(cpu.dx); push_word(cpu.bx);
    cpu.dx++;
    get_map_tile_data();
    cpu.bx = pop_word(); cpu.dx = pop_word();
    al = word_11C6; al = al & 0xF;
    cpu.ax = (cpu.ax & 0xFF00) | al;
    word_11CC = (al << 8) | (word_11CC & 0xFF);
    push_word(cpu.dx); push_word(cpu.bx);
    cpu.bx--;
    get_map_tile_data();
    cpu.bx = pop_word(); cpu.dx = pop_word();
    bl = word_11C6; bl &= 0xF0;
    bl |= ((word_11CC & 0xFF00) >> 8);
    cpu.bx = (cpu.bx & 0xFF00) | bl;
    dl = word_11CA; cpu.dx = (cpu.dx & 0xFF00) | dl;
    al = game_state.unknown[3]; cpu.ax = (cpu.ax & 0xFF00) | al;
    if (al > 2) {
      dl = cpu.dx & 0xFF; dl = dl << 4; bl = cpu.bx & 0xFF; bl = bl >> 4; dl = dl | bl;
      cpu.bx = (cpu.bx & 0xFF00) | bl; cpu.dx = (cpu.dx & 0xFF00) | dl;
      word_11CA = (word_11CA & 0xFF00) | dl;
    } else if (al == 2) {
      word_11CA = (word_11CA & 0xFF00) | bl;
    } else {
      bl = cpu.bx & 0xFF; bl = bl << 4; dl = cpu.dx & 0xFF; dl = dl >> 4; dl = dl | bl;
      cpu.bx = (cpu.bx & 0xFF00) | bl; cpu.dx = (cpu.dx & 0xFF00) | dl;
      word_11CA = (word_11CA & 0xFF00) | dl;
    }
  }
}

/* ================================================================== */
/* VERBATIM ports — level metadata + resource caching                  */
/* ================================================================== */
static void advance_data_ptr(void)
{
  cpu.di += cpu.bx;
  cpu.bx = 0;
}

/* engine.c:5037 */
static void cache_level_components(struct resource *res, int starting_index)
{
  uint8_t al;
  do {
    al = res->bytes[cpu.bx + cpu.di];
    cpu.ax = (cpu.ax & 0xFF00) | al;
    push_word(cpu.ax);
    al &= 0x7F;
    data_56E5[starting_index] = al;
    cpu.bx++;
    starting_index++;
    cpu.ax = pop_word();
  } while ((cpu.ax & 0xFF) < 0x80);
  cpu.di += cpu.bx;
  cpu.bx = 0;
}

/* engine.c:5061 — operates on the level resource `r`. */
static void read_level_metadata(struct resource *r)
{
  uint8_t al;
  data_5521 = r->bytes;
  cpu.di = 0;
  while (cpu.di < 4) {
    al = r->bytes[cpu.di];
    game_state.unknown[cpu.di + 0x21] = al;
    cpu.di++;
  }
  cpu.bx = 0;
  do {
    al = r->bytes[cpu.bx + cpu.di];
    data_5897[cpu.bx] = al;
    cpu.bx++;
  } while (al < 0x80);
  advance_data_ptr();
  cpu.si = 0;
  do {
    al = r->bytes[cpu.bx + cpu.di];
    cpu.ax = (cpu.ax & 0xFF00) | al;
    push_word(cpu.ax);
    al &= 0x7F;
    cpu.ax = (cpu.ax & 0xFF00) | al;
    data_56C6[cpu.si + 1] = al;
    cpu.bx++;
    al = r->bytes[cpu.bx + cpu.di];
    data_56C6[cpu.si + 0xf + 1] = al;
    cpu.bx++;
    cpu.si++;
    cpu.ax = pop_word();
  } while ((cpu.ax & 0xFF) < 0x80);
  cpu.si = 0; cache_level_components(r, 0);
  cpu.si = 4; cache_level_components(r, 4);
  cpu.si = 8; cache_level_components(r, 8);
  cpu.ax = r->bytes[cpu.bx + cpu.di];
  cpu.ax += (r->bytes[cpu.bx + cpu.di + 1]) << 8;
  word_5864 = cpu.ax;
  cpu.di += 2;
  al = game_state.unknown[0x22];
  cpu.ax = al;
  cpu.si = cpu.ax;
  al = game_state.unknown[0x21];
  al = al << 1;
  al += game_state.unknown[0x21];
  cpu.ax = al;
  do {
    data_5A04[cpu.si] = cpu.di;
    cpu.di += cpu.ax;
    cpu.si--;
  } while (cpu.si < 0x8000);
}

/* engine.c:5468 — but resource_load is our stub (no DATA1 read). */
static void cache_resources(void)
{
  uint8_t al, bl;
  struct resource *r;
  cpu.bx = 0xFFFF;
  do {
    cpu.bx++;
    push_word(cpu.bx);
    bl = data_5897[cpu.bx];
    cpu.bx = (cpu.bx & 0xFF00) | bl;
    cpu.bx &= 0x7F;
    cpu.bx += 0x6E;
    al = 1;
    r = resource_load(cpu.bx);
    (void)al;
    cpu.bx = pop_word();
    data_5897[cpu.bx + 0xf] = r->index;
    al = data_5897[cpu.bx];
  } while (al < 0x80);
  cpu.bx = 0xE;
  do {
    al = data_5897[cpu.bx + 0xf];
    if (al < 0x80) {
      r = resource_get_by_index(al);
      data_59E4[cpu.bx] = r;
    }
    cpu.bx--;
  } while (cpu.bx != 0xFFFF);
}

/* engine.c:5512 — STUBBED: record params instead of decoding pixels. */
static void draw_sprite_to_viewport_rec(const char *batch, int counter,
                                        short xpos, short ypos,
                                        uint16_t sprite_offset)
{
  record_draw(batch, counter, word_1051, (int)sprite_offset, xpos, ypos, byte_104E);
}

/* ================================================================== */
/* refresh_viewport — sampling + selection (engine.c:5618)             */
/* ================================================================== */
static void sample_fov_loop(void)
{
  uint8_t al, bl, dl;
  cpu.bx = game_state.unknown[3];
  cpu.si = data_5303[cpu.bx];
  cpu.di = 0xB;
  do {
    push_word(cpu.di); push_word(cpu.si);
    dl = game_state.unknown[1]; dl += data_530B[cpu.si];
    cpu.dx = (cpu.dx & 0xFF00) | dl;
    bl = game_state.unknown[0]; bl += data_530B[cpu.si + 1];
    cpu.bx = bl;
    move_player_on_map();
    cpu.si = pop_word(); cpu.di = pop_word();
    al = word_11CA; data_5A56[cpu.di] = al;
    al = (word_11CA & 0xFF00) >> 8; al &= 0xF7;
    cpu.ax = (cpu.ax & 0xFF00) | al;
    data_5A56[cpu.di + 0xC] = al;
    cpu.si--; cpu.si--; cpu.di--;
  } while (cpu.di != 0xFFFF);
}

/* engine.c:5533 — draw_viewport_sky, selection branch only. */
static void draw_viewport_sky(void)
{
  uint8_t al, bl;
  int tmp_carry;
  struct resource *r;
  bl = data_5A56[0x16];
  cpu.cf = 0;
  for (int i = 0; i < 3; i++) {
    tmp_carry = bl & 0x80 ? 1 : 0;
    bl = (bl << 1) + cpu.cf;
    cpu.cf = tmp_carry;
  }
  cpu.bx = (cpu.bx & 0xFF00) | bl;
  cpu.bx &= 3;
  bl = data_56E5[cpu.bx];
  cpu.bx = (cpu.bx & 0xFF00) | bl;
  al = data_5897[cpu.bx];
  al &= 0x7F;
  cpu.ax = (cpu.ax & 0xFF00) | al;
  if (al == 1) {
    r = data_59E4[cpu.bx];
    word_1051 = r;
    word_104F = 0;
    byte_104E = 0;
    cpu.bx = 4;
    draw_sprite_to_viewport_rec("SKY", -1, 0, 0, cpu.bx);
  }
  /* else: flat 2-colour fill (data_575C) — no component draw, nothing logged. */
  (void)data_575C;
}

static void refresh_viewport_select(void)
{
  uint8_t bl;
  struct resource *r;
  int counter;
  short xpos, ypos;

  /* (load_level_resources already done by caller; read_level_metadata done.) */
  sample_fov_loop();

  /* 0x51FC — mirror+map[0x26] step (no draw, but executed for fidelity). */
  bl = data_5A56[10];
  bl = bl >> 4;
  if (bl != 0) { cpu.bx = bl; bl = data_56C6[cpu.bx + 0xF]; cpu.bx = bl; }
  game_state.unknown[0x26] = bl;

  /* (game_state[0x23]&8) path: Purgatory flags 0x1C has bit3 set -> skip 37C8. */

  /* 0x5227 — set wall bit on current tile (no draw). */
  {
    uint8_t dl2 = game_state.unknown[1];
    cpu.dx = (cpu.dx & 0xFF00) | dl2;
    uint8_t bl2 = game_state.unknown[0];
    cpu.bx = (cpu.bx & 0xFF00) | bl2;
    get_map_tile_data();
    if (cpu.cf == 0) data_5521[word_551F + 1] |= 0x8;
  }

  cache_resources();
  draw_viewport_sky();

  /* ground 9 sprites, counter 8..0. */
  counter = 8;
  do {
    cpu.bx = data_56A3[counter];
    bl = data_5A56[cpu.bx + 0xC];
    bl = bl >> 4;
    cpu.bx = (cpu.bx & 0xFF00) | bl;
    cpu.bx &= 3;
    bl = data_56E5[cpu.bx + 4];
    cpu.bx = (cpu.bx & 0xFF00) | bl;
    r = data_59E4[cpu.bx];
    word_1051 = r;
    word_104F = 0;
    xpos = ground_points[counter].x;
    ypos = ground_points[counter].y;
    byte_104E = 0;
    cpu.bx = sprite_indices[counter];
    draw_sprite_to_viewport_rec("GND", counter, xpos, ypos, cpu.bx);
    counter--;
  } while (counter >= 0);

  /* other components, counter 23..0. */
  counter = 23;
  do {
    uint8_t al;
    cpu.ax = data_55EF[counter];
    cpu.di = cpu.ax;
    cpu.di &= 0x007F;
    bl = data_5A56[cpu.di];
    if ((cpu.ax & 0xFF) > 0x80) bl = bl >> 4;
    cpu.bx = (cpu.bx & 0xFF00) | bl;
    cpu.bx &= 0x000F;
    if (cpu.bx != 0) {
      cpu.cx = data_564F[counter];
      byte_104E = cpu.cx & 0xFF;
      al = data_56C6[cpu.bx];
      if (((cpu.cx & 0xFF) & 1) == 1) al = data_56E5[cpu.bx + 0x7];
      if (al <= 0x7F) {
        al = al << 1;
        cpu.ax = al;
        cpu.di = cpu.ax;
        r = data_59E4[cpu.di >> 1];
        if (verbose)
          printf("refresh_viewport: Drawing component %d (tag: %d)\n", counter, r->tag);
        word_1051 = r;
        word_104F = 0;
        xpos = data_558F[counter];
        ypos = data_55BF[counter];
        cpu.bx = data_561F[counter];
        draw_sprite_to_viewport_rec("OTH", counter, xpos, ypos, cpu.bx);
      }
    }
    counter--;
  } while (counter >= 0);
}

/* ================================================================== */
int main(int argc, char **argv)
{
  if (argc < 5) {
    fprintf(stderr, "Usage: %s <level.lvl> <facing> <x> <y> [out.golden] [-v]\n", argv[0]);
    return 2;
  }
  const char *lvl_path = argv[1];
  int facing = atoi(argv[2]);
  int px = atoi(argv[3]);
  int py = atoi(argv[4]);
  const char *out_path = (argc >= 6 && argv[5][0] != '-') ? argv[5] : NULL;
  for (int i = 1; i < argc; i++) if (!strcmp(argv[i], "-v")) verbose = 1;

  FILE *f = fopen(lvl_path, "rb");
  if (!f) { perror("fopen lvl"); return 1; }
  fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
  unsigned char *buf = malloc(sz);
  if (fread(buf, 1, sz, f) != (size_t)sz) { perror("fread"); return 1; }
  fclose(f);

  rm_init();
  /* level resource lands at allocations[2] (first dynamic slot). */
  level_res = &allocations[2];
  level_res->bytes = buf;
  level_res->len = sz;
  level_res->usage_type = 1;
  level_res->tag = 0x46 + 1; /* placeholder; not used by selection. */
  level_res->index = 2;

  game_state.unknown[0] = (unsigned char)px;
  game_state.unknown[1] = (unsigned char)py;
  game_state.unknown[3] = (unsigned char)facing;

  memset(data_5A56, 0, sizeof(data_5A56));
  memset(data_5897, 0, sizeof(data_5897));
  memset(data_56C6, 0, sizeof(data_56C6));
  memset(data_56E5, 0, sizeof(data_56E5));
  memset(data_59E4, 0, sizeof(data_59E4));
  byte_551E = 0;

  read_level_metadata(level_res);

  fprintf(stderr,
    "[golden_select] lvl=%s h=%u w=%u flags=0x%02X facing=%d x=%d y=%d\n",
    lvl_path, game_state.unknown[0x21], game_state.unknown[0x22],
    game_state.unknown[0x23], facing, px, py);

  refresh_viewport_select();

  /* ---- emit golden ---- */
  FILE *o = out_path ? fopen(out_path, "w") : stdout;
  if (!o) { perror("fopen out"); return 1; }

  fprintf(o, "# golden_select lvl=%s facing=%d x=%d y=%d\n", lvl_path, facing, px, py);
  fprintf(o, "# data_5A56[0..11]:");
  for (int i = 0; i < 12; i++) fprintf(o, " %02X", data_5A56[i]);
  fprintf(o, "\n# data_5A56[12..23]:");
  for (int i = 0; i < 12; i++) fprintf(o, " %02X", data_5A56[12 + i]);
  fprintf(o, "\n# data_5897[0..11]:");
  for (int i = 0; i < 12; i++) fprintf(o, " %02X", data_5897[i]);
  fprintf(o, "\n# data_56E5[0..11]:");
  for (int i = 0; i < 12; i++) fprintf(o, " %02X", data_56E5[i]);
  fprintf(o, "\n# data_56C6[0..15]:");
  for (int i = 0; i < 16; i++) fprintf(o, " %02X", data_56C6[i]);
  fprintf(o, "\n# data_59E4 tags[0..11]:");
  for (int i = 0; i < 12; i++) fprintf(o, " %d", data_59E4[i] ? data_59E4[i]->tag : -1);
  fprintf(o, "\n# ndraws=%d\n", ndraws);
  fprintf(o, "# idx batch counter tag offset xpos ypos b104e\n");
  for (int i = 0; i < ndraws; i++) {
    fprintf(o, "draw %2d %s c=%2d tag=%3d off=%2d x=%6d y=%4d b104e=0x%02X\n",
            i, draws[i].batch, draws[i].counter, draws[i].tag,
            draws[i].sprite_offset, (int)draws[i].xpos, (int)draws[i].ypos,
            draws[i].byte_104E);
  }

  if (out_path) { fclose(o); fprintf(stderr, "[golden_select] wrote %s\n", out_path); }
  free(buf);
  return 0;
}
