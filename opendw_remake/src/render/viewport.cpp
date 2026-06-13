// viewport.cpp — 第一人稱 viewport 解碼器實作。
//
// 忠實 port 自 opendw src/lib/ui.c,藍本為
// tools_build/viewport_golden/golden_decode.c。
// 解碼函式逐行對照,變數語意/順序與 golden_decode.c 一致。
// 查表來自 viewport_tables.hpp;b152_table 不在該 header,故在此本檔
// 內以 verbatim 區域表提供 (draw_viewport_flip_y 用)。

#include "render/viewport.hpp"
#include "render/viewport_tables.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace dw::render {

DecodeBranchCounters& decode_branch_counters() {
  static DecodeBranchCounters counters;
  return counters;
}

namespace {

// 解碼期間的執行狀態,對應 golden_decode.c 的 file-global。
//   viewport_memory — 即 ViewportDecoder::mem.data()。
//   offsets[]       — make_offset_table(word_1053) 產生 (等價 init_offsets)。
//   word_1053       — 列 stride 基準。
//   word_1055       — 依 byte_104E & 0x40 取 ±word_1053。
//   byte_104E       — quadrant dispatch flag。
struct DecodeCtx {
  unsigned char* viewport_memory = nullptr;
  const std::uint16_t* offsets = nullptr;
  unsigned short word_1053 = 0;
  unsigned short word_1055 = 0;
  unsigned char byte_104E = 0;
};

// viewport_data struct (copied from opendw src/lib/ui.h)
struct viewport_data {
  std::uint16_t xpos;       // Sometimes set as 36C0
  int ypos;                 // Sometimes set as 36C4
  int runlength;            // Sometimes set as 1048
  int numruns;              // sometimes set as 104D
  int unknown1;
  int unknown2;
  const unsigned char* data;
};

// get_offset() — offsets[pos] (copied from src/lib/offsets.c)。
//
// 注意:當 ypos 為負 (例:data6820 dy-byte = 0xF8 → ypos = -8) 時,
// 原版 / golden_decode.c 會以負索引讀 offsets[-N]。在 opendw 的全域記憶體
// 佈局 (offsets[] 位於 dragon.com 0xB042,其前方的位元組為 0) 與
// golden_decode.c 的 g++ 連結佈局下,offsets[-8] 實測 == 0x0000。
// 為了與 golden byte-for-byte 一致,這裡對 pos < 0 一律回 0
// (等同 offsets[] 下方為零的記憶體)。pos 在 [0, kNumOffsets) 走正常查表。
inline std::uint16_t get_offset(const DecodeCtx& ctx, int pos) {
  if (pos < 0) return 0;
  return ctx.offsets[pos];
}

// ---------------------------------------------------------------------------
// b152_table[256] (0xB152-0xB251, copied verbatim from src/lib/tables.c)
// 不在 viewport_tables.hpp,僅 draw_viewport_flip_y 使用,故置於本檔。
inline constexpr unsigned char b152_table[256] = {
  0x00, 0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70,
  0x80, 0x90, 0xA0, 0xB0, 0xC0, 0xD0, 0xE0, 0xF0,
  0x01, 0x11, 0x21, 0x31, 0x41, 0x51, 0x61, 0x71,
  0x81, 0x91, 0xA1, 0xB1, 0xC1, 0xD1, 0xE1, 0xF1,
  0x02, 0x12, 0x22, 0x32, 0x42, 0x52, 0x62, 0x72,
  0x82, 0x92, 0xA2, 0xB2, 0xC2, 0xD2, 0xE2, 0xF2,
  0x03, 0x13, 0x23, 0x33, 0x43, 0x53, 0x63, 0x73,
  0x83, 0x93, 0xA3, 0xB3, 0xC3, 0xD3, 0xE3, 0xF3,
  0x04, 0x14, 0x24, 0x34, 0x44, 0x54, 0x64, 0x74,
  0x84, 0x94, 0xA4, 0xB4, 0xC4, 0xD4, 0xE4, 0xF4,
  0x05, 0x15, 0x25, 0x35, 0x45, 0x55, 0x65, 0x75,
  0x85, 0x95, 0xA5, 0xB5, 0xC5, 0xD5, 0xE5, 0xF5,
  0x06, 0x16, 0x26, 0x36, 0x46, 0x56, 0x66, 0x76,
  0x86, 0x96, 0xA6, 0xB6, 0xC6, 0xD6, 0xE6, 0xF6,
  0x07, 0x17, 0x27, 0x37, 0x47, 0x57, 0x67, 0x77,
  0x87, 0x97, 0xA7, 0xB7, 0xC7, 0xD7, 0xE7, 0xF7,
  0x08, 0x18, 0x28, 0x38, 0x48, 0x58, 0x68, 0x78,
  0x88, 0x98, 0xA8, 0xB8, 0xC8, 0xD8, 0xE8, 0xF8,
  0x09, 0x19, 0x29, 0x39, 0x49, 0x59, 0x69, 0x79,
  0x89, 0x99, 0xA9, 0xB9, 0xC9, 0xD9, 0xE9, 0xF9,
  0x0A, 0x1A, 0x2A, 0x3A, 0x4A, 0x5A, 0x6A, 0x7A,
  0x8A, 0x9A, 0xAA, 0xBA, 0xCA, 0xDA, 0xEA, 0xFA,
  0x0B, 0x1B, 0x2B, 0x3B, 0x4B, 0x5B, 0x6B, 0x7B,
  0x8B, 0x9B, 0xAB, 0xBB, 0xCB, 0xDB, 0xEB, 0xFB,
  0x0C, 0x1C, 0x2C, 0x3C, 0x4C, 0x5C, 0x6C, 0x7C,
  0x8C, 0x9C, 0xAC, 0xBC, 0xCC, 0xDC, 0xEC, 0xFC,
  0x0D, 0x1D, 0x2D, 0x3D, 0x4D, 0x5D, 0x6D, 0x7D,
  0x8D, 0x9D, 0xAD, 0xBD, 0xCD, 0xDD, 0xED, 0xFD,
  0x0E, 0x1E, 0x2E, 0x3E, 0x4E, 0x5E, 0x6E, 0x7E,
  0x8E, 0x9E, 0xAE, 0xBE, 0xCE, 0xDE, 0xEE, 0xFE,
  0x0F, 0x1F, 0x2F, 0x3F, 0x4F, 0x5F, 0x6F, 0x7F,
  0x8F, 0x9F, 0xAF, 0xBF, 0xCF, 0xDF, 0xEF, 0xFF,
};

// 查表 accessors (對應 src/lib/tables.c)。
inline std::uint8_t  get_b152_table(std::uint8_t off)     { return b152_table[off]; }
inline std::uint8_t  get_and_table(std::uint8_t off)      { return and_table[off]; }
inline std::uint8_t  get_or_table(std::uint8_t off)       { return or_table[off]; }
inline std::uint16_t get_and_table_B452(std::uint8_t off) { return and_table_B452[off]; }
inline std::uint16_t get_or_table_B652(std::uint8_t off)  { return or_table_B652[off]; }

// ================================================================== //
// 以下 5 個分派函式逐行對照 golden_decode.c (= opendw src/lib/ui.c)。   //
// printf 診斷輸出刻意省略 (不影響 viewport_memory 結果)。              //
// ================================================================== //

// D88
void process_quadrant(const DecodeCtx& ctx, const viewport_data* d, unsigned char* data) {
  int newx, sign, ax;
  int word_104A;
  std::uint16_t offset;

  // sar 36C0, 1
  sign = d->xpos & 0x8000;
  newx = d->xpos >> 1;
  newx |= sign;

  ax = d->runlength;
  word_104A = ax;
  ax += newx;
  ax -= ctx.word_1053;
  if (ax > 0) {
    word_104A -= ax;
    if (word_104A <= 0)
      return;
  }

  // DA8
  offset = get_offset(ctx, d->ypos);
  offset += newx;
  unsigned char* p = data + offset;
  const unsigned char* si = d->data + 4;
  for (int i = 0; i < d->numruns; i++) {
    for (int j = 0; j < word_104A; j++) {
      unsigned char val = *si;
      unsigned char dval = *p;

      dval = dval & get_and_table(val);
      dval = dval | get_or_table(val);

      *p = dval;
      p++;
      si++;
    }
    offset += ctx.word_1055;
    p = data + offset;
    si += (d->runlength - word_104A);
  }
}

// 0xDEB
void draw_viewport_word_mode(const DecodeCtx& ctx, const viewport_data* d, unsigned char* data) {
  std::uint16_t ax, old_ax, newx, cx;
  std::uint16_t dx = 0;
  std::uint8_t dl;
  int sign;
  int word_104A;
  std::uint8_t byte_104C;
  std::uint16_t offset;

  // sar 36C0, 1
  sign = d->xpos & 0x8000;
  newx = d->xpos >> 1;
  newx |= sign;

  dl = 0;

  ax = d->runlength;
  word_104A = ax;
  ax += newx;
  old_ax = ax;
  ax -= ctx.word_1053;
  if (ax <= old_ax) {
    // 0x0E06
    word_104A -= ax;
    if (word_104A >= 0) {
      dl--;
    } else {
      // 0xE6C
      return;
    }
  }
  // 0xE0F
  byte_104C = dl;

  offset = get_offset(ctx, d->ypos);
  offset += newx;

  ax = ax & 0xFF;

  // 0xE27
  cx = word_104A;
  // 0xE35
  unsigned char* p = data + offset;
  const unsigned char* si = d->data + 4;

  // 1048 = 13 ?
  for (int i = 0; i < d->numruns; i++) {
    for (int j = 0; j < cx; j++) {
      unsigned char val = *si;

      dx = p[0];
      dx += p[1] << 8;

      dx &= get_and_table_B452(val);
      dx |= get_or_table_B652(val);

      *p = dx & 0xFF;
      p++;
      *p = (dx & 0xFF00) >> 8;
      si++;
    }
    // 0xE4C
    *p = (dx & 0xFF);
    if (byte_104C < 0x80) {
      p++;
      *p = (dx & 0xFF00) >> 8;
    }
    // 0x3A + 0x13
    // offset += 1055
    offset += ctx.word_1055;
    p = data + offset;
    si += (d->runlength - word_104A);
  }
  (void)ax;
}

// 0xEC5
void draw_viewport_neg_x_alt(const DecodeCtx& ctx, const viewport_data* d, unsigned char* data) {
  std::uint16_t ax;
  int bx;
  int sign, word_104A;
  std::uint16_t offset, save;
  std::uint16_t dx;
  const unsigned char* ds = d->data + 4;
  const unsigned char* base;
  std::uint8_t al;

  ax = d->xpos;
  ax = -ax;
  // sar ax, 1
  sign = ax & 0x8000;
  ax = ax >> 1;
  ax |= sign;

  bx = d->runlength;
  bx -= ax;
  word_104A = bx;
  if (word_104A <= 0) {
    return;
  }

  // 0xEDB
  ax = ax & 0xFF;
  ds += ax; // add si, ax
  bx = d->ypos;
  offset = get_offset(ctx, d->ypos);
  offset--;

  // 0xEEE
  for (int i = 0; i < d->numruns; i++) {
    save = offset;
    base = ds;
    unsigned char* p = data + offset;
    al = *ds++;
    bx = al;
    dx = p[0];
    dx += p[1] << 8;
    dx &= get_and_table_B452(bx);
    dx |= get_or_table_B652(bx);
    p++;
    *p = (dx & 0xFF00) >> 8;

    // 0xF10
    for (int j = 0; j < (word_104A - 1); j++) {
      al = *ds++;
      bx = al;
      dx = p[0];
      dx += p[1] << 8;

      dx &= get_and_table_B452(bx);
      dx |= get_or_table_B652(bx);

      *p = dx & 0xFF;
      p++;
      *p = (dx & 0xFF00) >> 8;
    }
    offset = save;
    offset += ctx.word_1055;
    p = data + offset;
    base += d->runlength;
    ds = base;
  }
  (void)al;
}

// 0xE6D
void draw_viewport_neg_x(const DecodeCtx& ctx, const viewport_data* d, unsigned char* data) {
  std::uint16_t ax, cx, dx;
  std::uint8_t al;
  std::uint16_t offset;
  std::uint16_t word_104A;
  int bx, sign;
  int si = 4;

  ax = d->xpos;
  ax = -ax;
  al = ax & 0xFF;

  // sar al, 1
  sign = al & 0x80;
  al = al >> 1;
  al |= sign;
  ax = (ax & 0xFF00) | al;

  bx = d->runlength;
  bx -= ax;

  word_104A = bx;
  if (bx <= 0)
    return;

  // 0xE83
  ax = ax & 0xFF;

  si += ax;
  bx = d->ypos;
  dx = get_offset(ctx, bx);
  ax = ax & 0xFF;

  // 0xE95
  for (int i = 0; i < d->numruns; i++) {
    cx = word_104A;
    offset = dx;
    // bp = si
    unsigned char* p = data + offset;
    const unsigned char* ds = d->data + si;

    // 0xE9E
    for (int j = 0; j < cx; j++) {
      al = *ds++;
      bx = al;
      al = *p;
      al &= get_and_table(bx);
      al |= get_or_table(bx);
      *p = al;
      p++;
    }
    si += d->runlength;
    dx += ctx.word_1055;
  }
}

// 0xF40 (flip y)
void draw_viewport_flip_y(const DecodeCtx& ctx, const viewport_data* d, unsigned char* data) {
  int cf = 0;
  int ax, bx, dx, word_104A;
  int di;
  std::uint8_t al, bl, bh;
  const unsigned char* p = d->data + 4;
  const unsigned char* q;

  // SAR xpos, 1
  std::uint16_t new_x = d->xpos;
  if (new_x & 1) {
    cf = 1;
  }
  new_x = new_x >> 1;

  ax = d->runlength;
  word_104A = ax;
  ax += new_x;
  ax -= ctx.word_1053;

  if (ax > 0) {
    // F56
    bx = d->runlength;
    bx -= ax;
    word_104A = bx;
    if (word_104A <= 0) {
      return;
    }
  }
  // F64
  bx = d->ypos;
  dx = new_x;
  dx += get_offset(ctx, bx);

  p += d->runlength;
  p--;
  q = p;
  bh = 0;

  // F7D
  for (int i = 0; i < d->numruns; i++) {
    di = dx;
    for (int j = 0; j < word_104A; j++) {
      bl = *q--;

      bl = get_b152_table(bl);

      al = data[di];
      al &= get_and_table(bl);
      al |= get_or_table(bl);

      data[di] = al;
      di++;
    }
    dx += ctx.word_1055;
    p += d->runlength;
    q = p;
  }
  (void)cf;
  (void)bh;
  (void)ax;
}

// 0xCF8 - extract and process viewport data.
void decode_viewport_data(DecodeCtx& ctx, const unsigned char* data, viewport_data* vp) {
  int ax;
  std::uint8_t al;
  std::uint16_t bx;
  const unsigned char* ds = data;

  vp->runlength = *ds++;
  vp->numruns = *ds++;

  al = *ds++;
  ax = (std::int8_t)al;
  if (ctx.byte_104E & 0x80) {
    // neg ax;
    ax = -ax;
  }
  vp->xpos += ax;
  if (vp->runlength >= 0x80 && ctx.byte_104E >= 0x80) {
    vp->xpos--;
  }
  // 0xD2A
  vp->runlength &= 0x7F;
  al = *ds++;
  ax = (std::int8_t)al;

  if (ctx.byte_104E & 0x40) {
    ax = -ax;
  }
  vp->ypos += ax;

  bx = vp->xpos;
  bx &= 1;
  bx = bx << 1;
  if (vp->xpos >= 0x8000) {
    bx |= 4;
  }

  // 0xD52
  if (ctx.byte_104E & 0x80) {
    bx |= 8;
  }

  // 0xD5C
  ax = ctx.word_1053;
  if (ctx.byte_104E & 0x40) {
    ax = -ax;
  }
  // 0xD67
  ctx.word_1055 = ax;

  // 0xD78 offset
  switch (bx) {
  case 0:
    decode_branch_counters().process_quadrant++;
    process_quadrant(ctx, vp, ctx.viewport_memory);
    break;
  case 2:
    decode_branch_counters().word_mode++;
    draw_viewport_word_mode(ctx, vp, ctx.viewport_memory);
    break;
  case 4:
    decode_branch_counters().neg_x++;
    draw_viewport_neg_x(ctx, vp, ctx.viewport_memory);
    break;
  case 6:
    decode_branch_counters().neg_x_alt++;
    draw_viewport_neg_x_alt(ctx, vp, ctx.viewport_memory);
    break;
  case 8:
    decode_branch_counters().flip_y++;
    draw_viewport_flip_y(ctx, vp, ctx.viewport_memory);
    break;
  default:
    std::fprintf(stderr, "decode_viewport_data: An unhandled BX (0x%04X) was specified.\n", bx);
    std::exit(1);
    break;
  }
}

} // namespace

// ---------------------------------------------------------------------------

void ViewportDecoder::reset(std::uint8_t fill) {
  mem.fill(fill);
}

// port 自 opendw ui_update_viewport (ui.c:519)。
void ViewportDecoder::to_framebuffer(render::Framebuffer& fb, int ox, int oy) const {
  constexpr int rows = 0x88;  // viewport_height = 136
  constexpr int cols = 0x50;  // viewport_width  = 80 (byte/列)
  const std::uint8_t* src = mem.data();
  for (int y = 0; y < rows; ++y) {
    int fx = ox;
    for (int x = 0; x < cols; ++x) {
      std::uint8_t al = *src++;
      std::uint8_t hi = (al >> 4) & 0x0F;  // 左像素
      std::uint8_t lo = al & 0x0F;         // 右像素
      fb.put(fx++, oy + y, hi);
      fb.put(fx++, oy + y, lo);
    }
  }
}

// port 自 opendw update_viewport (ui.c:1081)。
void ViewportDecoder::compose_frame(const std::uint8_t* vp0,
                                    const std::uint8_t* vp1,
                                    const std::uint8_t* vp2,
                                    const std::uint8_t* vp3) {
  // 0xCAD:清左右邊界 nibble (136 列,di 步進 0x50)。
  std::uint16_t di = 0;
  for (int i = 0; i < 0x88; ++i) {
    mem[di] &= 0x0F;
    mem[di + 0x4F] &= 0xF0;
    di += 0x50;
  }

  // 0xCBF:vidx 3→0,各象限模板 decode 進同一個 mem。
  //   xpos/ypos 取自 viewport_metadata[vidx*4 + 2/3] (與 opendw 一致)。
  const std::uint8_t* tmpl[4] = {vp0, vp1, vp2, vp3};
  for (int vidx = 3; vidx >= 0; --vidx) {
    int idx = vidx << 2;
    int xpos = viewport_metadata[idx + 2];
    int ypos = viewport_metadata[idx + 3];
    decode(tmpl[vidx], xpos, ypos, /*word_1053=*/0x50, /*byte_104E=*/0);
  }
}

// port 自 opendw draw_viewport_sky 的 al!=1 分支 (engine.c:5576)。
void ViewportDecoder::fill_sky_flat() {
  static const std::uint16_t data_575C[4] = {0x4040, 0x0404, 0, 0};
  int dx = 88;
  int bx = 0;
  std::uint8_t* vp = mem.data();
  do {
    if (dx < 0x28) bx |= 2;
    std::uint16_t ax = data_575C[bx];
    for (int i = 0; i < 40; ++i) {
      *vp++ = (ax & 0xFF00) >> 8;
      *vp++ = ax & 0xFF;
    }
    bx = bx ^ 1;
    dx--;
  } while (dx >= 0);
}

// port 自 opendw draw_sprite_to_viewport (engine.c:5512)。
bool ViewportDecoder::draw_sprite(const std::uint8_t* comp, std::size_t comp_len,
                                  std::uint16_t& word_104F, int sprite_offset,
                                  int xpos, int ypos, std::uint8_t byte_104E) {
  std::size_t hdr = (std::size_t)word_104F + (std::size_t)sprite_offset;
  if (hdr + 1 >= comp_len) return false;  // 防越界 (原版假設資料合法)
  std::uint16_t size = comp[hdr] | (comp[hdr + 1] << 8);
  if (size == 0) return false;            // 空 slot:不畫,不動 word_104F
  word_104F = static_cast<std::uint16_t>(word_104F + size);
  const std::uint8_t* payload = comp + word_104F;
  decode(payload, xpos, ypos, /*word_1053=*/0x50, byte_104E);
  return true;
}

void ViewportDecoder::decode(const std::uint8_t* tmpl,
                             int xpos,
                             int ypos,
                             std::uint16_t word_1053,
                             std::uint8_t byte_104E) {
  // 等價 opendw init_offsets(word_1053):offsets[i] = i * word_1053。
  OffsetTable ot = make_offset_table(word_1053);

  DecodeCtx ctx;
  ctx.viewport_memory = mem.data();
  ctx.offsets = ot.offsets;
  ctx.word_1053 = ot.word_1053;
  ctx.word_1055 = 0;
  ctx.byte_104E = byte_104E;

  viewport_data vp;
  vp.xpos = static_cast<std::uint16_t>(xpos);
  vp.ypos = ypos;
  vp.runlength = 0;
  vp.numruns = 0;
  vp.unknown1 = 0;
  vp.unknown2 = 0;
  vp.data = tmpl;

  decode_viewport_data(ctx, tmpl, &vp);
}

} // namespace dw::render
