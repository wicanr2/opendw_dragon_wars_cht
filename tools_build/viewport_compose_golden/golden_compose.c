/*
 * golden_compose.c
 *
 * Self-contained golden oracle for Dragon Wars *viewport COMPOSE* — the FOV
 * tile-sampling + wall-nibble derivation stage of refresh_viewport.
 *
 * It reproduces, VERBATIM (modulo whitespace), these functions from Devin
 * Smith's opendw disassembly (src/lib/engine.c):
 *
 *   - check_map_boundary_x   (engine.c:5147)
 *   - check_map_boundary_y   (engine.c:5176)
 *   - get_map_tile_data      (engine.c:5206)  -> fills word_11C6 / word_11C8
 *   - move_player_on_map     (engine.c:5332)  -> nibble rotate into word_11CA
 *   - the FOV sampling loop of refresh_viewport (engine.c:5633..5661)
 *
 * Everything that refresh_viewport does AFTER the sampling loop (sky / ground /
 * other-component selection, sprite blit, UI) is stubbed away — this harness
 * only covers VIEWPORT_COMPOSE.md step 1 (the data layer).
 *
 * The cpu/global model mirrors opendw exactly:
 *   - cpu.ax/bx/dx/si/di are 16-bit "registers" the original code mutates.
 *   - data_5521  = the level resource bytes (== r->bytes after resource_load).
 *   - data_5A04  = per-row offset table, built like read_level_metadata's tail.
 *   - game_state.unknown[0..0x23] for x/y/facing + boundary limits.
 *
 * Level metadata (data_5521 / data_5A04 / dimensions / boundary globals) are
 * parsed from a real .lvl file using the SAME layout read_level_metadata uses
 * (4-byte header into [0x21..0x24], then we locate the tile grid offset and
 * fill data_5A04 the same way). sprite/UI/resource loading is NOT invoked.
 *
 * Output (golden, to stdout AND to a .golden file):
 *   - the 12 sampled (x,y) coordinates in di order 11..0
 *   - word_11C6 captured per slot (the raw current-tile wall word)
 *   - data_5A56[0..11]  (other/wall nibble source = word_11CA low byte)
 *   - data_5A56[12..23] (ground nibble source   = word_11CA high byte & 0xF7)
 *
 * Usage:
 *   golden_compose <level.lvl> <facing> <x> <y> [out.golden]
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* ------------------------------------------------------------------ */
/* cpu model (subset of opendw's cpu struct)                           */
/* ------------------------------------------------------------------ */
static struct { uint16_t ax, bx, dx, si, di; int cf; } cpu;

static uint16_t cpu_stack[64];
static int cpu_sp = 0;
static void push_word(uint16_t v) { cpu_stack[cpu_sp++] = v; }
static uint16_t pop_word(void) { return cpu_stack[--cpu_sp]; }

/* ------------------------------------------------------------------ */
/* game state                                                          */
/* ------------------------------------------------------------------ */
static struct { unsigned char unknown[256]; } game_state;

/* ------------------------------------------------------------------ */
/* level globals (mirror engine.c)                                     */
/* ------------------------------------------------------------------ */
static unsigned char *data_5521;   /* level resource bytes              */
static uint8_t  byte_551E;
static uint16_t word_551F;
static uint16_t word_11C6, word_11C8, word_11CA, word_11CC;
static uint16_t data_5A04[128];    /* per-row offsets                   */
static unsigned char data_5A56[128];

/* FOV step table (engine.c:191/197), copied verbatim.                 */
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

/* capture buffers for golden dump */
static int      cap_x[12], cap_y[12];     /* sampled (x,y) per di slot */
static uint16_t cap_11C6[12];             /* word_11C6 at primary tile */

static int verbose = 0;

/* ================================================================== */
/* VERBATIM ports from engine.c                                        */
/* ================================================================== */

/* 0x5523 — engine.c:5147 */
static void check_map_boundary_x(void)
{
  uint8_t bl;
  bl = cpu.bx & 0xFF;
  if (bl < game_state.unknown[0x22]) {
    return;
  }
  if ((game_state.unknown[0x23] & 0x2) != 0) {
    printf("check_map_boundary_x 0x5530 unimplemented,\n"); exit(1);
  }
  byte_551E = 0x80;
  if (bl < 0x80) {
    bl = game_state.unknown[0x22];
    bl--;
    cpu.bx = (cpu.bx & 0xFF00) | bl;
    return;
  }
  cpu.bx = cpu.bx & 0xFF00;
}

/* 0x5559 — engine.c:5176 */
static void check_map_boundary_y(int x)
{
  uint8_t dl;
  (void)x;
  dl = cpu.dx & 0xFF;
  if (dl < game_state.unknown[0x21]) {
    return;
  }
  if ((game_state.unknown[0x23] & 2) != 0) {
    printf("check_map_boundary_y 0x5566 unimplemented,\n"); exit(1);
  }
  byte_551E = 0x80;
  if (dl > 0x80) {
    dl = 0;
    cpu.dx = 0xFF00;
  } else {
    dl = game_state.unknown[0x21];
    dl--;
    cpu.dx = (cpu.dx & 0xFF00) | dl;
  }
}

/* 0x54D8 — engine.c:5206 */
static void get_map_tile_data(int x, int y)
{
  uint8_t al;
  (void)y;
  check_map_boundary_x();
  check_map_boundary_y(x);

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
  if (verbose)
    printf("get_map_tile_data - DI: 0x%04X AX: 0x%04X\n", cpu.di, cpu.ax);

  word_11C6 = cpu.ax;
  al = data_5521[cpu.di + 2];
  word_11C8 = al;
  cpu.cf = 0;
  if ((byte_551E & 0x80) == 0x80) {
    cpu.ax = 0;
    byte_551E = 0;
    word_11C8 = 0;
    word_11C6 &= 0x3000;
    cpu.cf = 1;
  }
}

/* 0x536B — engine.c:5332 */
static void move_player_on_map(int x, int y)
{
  uint8_t al, bl, dl;

  push_word(cpu.dx);
  push_word(cpu.bx);
  get_map_tile_data(x, y);
  cpu.bx = pop_word();
  cpu.dx = pop_word();

  word_11CC = word_11C8;
  cpu.ax = word_11C6;
  word_11CA = cpu.ax;

  if (game_state.unknown[3] != 0) {
    push_word(cpu.dx);
    push_word(cpu.bx);
    cpu.dx++;
    get_map_tile_data(cpu.dx, cpu.bx);
    cpu.bx = pop_word();
    cpu.dx = pop_word();

    al = word_11C6;
    al = al & 0xF;
    cpu.ax = (cpu.ax & 0xFF00) | al;
    word_11CC = (al << 8) | (word_11CC & 0xFF);

    push_word(cpu.dx);
    push_word(cpu.bx);
    cpu.bx--;
    get_map_tile_data(cpu.dx, cpu.bx);
    cpu.bx = pop_word();
    cpu.dx = pop_word();

    bl = word_11C6;
    bl &= 0xF0;
    bl |= ((word_11CC & 0xFF00) >> 8);
    cpu.bx = (cpu.bx & 0xFF00) | bl;
    dl = word_11CA;
    cpu.dx = (cpu.dx & 0xFF00) | dl;
    al = game_state.unknown[3];
    cpu.ax = (cpu.ax & 0xFF00) | al;
    if (al > 2) {
      dl = cpu.dx & 0xFF;
      dl = dl << 4;
      bl = cpu.bx & 0xFF;
      bl = bl >> 4;
      dl = dl | bl;
      cpu.bx = (cpu.bx & 0xFF00) | bl;
      cpu.dx = (cpu.dx & 0xFF00) | dl;
      word_11CA = (word_11CA & 0xFF00) | dl;
    } else if (al == 2) {
      word_11CA = (word_11CA & 0xFF00) | bl;
    } else {
      bl = cpu.bx & 0xFF;
      bl = bl << 4;
      dl = cpu.dx & 0xFF;
      dl = dl >> 4;
      dl = dl | bl;
      cpu.bx = (cpu.bx & 0xFF00) | bl;
      cpu.dx = (cpu.dx & 0xFF00) | dl;
      word_11CA = (word_11CA & 0xFF00) | dl;
    }
  }
}

/* refresh_viewport FOV sampling loop — engine.c:5632..5661, verbatim. */
static void sample_fov(void)
{
  uint8_t al, bl, dl;

  cpu.bx = game_state.unknown[3];

  cpu.si = data_5303[cpu.bx];
  cpu.di = 0xB;
  do {
    push_word(cpu.di);
    push_word(cpu.si);

    dl = game_state.unknown[1];
    dl += data_530B[cpu.si];
    cpu.dx = (cpu.dx & 0xFF00) | dl;
    bl = game_state.unknown[0];
    bl += data_530B[cpu.si + 1];
    cpu.bx = bl;

    /* capture the primary sampled (x,y) for golden (di order). */
    cap_x[cpu.di] = (int)(int8_t)bl;   /* note: stored as raw byte; record as col */
    cap_y[cpu.di] = (int)(int8_t)dl;

    move_player_on_map(dl, bl);
    /* word_11C6 here is the LAST tile read inside move_player_on_map.
     * The "primary" tile word is re-derivable; for the dump we capture
     * the rotated result word_11CA below and also expose word_11C6
     * of the primary tile via a separate read (see note in main). */
    cpu.si = pop_word();
    cpu.di = pop_word();
    if (verbose)
      printf("sample_fov 0x%04X 11CA: 0x%04X\n", cpu.di, word_11CA);
    al = word_11CA;
    data_5A56[cpu.di] = al;
    al = (word_11CA & 0xFF00) >> 8;
    al &= 0xF7;
    cpu.ax = (cpu.ax & 0xFF00) | al;
    data_5A56[cpu.di + 0xC] = al;
    cpu.si--;
    cpu.si--;
    cpu.di--;
  } while (cpu.di != 0xFFFF);
}

/* ================================================================== */
/* Level parse (mirror read_level_metadata layout, no resource load)   */
/* ================================================================== */

/* Locate the tile grid offset inside the .lvl and fill data_5A04 exactly
 * like read_level_metadata does (engine.c:5061..5142). We re-walk the
 * variable-length header sections the same way the C parser / remake does:
 *   [0..3]   header (h,w,flags,?)  -> game_state[0x21..0x24]
 *   then     data_5897 (until byte >= 0x80)
 *   then     data_56C6 pairs (until first-of-pair >= 0x80)
 *   then     3 component lists (each until byte >= 0x80)
 *   then     2-byte name offset
 *   then     tile grid (grid offset = di here)
 * data_5A04[si] for si=w..0 walks backwards by 3*h each step.
 */
static int parse_level(const unsigned char *buf, long sz)
{
  if (sz < 8) return -1;

  /* header -> game_state[0x21..0x24] (read_level_metadata loop di<4). */
  for (int i = 0; i < 4; i++)
    game_state.unknown[0x21 + i] = buf[i];

  uint8_t h = game_state.unknown[0x21];
  uint8_t w = game_state.unknown[0x22];

  /* walk header sections to find the grid offset (di). */
  long p = 4;
  /* data_5897: until byte >= 0x80 (inclusive). */
  while (p < sz && buf[p] < 0x80) p++;
  p++; /* include terminator */
  /* data_56C6 pairs: read pairs; loop ends when first-of-pair >= 0x80. */
  while (p + 1 < sz) { uint8_t f = buf[p]; p += 2; if (f >= 0x80) break; }
  /* 3 component lists, each until byte >= 0x80 (inclusive). */
  for (int k = 0; k < 3; k++) {
    while (p < sz && buf[p] < 0x80) p++;
    p++;
  }
  /* 2-byte name offset. */
  p += 2;

  long grid = p;
  if (grid >= sz) return -1;

  /* fill data_5A04 like read_level_metadata tail:
   *   si = w; di = grid; ax = 3*h; do { data_5A04[si]=di; di+=ax; si--; } while(si<0x8000)
   */
  {
    int si = w;
    uint16_t di = (uint16_t)grid;
    uint16_t ax = (uint16_t)(3 * h);
    do {
      data_5A04[si] = di;
      di += ax;
      si--;
    } while ((uint16_t)si < 0x8000);
  }
  return 0;
}

/* ================================================================== */
int main(int argc, char **argv)
{
  if (argc < 5) {
    fprintf(stderr,
      "Usage: %s <level.lvl> <facing> <x> <y> [out.golden] [-v]\n", argv[0]);
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

  data_5521 = buf;
  if (parse_level(buf, sz) != 0) {
    fprintf(stderr, "parse_level failed\n"); return 1;
  }

  game_state.unknown[0] = (unsigned char)px;       /* x */
  game_state.unknown[1] = (unsigned char)py;       /* y */
  game_state.unknown[3] = (unsigned char)facing;   /* facing */

  memset(data_5A56, 0, sizeof(data_5A56));
  byte_551E = 0;

  fprintf(stderr,
    "[golden_compose] lvl=%s h=%u w=%u flags=0x%02X facing=%d x=%d y=%d\n",
    lvl_path, game_state.unknown[0x21], game_state.unknown[0x22],
    game_state.unknown[0x23], facing, px, py);

  /* Also dump per-slot word_11C6 of the PRIMARY tile (the current-tile word
   * before rotation). We re-run get_map_tile_data on the primary (x,y) of
   * each slot with facing temporarily 0 so move_player_on_map's neighbour
   * reads don't clobber word_11C6 — this is a DIAGNOSTIC capture, not part
   * of the verbatim loop. The verbatim sample_fov() below produces the
   * authoritative data_5A56. */
  {
    int saved_facing = game_state.unknown[3];
    cpu.bx = saved_facing;
    int si = data_5303[saved_facing];
    for (int di = 0xB; di >= 0; di--) {
      uint8_t dl = game_state.unknown[1] + data_530B[si];
      uint8_t bl = game_state.unknown[0] + data_530B[si + 1];
      cap_x[di] = bl;
      cap_y[di] = dl;
      /* primary tile word_11C6 (facing forced 0 to avoid neighbour rotate). */
      uint8_t fsave = game_state.unknown[3];
      game_state.unknown[3] = 0;
      cpu.dx = (cpu.dx & 0xFF00) | dl;
      cpu.bx = bl;
      byte_551E = 0;
      get_map_tile_data(dl, bl);
      cap_11C6[di] = word_11C6;
      game_state.unknown[3] = fsave;
      si -= 2;
    }
  }

  /* authoritative sampling (verbatim refresh_viewport loop). */
  memset(data_5A56, 0, sizeof(data_5A56));
  byte_551E = 0;
  sample_fov();

  /* ---- emit golden ---- */
  FILE *o = out_path ? fopen(out_path, "w") : stdout;
  if (!o) { perror("fopen out"); return 1; }

  fprintf(o, "# golden_compose lvl=%s facing=%d x=%d y=%d\n",
          lvl_path, facing, px, py);
  fprintf(o, "# di : sample(x,y) word_11C6 : 5A56[di] 5A56[di+0xC]\n");
  for (int di = 11; di >= 0; di--) {
    fprintf(o, "di=%2d (x=%3d y=%3d) 11C6=0x%04X : wall=0x%02X ground=0x%02X\n",
            di, cap_x[di], cap_y[di], cap_11C6[di],
            data_5A56[di], data_5A56[di + 0xC]);
  }
  fprintf(o, "5A56[0..11] =");
  for (int i = 0; i < 12; i++) fprintf(o, " %02X", data_5A56[i]);
  fprintf(o, "\n5A56[12..23]=");
  for (int i = 12; i < 24; i++) fprintf(o, " %02X", data_5A56[i]);
  fprintf(o, "\n");

  if (out_path) {
    fclose(o);
    fprintf(stderr, "[golden_compose] wrote %s\n", out_path);
  }
  free(buf);
  return 0;
}
