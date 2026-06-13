/*
 * golden_minimap.c
 *
 * Self-contained golden oracle for Dragon Wars *overhead minimap* (the `?`
 * map command). It is a sibling of viewport_compose_golden/golden_pixel.c:
 * it reuses the SAME validated decode core (decode_viewport_data + 5 dispatch
 * fns + tables, copied verbatim from golden_decode.c / golden_pixel.c) and the
 * SAME self-contained resource model (reads pre-extracted decompressed
 * component bytes from <components_dir>/<sec>.bin — no DATA1, no Huffman here).
 *
 * Instead of refresh_viewport (first-person), it runs the minimap composer:
 *
 *   process_minimap_commands (engine.c:3260)   — entry (op_6D / `?`)
 *     draw_minimap            (engine.c:3204)   — 8-pass scroll loop
 *       draw_minimap_row      (engine.c:2994)   — 9 cells, tile→sprite select
 *         calc_minimap_position (engine.c:2955)
 *         get_map_tile_data     (engine.c:5206)
 *         draw_minimap_segment  (engine.c:2979) — minimap_viewport per cell
 *         plot_minimap_resource (engine.c:3150) — ground/monster/item sprites
 *         draw_minimap_from_data6820 (engine.c:3184) — player marker
 *
 * NOTE on the scroll animation: the original calls set_viewport_size() +
 * ui_viewport_reset() (memmove) between the 8 passes to scroll the map onto
 * the framebuffer. ui_viewport_reset does `memmove(vp, vp+0xD80, 0xD80)`.
 * That animation is a pure framebuffer-presentation concern that does not
 * change the *composed* per-pass pixels; for a deterministic, leaf-level
 * pixel golden we dump the viewport_memory produced by the *first* full
 * draw_minimap_row sweep (byte_1964 == 0), which holds the 9×N cell grid
 * decoded at init_offsets(0x90). This is exactly the data
 * set_viewport_size/ui_update_viewport later blits to the framebuffer.
 *
 * The remake's minimap renderer reproduces this same first-pass
 * viewport_memory byte-for-byte.
 *
 * Usage:
 *   golden_minimap <level.lvl> <px> <py> <components_dir> [out.mmem] [-v]
 *
 *   px,py = player map position (game_state.unknown[0]=x, [1]=y), as used by
 *           process_minimap_commands (byte_1960=unknown[0], byte_1961=unknown[1]).
 *
 * Output (golden): minimap viewport_memory, MMEM_SZ raw bytes.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

static int verbose = 0;

/* ------------------------------------------------------------------ */
/* cpu model                                                           */
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

/* ================================================================== */
/* viewport_data + decode core (copied from golden_pixel.c, verbatim)  */
/* ================================================================== */
struct viewport_data {
  uint16_t xpos;
  int ypos;
  int runlength;
  int numruns;
  int unknown1;
  int unknown2;
  unsigned char *data;
};

/* minimap uses init_offsets(0x90): row stride 0x90 (144) bytes = 288 px.
 * draw cells at ypos=0x18; player/ground sprites ~24 rows tall. Allocate a
 * generous buffer (0x100 rows * 0x90) so all decode writes land in-bounds. */
#define MMEM_ROWS 0x100
#define MMEM_STRIDE 0x90
static const int minimap_mem_sz = MMEM_ROWS * MMEM_STRIDE;
static unsigned char *viewport_memory; /* 0x4F11 (here: minimap buffer) */
static unsigned short word_1053 = 0;
static unsigned short word_1055 = 0;
static unsigned char byte_104E = 0;

#define NUM_OFFSETS 0x100
static uint16_t offsets[NUM_OFFSETS];
static void init_offsets(unsigned short dx)
{
  int i; uint16_t val = 0;
  word_1053 = dx;
  for (i = 0; i < NUM_OFFSETS; i++) { offsets[i] = val; val += dx; }
}
static uint16_t get_offset(int pos) { if (pos < 0) return 0; return offsets[pos]; }

/* --- lookup tables (verbatim from golden_pixel.c) --- */
static unsigned char b152_table[256] = {
  0x00,0x10,0x20,0x30,0x40,0x50,0x60,0x70,0x80,0x90,0xA0,0xB0,0xC0,0xD0,0xE0,0xF0,
  0x01,0x11,0x21,0x31,0x41,0x51,0x61,0x71,0x81,0x91,0xA1,0xB1,0xC1,0xD1,0xE1,0xF1,
  0x02,0x12,0x22,0x32,0x42,0x52,0x62,0x72,0x82,0x92,0xA2,0xB2,0xC2,0xD2,0xE2,0xF2,
  0x03,0x13,0x23,0x33,0x43,0x53,0x63,0x73,0x83,0x93,0xA3,0xB3,0xC3,0xD3,0xE3,0xF3,
  0x04,0x14,0x24,0x34,0x44,0x54,0x64,0x74,0x84,0x94,0xA4,0xB4,0xC4,0xD4,0xE4,0xF4,
  0x05,0x15,0x25,0x35,0x45,0x55,0x65,0x75,0x85,0x95,0xA5,0xB5,0xC5,0xD5,0xE5,0xF5,
  0x06,0x16,0x26,0x36,0x46,0x56,0x66,0x76,0x86,0x96,0xA6,0xB6,0xC6,0xD6,0xE6,0xF6,
  0x07,0x17,0x27,0x37,0x47,0x57,0x67,0x77,0x87,0x97,0xA7,0xB7,0xC7,0xD7,0xE7,0xF7,
  0x08,0x18,0x28,0x38,0x48,0x58,0x68,0x78,0x88,0x98,0xA8,0xB8,0xC8,0xD8,0xE8,0xF8,
  0x09,0x19,0x29,0x39,0x49,0x59,0x69,0x79,0x89,0x99,0xA9,0xB9,0xC9,0xD9,0xE9,0xF9,
  0x0A,0x1A,0x2A,0x3A,0x4A,0x5A,0x6A,0x7A,0x8A,0x9A,0xAA,0xBA,0xCA,0xDA,0xEA,0xFA,
  0x0B,0x1B,0x2B,0x3B,0x4B,0x5B,0x6B,0x7B,0x8B,0x9B,0xAB,0xBB,0xCB,0xDB,0xEB,0xFB,
  0x0C,0x1C,0x2C,0x3C,0x4C,0x5C,0x6C,0x7C,0x8C,0x9C,0xAC,0xBC,0xCC,0xDC,0xEC,0xFC,
  0x0D,0x1D,0x2D,0x3D,0x4D,0x5D,0x6D,0x7D,0x8D,0x9D,0xAD,0xBD,0xCD,0xDD,0xED,0xFD,
  0x0E,0x1E,0x2E,0x3E,0x4E,0x5E,0x6E,0x7E,0x8E,0x9E,0xAE,0xBE,0xCE,0xDE,0xEE,0xFE,
  0x0F,0x1F,0x2F,0x3F,0x4F,0x5F,0x6F,0x7F,0x8F,0x9F,0xAF,0xBF,0xCF,0xDF,0xEF,0xFF,
};
static unsigned char and_table[256] = {
  0x00,0x00,0x00,0x00,0x00,0x00,0x0F,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x0F,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x0F,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x0F,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x0F,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x0F,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0xF0,0xF0,0xF0,0xF0,0xF0,0xF0,0xFF,0xF0,0xF0,0xF0,0xF0,0xF0,0xF0,0xF0,0xF0,0xF0,
  0x00,0x00,0x00,0x00,0x00,0x00,0x0F,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x0F,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x0F,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x0F,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x0F,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x0F,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x0F,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x0F,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x0F,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
};
static unsigned char or_table[256] = {
  0x00,0x01,0x02,0x03,0x04,0x05,0x00,0x07,0x08,0x09,0x0A,0x0B,0x0C,0x0D,0x0E,0x0F,
  0x10,0x11,0x12,0x13,0x14,0x15,0x10,0x17,0x18,0x19,0x1A,0x1B,0x1C,0x1D,0x1E,0x1F,
  0x20,0x21,0x22,0x23,0x24,0x25,0x20,0x27,0x28,0x29,0x2A,0x2B,0x2C,0x2D,0x2E,0x2F,
  0x30,0x31,0x32,0x33,0x34,0x35,0x30,0x37,0x38,0x39,0x3A,0x3B,0x3C,0x3D,0x3E,0x3F,
  0x40,0x41,0x42,0x43,0x44,0x45,0x40,0x47,0x48,0x49,0x4A,0x4B,0x4C,0x4D,0x4E,0x4F,
  0x50,0x51,0x52,0x53,0x54,0x55,0x50,0x57,0x58,0x59,0x5A,0x5B,0x5C,0x5D,0x5E,0x5F,
  0x00,0x01,0x02,0x03,0x04,0x05,0x00,0x07,0x08,0x09,0x0A,0x0B,0x0C,0x0D,0x0E,0x0F,
  0x70,0x71,0x72,0x73,0x74,0x75,0x50,0x77,0x78,0x79,0x7A,0x7B,0x7C,0x7D,0x7E,0x7F,
  0x80,0x81,0x82,0x83,0x84,0x85,0x80,0x87,0x88,0x89,0x8A,0x8B,0x8C,0x8D,0x8E,0x8F,
  0x90,0x91,0x92,0x93,0x94,0x95,0x90,0x97,0x98,0x99,0x9A,0x9B,0x9C,0x9D,0x9E,0x9F,
  0xA0,0xA1,0xA2,0xA3,0xA4,0xA5,0xA0,0xA7,0xA8,0xA9,0xAA,0xAB,0xAC,0xAD,0xAE,0xAF,
  0xB0,0xB1,0xB2,0xB3,0xB4,0xB5,0xB0,0xB7,0xB8,0xB9,0xBA,0xBB,0xBC,0xBD,0xBE,0xBF,
  0xC0,0xC1,0xC2,0xC3,0xC4,0xC5,0xC0,0xC7,0xC8,0xC9,0xCA,0xCB,0xCC,0xCD,0xCE,0xCF,
  0xD0,0xD1,0xD2,0xD3,0xD4,0xD5,0xD0,0xD7,0xD8,0xD9,0xDA,0xDB,0xDC,0xDD,0xDE,0xDF,
  0xE0,0xE1,0xE2,0xE3,0xE4,0xE5,0xE0,0xE7,0xE8,0xE9,0xEA,0xEB,0xEC,0xED,0xEE,0xEF,
  0xF0,0xF1,0xF2,0xF3,0xF4,0xF5,0xF0,0xF7,0xF8,0xF9,0xFA,0xFB,0xFC,0xFD,0xFE,0xFF,
};
static uint16_t and_table_B452[256] = {
  0x0FF0,0x0FF0,0x0FF0,0x0FF0,0x0FF0,0x0FF0,0xFFF0,0x0FF0,0x0FF0,0x0FF0,0x0FF0,0x0FF0,0x0FF0,0x0FF0,0x0FF0,0x0FF0,
  0x0FF0,0x0FF0,0x0FF0,0x0FF0,0x0FF0,0x0FF0,0xFFF0,0x0FF0,0x0FF0,0x0FF0,0x0FF0,0x0FF0,0x0FF0,0x0FF0,0x0FF0,0x0FF0,
  0x0FF0,0x0FF0,0x0FF0,0x0FF0,0x0FF0,0x0FF0,0xFFF0,0x0FF0,0x0FF0,0x0FF0,0x0FF0,0x0FF0,0x0FF0,0x0FF0,0x0FF0,0x0FF0,
  0x0FF0,0x0FF0,0x0FF0,0x0FF0,0x0FF0,0x0FF0,0xFFF0,0x0FF0,0x0FF0,0x0FF0,0x0FF0,0x0FF0,0x0FF0,0x0FF0,0x0FF0,0x0FF0,
  0x0FF0,0x0FF0,0x0FF0,0x0FF0,0x0FF0,0x0FF0,0xFFF0,0x0FF0,0x0FF0,0x0FF0,0x0FF0,0x0FF0,0x0FF0,0x0FF0,0x0FF0,0x0FF0,
  0x0FF0,0x0FF0,0x0FF0,0x0FF0,0x0FF0,0x0FF0,0xFFF0,0x0FF0,0x0FF0,0x0FF0,0x0FF0,0x0FF0,0x0FF0,0x0FF0,0x0FF0,0x0FF0,
  0x0FFF,0x0FFF,0x0FFF,0x0FFF,0x0FFF,0x0FFF,0xFFFF,0x0FFF,0x0FFF,0x0FFF,0x0FFF,0x0FFF,0x0FFF,0x0FFF,0x0FFF,0x0FFF,
  0x0FF0,0x0FF0,0x0FF0,0x0FF0,0x0FF0,0x0FF0,0xFFF0,0x0FF0,0x0FF0,0x0FF0,0x0FF0,0x0FF0,0x0FF0,0x0FF0,0x0FF0,0x0FF0,
  0x0FF0,0x0FF0,0x0FF0,0x0FF0,0x0FF0,0x0FF0,0xFFF0,0x0FF0,0x0FF0,0x0FF0,0x0FF0,0x0FF0,0x0FF0,0x0FF0,0x0FF0,0x0FF0,
  0x0FF0,0x0FF0,0x0FF0,0x0FF0,0x0FF0,0x0FF0,0xFFF0,0x0FF0,0x0FF0,0x0FF0,0x0FF0,0x0FF0,0x0FF0,0x0FF0,0x0FF0,0x0FF0,
  0x0FF0,0x0FF0,0x0FF0,0x0FF0,0x0FF0,0x0FF0,0xFFF0,0x0FF0,0x0FF0,0x0FF0,0x0FF0,0x0FF0,0x0FF0,0x0FF0,0x0FF0,0x0FF0,
  0x0FF0,0x0FF0,0x0FF0,0x0FF0,0x0FF0,0x0FF0,0xFFF0,0x0FF0,0x0FF0,0x0FF0,0x0FF0,0x0FF0,0x0FF0,0x0FF0,0x0FF0,0x0FF0,
  0x0FF0,0x0FF0,0x0FF0,0x0FF0,0x0FF0,0x0FF0,0xFFF0,0x0FF0,0x0FF0,0x0FF0,0x0FF0,0x0FF0,0x0FF0,0x0FF0,0x0FF0,0x0FF0,
  0x0FF0,0x0FF0,0x0FF0,0x0FF0,0x0FF0,0x0FF0,0xFFF0,0x0FF0,0x0FF0,0x0FF0,0x0FF0,0x0FF0,0x0FF0,0x0FF0,0x0FF0,0x0FF0,
  0x0FF0,0x0FF0,0x0FF0,0x0FF0,0x0FF0,0x0FF0,0xFFF0,0x0FF0,0x0FF0,0x0FF0,0x0FF0,0x0FF0,0x0FF0,0x0FF0,0x0FF0,0x0FF0,
  0x0FF0,0x0FF0,0x0FF0,0x0FF0,0x0FF0,0x0FF0,0xFFF0,0x0FF0,0x0FF0,0x0FF0,0x0FF0,0x0FF0,0x0FF0,0x0FF0,0x0FF0,0x0FF0,
};
static uint16_t or_table_B652[256] = {
  0x0000,0x1000,0x2000,0x3000,0x4000,0x5000,0x0000,0x7000,0x8000,0x9000,0xA000,0xB000,0xC000,0xD000,0xE000,0xF000,
  0x0001,0x1001,0x2001,0x3001,0x4001,0x5001,0x0001,0x7001,0x8001,0x9001,0xA001,0xB001,0xC001,0xD001,0xE001,0xF001,
  0x0002,0x1002,0x2002,0x3002,0x4002,0x5002,0x0002,0x7002,0x8002,0x9002,0xA002,0xB002,0xC002,0xD002,0xE002,0xF002,
  0x0003,0x1003,0x2003,0x3003,0x4003,0x5003,0x0003,0x7003,0x8003,0x9003,0xA003,0xB003,0xC003,0xD003,0xE003,0xF003,
  0x0004,0x1004,0x2004,0x3004,0x4004,0x5004,0x0004,0x7004,0x8004,0x9004,0xA004,0xB004,0xC004,0xD004,0xE004,0xF004,
  0x0005,0x1005,0x2005,0x3005,0x4005,0x5005,0x0005,0x7005,0x8005,0x9005,0xA005,0xB005,0xC005,0xD005,0xE005,0xF005,
  0x0000,0x1000,0x2000,0x3000,0x4000,0x5000,0x0000,0x7000,0x8000,0x9000,0xA000,0xB000,0xC000,0xD000,0xE000,0xF000,
  0x0007,0x1007,0x2007,0x3007,0x4007,0x5007,0x0007,0x7007,0x8007,0x9007,0xA007,0xB007,0xC007,0xD007,0xE007,0xF007,
  0x0008,0x1008,0x2008,0x3008,0x4008,0x5008,0x0008,0x7008,0x8008,0x9008,0xA008,0xB008,0xC008,0xD008,0xE008,0xF008,
  0x0009,0x1009,0x2009,0x3009,0x4009,0x5009,0x0009,0x7009,0x8009,0x9009,0xA009,0xB009,0xC009,0xD009,0xE009,0xF009,
  0x000A,0x100A,0x200A,0x300A,0x400A,0x500A,0x000A,0x700A,0x800A,0x900A,0xA00A,0xB00A,0xC00A,0xD00A,0xE00A,0xF00A,
  0x000B,0x100B,0x200B,0x300B,0x400B,0x500B,0x000B,0x700B,0x800B,0x900B,0xA00B,0xB00B,0xC00B,0xD00B,0xE00B,0xF00B,
  0x000C,0x100C,0x200C,0x300C,0x400C,0x500C,0x000C,0x700C,0x800C,0x900C,0xA00C,0xB00C,0xC00C,0xD00C,0xE00C,0xF00C,
  0x000D,0x100D,0x200D,0x300D,0x400D,0x500D,0x000D,0x700D,0x800D,0x900D,0xA00D,0xB00D,0xC00D,0xD00D,0xE00D,0xF00D,
  0x000E,0x100E,0x200E,0x300E,0x400E,0x500E,0x000E,0x700E,0x800E,0x900E,0xA00E,0xB00E,0xC00E,0xD00E,0xE00E,0xF00E,
  0x000F,0x100F,0x200F,0x300F,0x400F,0x500F,0x000F,0x700F,0x800F,0x900F,0xA00F,0xB00F,0xC00F,0xD00F,0xE00F,0xF00F,
};
static uint8_t  get_b152_table(uint8_t off)      { return b152_table[off]; }
static uint8_t  get_and_table(uint8_t off)       { return and_table[off]; }
static uint8_t  get_or_table(uint8_t off)        { return or_table[off]; }
static uint16_t get_and_table_B452(uint8_t off)  { return and_table_B452[off]; }
static uint16_t get_or_table_B652(uint8_t off)   { return or_table_B652[off]; }

/* --- 5 dispatch fns (verbatim from golden_pixel.c) --- */
static void process_quadrant(const struct viewport_data *d, unsigned char *data)
{
  int newx, sign, ax; int word_104A; uint16_t offset;
  sign = d->xpos & 0x8000; newx = d->xpos >> 1; newx |= sign;
  ax = d->runlength; word_104A = ax; ax += newx; ax -= word_1053;
  if (ax > 0) { word_104A -= ax; if (word_104A <= 0) return; }
  offset = get_offset(d->ypos); offset += newx;
  unsigned char *p = data + offset; unsigned char *si = d->data + 4;
  for (int i = 0; i < d->numruns; i++) {
    for (int j = 0; j < word_104A; j++) {
      unsigned char val = *si; unsigned char dval = *p;
      dval = dval & get_and_table(val); dval = dval | get_or_table(val);
      *p = dval; p++; si++;
    }
    offset += word_1055; p = data + offset; si += (d->runlength - word_104A);
  }
}
static void draw_viewport_word_mode(const struct viewport_data *d, unsigned char *data)
{
  uint16_t ax, old_ax, newx, cx; uint16_t dx = 0; uint8_t dl; int sign;
  int word_104A; uint8_t byte_104C; uint16_t offset;
  sign = d->xpos & 0x8000; newx = d->xpos >> 1; newx |= sign;
  dl = 0;
  ax = d->runlength; word_104A = ax; ax += newx; old_ax = ax; ax -= word_1053;
  if (ax <= old_ax) { word_104A -= ax; if (word_104A >= 0) { dl--; } else { return; } }
  byte_104C = dl;
  offset = get_offset(d->ypos); offset += newx; ax = ax & 0xFF;
  cx = word_104A;
  unsigned char *p = data + offset; unsigned char *si = d->data + 4;
  for (int i = 0; i < d->numruns; i++) {
    for (int j = 0; j < cx; j++) {
      unsigned char val = *si;
      dx = p[0]; dx += p[1] << 8;
      dx &= get_and_table_B452(val); dx |= get_or_table_B652(val);
      *p = dx & 0xFF; p++; *p = (dx & 0xFF00) >> 8; si++;
    }
    *p = (dx & 0xFF);
    if (byte_104C < 0x80) { p++; *p = (dx & 0xFF00) >> 8; }
    offset += word_1055; p = data + offset; si += (d->runlength - word_104A);
  }
  (void)ax;
}
static void draw_viewport_neg_x_alt(const struct viewport_data *d, unsigned char *data)
{
  uint16_t ax; int bx; int sign, word_104A; uint16_t offset, save; uint16_t dx;
  unsigned char *ds = d->data + 4; unsigned char *base; uint8_t al;
  ax = d->xpos; ax = -ax; sign = ax & 0x8000; ax = ax >> 1; ax |= sign;
  bx = d->runlength; bx -= ax; word_104A = bx; if (word_104A <= 0) return;
  ax = ax & 0xFF; ds += ax; bx = d->ypos;
  offset = get_offset(d->ypos); offset--;
  for (int i = 0; i < d->numruns; i++) {
    save = offset; base = ds; unsigned char *p = data + offset;
    al = *ds++; bx = al; dx = p[0]; dx += p[1] << 8;
    dx &= get_and_table_B452(bx); dx |= get_or_table_B652(bx);
    p++; *p = (dx & 0xFF00) >> 8;
    for (int j = 0; j < (word_104A - 1); j++) {
      al = *ds++; bx = al; dx = p[0]; dx += p[1] << 8;
      dx &= get_and_table_B452(bx); dx |= get_or_table_B652(bx);
      *p = dx & 0xFF; p++; *p = (dx & 0xFF00) >> 8;
    }
    offset = save; offset += word_1055; p = data + offset;
    base += d->runlength; ds = base;
  }
  (void)al;
}
static void draw_viewport_neg_x(const struct viewport_data *d, unsigned char *data)
{
  uint16_t ax, cx, dx; uint8_t al; uint16_t offset; uint16_t word_104A;
  int bx, sign; int si = 4;
  ax = d->xpos; ax = -ax; al = ax & 0xFF;
  sign = al & 0x80; al = al >> 1; al |= sign; ax = (ax & 0xFF00) | al;
  bx = d->runlength; bx -= ax; word_104A = bx; if (bx <= 0) return;
  ax = ax & 0xFF; si += ax; bx = d->ypos; dx = get_offset(bx); ax = ax & 0xFF;
  for (int i = 0; i < d->numruns; i++) {
    cx = word_104A; offset = dx;
    unsigned char *p = data + offset; unsigned char *ds = d->data + si;
    for (int j = 0; j < cx; j++) {
      al = *ds++; bx = al; al = *p;
      al &= get_and_table(bx); al |= get_or_table(bx);
      *p = al; p++;
    }
    si += d->runlength; dx += word_1055;
  }
}
static void draw_viewport_flip_y(const struct viewport_data *d, unsigned char *data)
{
  int cf = 0; int ax, bx, dx, word_104A; int di; uint8_t al, bl, bh;
  unsigned char *p = d->data + 4; unsigned char *q;
  uint16_t new_x = d->xpos; if (new_x & 1) cf = 1; new_x = new_x >> 1;
  ax = d->runlength; word_104A = ax; ax += new_x; ax -= word_1053;
  if (ax > 0) { bx = d->runlength; bx -= ax; word_104A = bx; if (word_104A <= 0) return; }
  bx = d->ypos; dx = new_x; dx += get_offset(bx);
  p += d->runlength; p--; q = p; bh = 0;
  for (int i = 0; i < d->numruns; i++) {
    di = dx;
    for (int j = 0; j < word_104A; j++) {
      bl = *q--; bl = get_b152_table(bl);
      al = data[di]; al &= get_and_table(bl); al |= get_or_table(bl);
      data[di] = al; di++;
    }
    dx += word_1055; p += d->runlength; q = p;
  }
  (void)cf; (void)bh; (void)ax;
}
static void decode_viewport_data(unsigned char *data, struct viewport_data *vp)
{
  int ax; uint8_t al; uint16_t bx; unsigned char *ds = data;
  vp->runlength = *ds++; vp->numruns = *ds++;
  al = *ds++; ax = (int8_t)al; if (byte_104E & 0x80) ax = -ax;
  vp->xpos += ax;
  if (vp->runlength >= 0x80 && byte_104E >= 0x80) vp->xpos--;
  vp->runlength &= 0x7F;
  al = *ds++; ax = (int8_t)al; if (byte_104E & 0x40) ax = -ax;
  vp->ypos += ax;
  bx = vp->xpos; bx &= 1; bx = bx << 1;
  if (vp->xpos >= 0x8000) bx |= 4;
  if (byte_104E & 0x80) bx |= 8;
  ax = word_1053; if (byte_104E & 0x40) ax = -ax; word_1055 = ax;
  switch (bx) {
  case 0: process_quadrant(vp, viewport_memory); break;
  case 2: draw_viewport_word_mode(vp, viewport_memory); break;
  case 4: draw_viewport_neg_x(vp, viewport_memory); break;
  case 6: draw_viewport_neg_x_alt(vp, viewport_memory); break;
  case 8: draw_viewport_flip_y(vp, viewport_memory); break;
  default:
    printf("%s: An unhandled BX (0x%04X) was specified.\n", __func__, bx);
    exit(1); break;
  }
}

/* ================================================================== */
/* resource model — same as golden_pixel.c                             */
/* ================================================================== */
struct resource { unsigned char *bytes; size_t len; int usage_type; int tag; int index; };
static struct resource allocations[128];
static const char *g_components_dir;

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
static unsigned char *read_component(int sec, size_t *out_len)
{
  char path[4096];
  snprintf(path, sizeof(path), "%s/%d.bin", g_components_dir, sec);
  FILE *f = fopen(path, "rb");
  if (!f) {
    fprintf(stderr, "[golden_minimap] missing component bin: %s\n", path);
    exit(1);
  }
  fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
  unsigned char *b = malloc(sz ? sz : 1);
  if (sz > 0 && fread(b, 1, sz, f) != (size_t)sz) { perror("fread comp"); exit(1); }
  fclose(f);
  *out_len = (size_t)sz;
  return b;
}
static struct resource *resource_load(int sec)
{
  int idx = find_index_by_tag(sec);
  if (idx != -1) return &allocations[idx];
  int i; for (i = 0; i < 128; i++) if (allocations[i].usage_type == 0) break;
  allocations[i].usage_type = 1;
  allocations[i].tag = sec;
  allocations[i].index = i;
  allocations[i].bytes = read_component(sec, &allocations[i].len);
  return &allocations[i];
}
static struct resource *resource_get_by_index(int index) { return &allocations[index]; }

/* ================================================================== */
/* level globals (mirror engine.c)                                     */
/* ================================================================== */
static unsigned char *data_5521;
static uint8_t  byte_551E;
static uint16_t word_551F;
static uint16_t word_11C6, word_11C8;
static uint16_t word_5864;
static uint16_t data_5A04[128];
static unsigned char data_5897[256];
static unsigned char data_56C6[128];
static unsigned char data_56E5[128];
static struct resource *data_59E4[128];
static struct resource *word_1051;
static uint16_t word_104F;
static struct resource *level_res;

/* minimap state (engine.c) */
static uint8_t byte_1960, byte_1961, byte_1962, byte_1964, byte_1966;
static uint8_t byte_1949;

/* minimap-specific tables (engine.c:77) */
static unsigned short data_19FE[] = {
  0x0000, 0x0018, 0x0000,
  0xFFF8, 0x0010, 0x0000,
  0xFFF8, 0x0010, 0x0002
};

/* minimap viewport templates (loaded from bundle) */
static unsigned char *minimap_viewport; /* com 0x695C */
static unsigned char *data_6820;        /* com 0x6820 */
static unsigned char *ui_get_minimap_viewport(void) { return minimap_viewport; }
static unsigned char *ui_get_data_6820(void) { return data_6820; }

/* ================================================================== */
/* VERBATIM ports — boundary / metadata / caching (from golden_pixel)  */
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
  check_map_boundary_x(); check_map_boundary_y();
  cpu.dx = cpu.dx & 0xFF; cpu.ax = cpu.dx; cpu.ax = cpu.ax << 1; cpu.ax += cpu.dx;
  cpu.bx = cpu.bx & 0xFF; cpu.ax += data_5A04[cpu.bx + 1]; word_551F = cpu.ax;
  cpu.di = cpu.ax;
  cpu.ax = data_5521[cpu.di]; cpu.ax += data_5521[cpu.di + 1] << 8;
  word_11C6 = cpu.ax; al = data_5521[cpu.di + 2]; word_11C8 = al; cpu.cf = 0;
  if ((byte_551E & 0x80) == 0x80) {
    cpu.ax = 0; byte_551E = 0; word_11C8 = 0; word_11C6 &= 0x3000; cpu.cf = 1;
  }
}
static void advance_data_ptr(void) { cpu.di += cpu.bx; cpu.bx = 0; }
static void cache_level_components(struct resource *res, int starting_index)
{
  uint8_t al;
  do {
    al = res->bytes[cpu.bx + cpu.di]; cpu.ax = (cpu.ax & 0xFF00) | al; push_word(cpu.ax);
    al &= 0x7F; data_56E5[starting_index] = al; cpu.bx++; starting_index++; cpu.ax = pop_word();
  } while ((cpu.ax & 0xFF) < 0x80);
  cpu.di += cpu.bx; cpu.bx = 0;
}
static void read_level_metadata(struct resource *r)
{
  uint8_t al;
  data_5521 = r->bytes; cpu.di = 0;
  while (cpu.di < 4) { al = r->bytes[cpu.di]; game_state.unknown[cpu.di + 0x21] = al; cpu.di++; }
  cpu.bx = 0;
  do { al = r->bytes[cpu.bx + cpu.di]; data_5897[cpu.bx] = al; cpu.bx++; } while (al < 0x80);
  advance_data_ptr();
  cpu.si = 0;
  do {
    al = r->bytes[cpu.bx + cpu.di]; cpu.ax = (cpu.ax & 0xFF00) | al; push_word(cpu.ax);
    al &= 0x7F; cpu.ax = (cpu.ax & 0xFF00) | al; data_56C6[cpu.si + 1] = al; cpu.bx++;
    al = r->bytes[cpu.bx + cpu.di]; data_56C6[cpu.si + 0xf + 1] = al; cpu.bx++; cpu.si++;
    cpu.ax = pop_word();
  } while ((cpu.ax & 0xFF) < 0x80);
  cpu.si = 0; cache_level_components(r, 0);
  cpu.si = 4; cache_level_components(r, 4);
  cpu.si = 8; cache_level_components(r, 8);
  cpu.ax = r->bytes[cpu.bx + cpu.di]; cpu.ax += (r->bytes[cpu.bx + cpu.di + 1]) << 8;
  word_5864 = cpu.ax; cpu.di += 2;
  al = game_state.unknown[0x22]; cpu.ax = al; cpu.si = cpu.ax;
  al = game_state.unknown[0x21]; al = al << 1; al += game_state.unknown[0x21]; cpu.ax = al;
  do { data_5A04[cpu.si] = cpu.di; cpu.di += cpu.ax; cpu.si--; } while (cpu.si < 0x8000);
}
static void cache_resources(void)
{
  uint8_t al, bl; struct resource *r;
  cpu.bx = 0xFFFF;
  do {
    cpu.bx++; push_word(cpu.bx);
    bl = data_5897[cpu.bx]; cpu.bx = (cpu.bx & 0xFF00) | bl; cpu.bx &= 0x7F; cpu.bx += 0x6E;
    r = resource_load(cpu.bx);
    cpu.bx = pop_word(); data_5897[cpu.bx + 0xf] = r->index; al = data_5897[cpu.bx];
  } while (al < 0x80);
  cpu.bx = 0xE;
  do {
    al = data_5897[cpu.bx + 0xf];
    if (al < 0x80) { r = resource_get_by_index(al); data_59E4[cpu.bx] = r; }
    cpu.bx--;
  } while (cpu.bx != 0xFFFF);
}

/* engine.c:5512 — load sprite payload + decode into viewport_memory. */
static void draw_sprite_to_viewport(struct viewport_data *vp, uint16_t sprite_offset)
{
  unsigned char *ds = word_1051->bytes + word_104F + sprite_offset;
  cpu.ax = *ds; ds++; cpu.ax += (*ds) << 8;
  if (verbose) printf("draw_sprite_to_viewport: BX:0x%04X AX:0x%04X (104F=%u tag=%d)\n",
                      sprite_offset, cpu.ax, word_104F, word_1051->tag);
  if (cpu.ax == 0) return;
  word_104F += cpu.ax;
  ds = word_1051->bytes + word_104F;
  vp->data = ds;
  decode_viewport_data(vp->data, vp);
}

/* ================================================================== */
/* VERBATIM ports — minimap (engine.c:2955..3257)                      */
/* ================================================================== */

/* engine.c:2955 */
static void calc_minimap_position(void)
{
  uint8_t bl, dl;
  bl = 3;
  bl -= byte_1964;
  bl += byte_1960;
  cpu.bx = (cpu.bx & 0xFF00) | bl;
  dl = byte_1961;
  dl -= 4;
  dl += byte_1962;
  cpu.dx = (cpu.dx & 0xFF00) | dl;
  if (verbose) printf("calc_minimap_position: 0x%02X:0x%02X\n", bl, dl);
}

/* engine.c:2979 */
static void draw_minimap_segment(int input)
{
  struct viewport_data vp;
  memset(&vp, 0, sizeof(vp));
  word_104F = cpu.bx;
  vp.xpos = input << 5;
  vp.ypos = 0x18;
  vp.data = ui_get_minimap_viewport();
  decode_viewport_data(ui_get_minimap_viewport(), &vp);
}

/* engine.c:3150 */
static void plot_minimap_resource(uint8_t val, uint16_t di)
{
  struct resource *r;
  struct viewport_data vp;
  memset(&vp, 0, sizeof(vp));
  if (val > 0x7F) return;
  cpu.bx = 0;
  word_104F = cpu.bx;
  r = data_59E4[val];
  word_1051 = r;
  cpu.ax = byte_1962;
  cpu.ax = cpu.ax << 5;
  if (di > 0xC) { printf("plot_minimap_resource: di OOB 0x%04X\n", di); exit(1); }
  di = di >> 1;
  cpu.ax += data_19FE[di];
  vp.xpos = cpu.ax;
  cpu.ax = data_19FE[di + 1];
  vp.ypos = cpu.ax;
  cpu.bx = data_19FE[di + 2];
  draw_sprite_to_viewport(&vp, cpu.bx);
}

/* engine.c:3184 */
static void draw_minimap_from_data6820(void)
{
  struct viewport_data vp;
  memset(&vp, 0, sizeof(vp));
  if (verbose) printf("draw_minimap_from_data6820\n");
  cpu.bx = 0x6820;
  word_104F = cpu.bx;
  cpu.ax = byte_1962;
  cpu.ax = cpu.ax << 5;
  vp.xpos = cpu.ax;
  vp.ypos = 0x18;
  vp.data = ui_get_data_6820();
  decode_viewport_data(ui_get_data_6820(), &vp);
}

/* engine.c:2994 */
static void draw_minimap_row(uint8_t input)
{
  uint8_t al, bl, dl;
  calc_minimap_position();
  al = 0;
  bl = cpu.bx & 0xFF;
  dl = cpu.dx & 0xFF;
  if (bl == game_state.unknown[0] && dl == game_state.unknown[1]) {
    al = 0xFF;
  }
  byte_1966 = al;
  /* calc_minimap_position already left cpu.bx = x-coord, cpu.dx = y-coord;
   * get_map_tile_data / check_map_boundary_{x,y} operate directly on cpu
   * (the (x,y) args in opendw's C stub are decorative). No register swap. */
  (void)bl; (void)dl;
  get_map_tile_data();
  if (verbose) printf("  cell col=%u 11C6=0x%04X wallbit=%d\n",
                      byte_1962, word_11C6, ((word_11C6>>8)&0x08)!=0);

  if ((((word_11C6 & 0xFF00) >> 8) & 0x08) != 0) {
    bl = (word_11C6 & 0xFF00) >> 8;
    bl = bl >> 4;
    bl = bl & 0x3;
    al = data_56E5[bl + 4];
    cpu.di = 0;
    plot_minimap_resource(al, 0);
    bl = word_11C6;
    bl = bl >> 4;
    bl = bl & 0xF;
    if (bl != 0) {
      al = data_56C6[bl];
      cpu.di = 6;
      plot_minimap_resource(al, 6);
    }
    bl = word_11C6;
    bl = bl & 0xF;
    if (bl != 0) {
      al = data_56C6[bl];
      cpu.di = 0xC;
      plot_minimap_resource(al, 0xC);
    }
    if ((byte_1966 & 0x80) != 0) {
      draw_minimap_from_data6820();
      return;
    }
    bl = (word_11C6 & 0xFF00) >> 8;
    bl &= 0x7;
    if (bl == 0) return;
    al = data_56E5[bl + 7];
    cpu.di = 0;
    plot_minimap_resource(al, 0);
    return;
  }
  /* 18E4 */
  cpu.bx = 0x695C;
  draw_minimap_segment(input);

  al = word_11C6;
  byte_1949 = al;

  calc_minimap_position();          /* cpu.bx = x, cpu.dx = y */
  bl = cpu.bx & 0xFF;
  bl++;                             /* x + 1 */
  cpu.bx = (cpu.bx & 0xFF00) | bl;
  get_map_tile_data();

  if ((((word_11C6 & 0xFF00) >> 8) & 0x08) != 0) {
    bl = byte_1949;
    bl = bl >> 4;
    bl &= 0xF;
    if (bl != 0) {
      al = data_56C6[bl];
      cpu.di = 0x6;
      plot_minimap_resource(al, 6);
    }
  }

  calc_minimap_position();          /* cpu.bx = x, cpu.dx = y */
  dl = cpu.dx & 0xFF;
  dl--;                             /* y - 1 */
  cpu.dx = (cpu.dx & 0xFF00) | dl;
  get_map_tile_data();

  if ((((word_11C6 & 0xFF00) >> 8) & 0x08) != 0) {
    bl = byte_1949;
    bl &= 0xF;
    if (bl != 0) {
      al = data_56C6[bl];
      cpu.di = 0x0C;
      plot_minimap_resource(al, 0xC);
    }
    if ((byte_1966 & 0x80) == 0) return;
    draw_minimap_from_data6820();
    return;
  }
  if ((byte_1966 & 0x80) == 0) return;
  draw_minimap_from_data6820();
}

/* engine.c:3204 — first full sweep only (byte_1964 == 0). */
static void draw_minimap_first_pass(void)
{
  uint8_t al;
  byte_1964 = 0;
  byte_104E = 0;
  al = 0;
  do {
    byte_1962 = al;
    draw_minimap_row(al);
    al = byte_1962;
    al++;
  } while (al < 9);
}

/* ================================================================== */
static unsigned char *read_file(const char *path, size_t *out_len)
{
  FILE *f = fopen(path, "rb");
  if (!f) { fprintf(stderr, "fopen %s: ", path); perror(""); exit(1); }
  fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
  unsigned char *b = malloc(sz ? sz : 1);
  if (sz > 0 && fread(b, 1, sz, f) != (size_t)sz) { perror("fread"); exit(1); }
  fclose(f);
  if (out_len) *out_len = (size_t)sz;
  return b;
}

int main(int argc, char **argv)
{
  if (argc < 5) {
    fprintf(stderr,
      "Usage: %s <level.lvl> <px> <py> <viewport_dir> [components_dir] [out.mmem] [-v]\n"
      "  viewport_dir must contain minimap.bin and data6820.bin.\n"
      "  components_dir defaults to <viewport_dir>/../components.\n",
      argv[0]);
    return 2;
  }
  const char *lvl_path = argv[1];
  int px = atoi(argv[2]);
  int py = atoi(argv[3]);
  const char *vp_dir = argv[4];
  static char comp_buf[4096];
  const char *comp_dir = NULL;
  const char *out_path = NULL;
  for (int i = 5; i < argc; i++) {
    if (!strcmp(argv[i], "-v")) { verbose = 1; continue; }
    if (!comp_dir) { comp_dir = argv[i]; continue; }
    if (!out_path) { out_path = argv[i]; continue; }
  }
  if (!comp_dir) {
    snprintf(comp_buf, sizeof(comp_buf), "%s/../components", vp_dir);
    comp_dir = comp_buf;
  }
  g_components_dir = comp_dir;

  /* minimap templates */
  char mm_path[4096], d6_path[4096];
  snprintf(mm_path, sizeof(mm_path), "%s/minimap.bin", vp_dir);
  snprintf(d6_path, sizeof(d6_path), "%s/data6820.bin", vp_dir);
  minimap_viewport = read_file(mm_path, NULL);
  data_6820 = read_file(d6_path, NULL);

  size_t sz;
  unsigned char *buf = read_file(lvl_path, &sz);

  rm_init();
  level_res = &allocations[2];
  level_res->bytes = buf; level_res->len = sz; level_res->usage_type = 1;
  level_res->tag = 0x46 + 1; level_res->index = 2;

  game_state.unknown[0] = (unsigned char)px;
  game_state.unknown[1] = (unsigned char)py;
  game_state.unknown[3] = 0;

  memset(data_5897, 0, sizeof(data_5897));
  memset(data_56C6, 0, sizeof(data_56C6));
  memset(data_56E5, 0, sizeof(data_56E5));
  memset(data_59E4, 0, sizeof(data_59E4));
  byte_551E = 0;

  read_level_metadata(level_res);

  /* Explored-tile seeding (fog of war).
   * refresh_viewport sets data_5521[word_551F+1] |= 0x8 for the player's tile
   * each refresh, so the minimap reveals only visited tiles. For a
   * deterministic golden we seed the "seen" bit. Default: mark ALL tiles
   * explored (== fully-walked map), which exercises every minimap branch.
   * Set env MM_SEED=player to instead seed only the player's current tile. */
  {
    const char *seed = getenv("MM_SEED");
    int player_only = (seed && !strcmp(seed, "player"));
    if (player_only) {
      /* one tile, like a single refresh_viewport on entry */
      cpu.bx = game_state.unknown[0];
      cpu.dx = game_state.unknown[1];
      byte_551E = 0;
      get_map_tile_data();
      if (cpu.cf == 0) data_5521[word_551F + 1] |= 0x8;
    } else {
      uint8_t W = game_state.unknown[0x22], H = game_state.unknown[0x21];
      for (uint8_t yy = 0; yy < H; yy++) {
        for (uint8_t xx = 0; xx < W; xx++) {
          cpu.bx = xx; cpu.dx = yy; byte_551E = 0;
          get_map_tile_data();
          if (cpu.cf == 0) data_5521[word_551F + 1] |= 0x8;
        }
      }
    }
    byte_551E = 0;
  }

  fprintf(stderr,
    "[golden_minimap] lvl=%s h=%u w=%u flags=0x%02X px=%d py=%d comp=%s\n",
    lvl_path, game_state.unknown[0x21], game_state.unknown[0x22],
    game_state.unknown[0x23], px, py, g_components_dir);

  viewport_memory = malloc(minimap_mem_sz);
  memset(viewport_memory, 0, minimap_mem_sz);

  /* process_minimap_commands prologue: byte_1960=unknown[0], byte_1961=unknown[1],
   * init_offsets(0x90), cache_resources(), then the first draw_minimap sweep. */
  byte_1960 = game_state.unknown[0];
  byte_1961 = game_state.unknown[1];
  init_offsets(0x90);
  cache_resources();

  draw_minimap_first_pass();

  FILE *o = out_path ? fopen(out_path, "wb") : stdout;
  if (!o) { perror("fopen out"); return 1; }
  if (fwrite(viewport_memory, 1, minimap_mem_sz, o) != (size_t)minimap_mem_sz) {
    perror("fwrite"); return 1;
  }
  if (out_path) { fclose(o); fprintf(stderr, "[golden_minimap] wrote %s (%d bytes)\n", out_path, minimap_mem_sz); }

  free(buf); free(viewport_memory);
  return 0;
}
