#include "render/minimap.hpp"

#include <cstdio>
#include <cstring>

#include "render/viewport_tables.hpp"

namespace dw::render {
namespace {

// minimap-specific 落點表 (engine.c:77)。
//   每 di (0/6/0xC → >>1 → 0/3/6) 對應 3 個 word:xpos偏移、ypos、sprite_offset。
const unsigned short data_19FE[] = {
    0x0000, 0x0018, 0x0000,  // di=0  地形/玩家足下:x=0,   y=24, off=0
    0xFFF8, 0x0010, 0x0000,  // di=6  怪物/NPC:    x=-8,  y=16, off=0
    0xFFF8, 0x0010, 0x0002,  // di=C  物品/事件:    x=-8,  y=16, off=2
};

// ---------------------------------------------------------------------------
// decode 核心 (0x90 stride 版)。逐行對照 golden_minimap.c / viewport.cpp,
// 但 viewport_memory / offsets 由本類別持有 (大 buffer + stride 0x90)。
struct viewport_data {
  std::uint16_t xpos;
  int ypos;
  int runlength;
  int numruns;
  const unsigned char* data;
};

struct DecodeCtx {
  unsigned char* viewport_memory = nullptr;
  std::uint16_t offsets[0x100]{};  // stride*i;0x90 stride 需 >0x88 上限
  std::uint16_t word_1053 = 0;
  std::uint16_t word_1055 = 0;
  std::uint8_t byte_104E = 0;

  void init_offsets(std::uint16_t dx) {
    word_1053 = dx;
    std::uint16_t v = 0;
    for (int i = 0; i < 0x100; ++i) {
      offsets[i] = v;
      v = static_cast<std::uint16_t>(v + dx);
    }
  }
  std::uint16_t get_offset(int pos) const { return pos < 0 ? 0 : offsets[pos]; }
};

inline std::uint8_t get_b152(std::uint8_t o) {
  // b152_table 不在 viewport_tables.hpp;以與 tables.c 等價的公式提供:
  //   b152[i] = (i>>4) | ((i&0xF)<<4) ... 但實際是 nibble swap 後? 不,
  //   tables.c b152_table[off] = (off<<4 | off>>4) 的轉置表。改用直接公式:
  //   值 = ((o & 0x0F) << 4) | ((o & 0xF0) >> 4)。
  return static_cast<std::uint8_t>(((o & 0x0F) << 4) | ((o & 0xF0) >> 4));
}

void process_quadrant(const DecodeCtx& ctx, const viewport_data* d, unsigned char* data) {
  int newx, sign, ax, word_104A;
  std::uint16_t offset;
  sign = d->xpos & 0x8000; newx = d->xpos >> 1; newx |= sign;
  ax = d->runlength; word_104A = ax; ax += newx; ax -= ctx.word_1053;
  if (ax > 0) { word_104A -= ax; if (word_104A <= 0) return; }
  offset = ctx.get_offset(d->ypos); offset += newx;
  unsigned char* p = data + offset; const unsigned char* si = d->data + 4;
  for (int i = 0; i < d->numruns; i++) {
    for (int j = 0; j < word_104A; j++) {
      unsigned char val = *si, dval = *p;
      dval = dval & and_table[val]; dval = dval | or_table[val];
      *p = dval; p++; si++;
    }
    offset += ctx.word_1055; p = data + offset; si += (d->runlength - word_104A);
  }
}
void draw_viewport_word_mode(const DecodeCtx& ctx, const viewport_data* d, unsigned char* data) {
  std::uint16_t ax, old_ax, newx, cx; std::uint16_t dx = 0; std::uint8_t dl; int sign;
  int word_104A; std::uint8_t byte_104C; std::uint16_t offset;
  sign = d->xpos & 0x8000; newx = d->xpos >> 1; newx |= sign;
  dl = 0; ax = d->runlength; word_104A = ax; ax += newx; old_ax = ax; ax -= ctx.word_1053;
  if (ax <= old_ax) { word_104A -= ax; if (word_104A >= 0) { dl--; } else { return; } }
  byte_104C = dl; offset = ctx.get_offset(d->ypos); offset += newx; ax = ax & 0xFF; cx = word_104A;
  unsigned char* p = data + offset; const unsigned char* si = d->data + 4;
  for (int i = 0; i < d->numruns; i++) {
    for (int j = 0; j < cx; j++) {
      unsigned char val = *si; dx = p[0]; dx += p[1] << 8;
      dx &= and_table_B452[val]; dx |= or_table_B652[val];
      *p = dx & 0xFF; p++; *p = (dx & 0xFF00) >> 8; si++;
    }
    *p = (dx & 0xFF);
    if (byte_104C < 0x80) { p++; *p = (dx & 0xFF00) >> 8; }
    offset += ctx.word_1055; p = data + offset; si += (d->runlength - word_104A);
  }
  (void)ax;
}
void draw_viewport_neg_x_alt(const DecodeCtx& ctx, const viewport_data* d, unsigned char* data) {
  std::uint16_t ax; int bx; int sign, word_104A; std::uint16_t offset, save; std::uint16_t dx;
  const unsigned char* ds = d->data + 4; const unsigned char* base; std::uint8_t al;
  ax = d->xpos; ax = -ax; sign = ax & 0x8000; ax = ax >> 1; ax |= sign;
  bx = d->runlength; bx -= ax; word_104A = bx; if (word_104A <= 0) return;
  ax = ax & 0xFF; ds += ax; bx = d->ypos; offset = ctx.get_offset(d->ypos); offset--;
  for (int i = 0; i < d->numruns; i++) {
    save = offset; base = ds; unsigned char* p = data + offset;
    al = *ds++; bx = al; dx = p[0]; dx += p[1] << 8;
    dx &= and_table_B452[bx]; dx |= or_table_B652[bx];
    p++; *p = (dx & 0xFF00) >> 8;
    for (int j = 0; j < (word_104A - 1); j++) {
      al = *ds++; bx = al; dx = p[0]; dx += p[1] << 8;
      dx &= and_table_B452[bx]; dx |= or_table_B652[bx];
      *p = dx & 0xFF; p++; *p = (dx & 0xFF00) >> 8;
    }
    offset = save; offset += ctx.word_1055; p = data + offset; base += d->runlength; ds = base;
  }
  (void)al;
}
void draw_viewport_neg_x(const DecodeCtx& ctx, const viewport_data* d, unsigned char* data) {
  std::uint16_t ax, cx, dx; std::uint8_t al; std::uint16_t offset; std::uint16_t word_104A;
  int bx, sign; int si = 4;
  ax = d->xpos; ax = -ax; al = ax & 0xFF; sign = al & 0x80; al = al >> 1; al |= sign; ax = (ax & 0xFF00) | al;
  bx = d->runlength; bx -= ax; word_104A = bx; if (bx <= 0) return;
  ax = ax & 0xFF; si += ax; bx = d->ypos; dx = ctx.get_offset(bx); ax = ax & 0xFF;
  for (int i = 0; i < d->numruns; i++) {
    cx = word_104A; offset = dx;
    unsigned char* p = data + offset; const unsigned char* ds = d->data + si;
    for (int j = 0; j < cx; j++) {
      al = *ds++; bx = al; al = *p; al &= and_table[bx]; al |= or_table[bx]; *p = al; p++;
    }
    si += d->runlength; dx += ctx.word_1055;
  }
}
void draw_viewport_flip_y(const DecodeCtx& ctx, const viewport_data* d, unsigned char* data) {
  int cf = 0; int ax, bx, dx, word_104A; int di; std::uint8_t al, bl, bh;
  const unsigned char* p = d->data + 4; const unsigned char* q;
  std::uint16_t new_x = d->xpos; if (new_x & 1) cf = 1; new_x = new_x >> 1;
  ax = d->runlength; word_104A = ax; ax += new_x; ax -= ctx.word_1053;
  if (ax > 0) { bx = d->runlength; bx -= ax; word_104A = bx; if (word_104A <= 0) return; }
  bx = d->ypos; dx = new_x; dx += ctx.get_offset(bx);
  p += d->runlength; p--; q = p; bh = 0;
  for (int i = 0; i < d->numruns; i++) {
    di = dx;
    for (int j = 0; j < word_104A; j++) {
      bl = *q--; bl = get_b152(bl);
      al = data[di]; al &= and_table[bl]; al |= or_table[bl]; data[di] = al; di++;
    }
    dx += ctx.word_1055; p += d->runlength; q = p;
  }
  (void)cf; (void)bh; (void)ax;
}
void decode_viewport_data(DecodeCtx& ctx, const unsigned char* data, viewport_data* vp) {
  int ax; std::uint8_t al; std::uint16_t bx; const unsigned char* ds = data;
  vp->runlength = *ds++; vp->numruns = *ds++;
  al = *ds++; ax = (std::int8_t)al; if (ctx.byte_104E & 0x80) ax = -ax;
  vp->xpos += ax;
  if (vp->runlength >= 0x80 && ctx.byte_104E >= 0x80) vp->xpos--;
  vp->runlength &= 0x7F;
  al = *ds++; ax = (std::int8_t)al; if (ctx.byte_104E & 0x40) ax = -ax; vp->ypos += ax;
  bx = vp->xpos; bx &= 1; bx = bx << 1;
  if (vp->xpos >= 0x8000) bx |= 4;
  if (ctx.byte_104E & 0x80) bx |= 8;
  ax = ctx.word_1053; if (ctx.byte_104E & 0x40) ax = -ax; ctx.word_1055 = ax;
  switch (bx) {
    case 0: process_quadrant(ctx, vp, ctx.viewport_memory); break;
    case 2: draw_viewport_word_mode(ctx, vp, ctx.viewport_memory); break;
    case 4: draw_viewport_neg_x(ctx, vp, ctx.viewport_memory); break;
    case 6: draw_viewport_neg_x_alt(ctx, vp, ctx.viewport_memory); break;
    case 8: draw_viewport_flip_y(ctx, vp, ctx.viewport_memory); break;
    default:
      std::fprintf(stderr, "minimap decode: unhandled BX 0x%04X\n", bx);
      return;
  }
}

// ---------------------------------------------------------------------------
// minimap 組合器 (mirror golden_minimap.c)。持有 cpu/level 鏡像狀態。
struct MinimapComposer {
  const res::Level& lvl;
  const LevelComponents& lc;
  const ComponentStore& comps;
  DecodeCtx& ctx;

  // game_state.unknown[] 子集
  std::uint8_t gx, gy;
  std::uint8_t bound_w, bound_h, bound_f;

  // cpu (16-bit)
  std::uint16_t bx = 0, dx = 0, di = 0;
  std::uint16_t word_551F = 0;
  std::uint8_t byte_551E = 0;
  std::uint16_t word_11C6 = 0;

  // minimap state
  std::uint8_t byte_1960 = 0, byte_1961 = 0, byte_1962 = 0, byte_1964 = 0;
  std::uint8_t byte_1966 = 0, byte_1949 = 0;

  // 可變的 level bytes copy (供 seed 探索旗標 + get_map_tile_data 讀)。
  std::vector<std::uint8_t> map;
  std::array<std::uint16_t, 128> data_5A04{};

  // minimap viewport 模板 (前 2 byte = runlength,numruns)。
  const std::uint8_t* minimap_vp;
  std::size_t minimap_vp_len;
  const std::uint8_t* data_6820;
  std::size_t data_6820_len;

  MinimapComposer(const res::Level& L, const LevelComponents& C,
                  const ComponentStore& S, DecodeCtx& X)
      : lvl(L), lc(C), comps(S), ctx(X) {
    bound_h = static_cast<std::uint8_t>(L.h);
    bound_w = static_cast<std::uint8_t>(L.w);
    bound_f = static_cast<std::uint8_t>(L.flags);
    map.resize(L.size());
    for (std::size_t i = 0; i < L.size(); ++i) map[i] = L.byte_at(i);
    // data_5A04 (read_level_metadata 尾段):si=w; di=grid; ax=3*h。
    int sidx = bound_w;
    std::uint16_t doff = static_cast<std::uint16_t>(L.grid());
    std::uint16_t a = static_cast<std::uint16_t>(3 * bound_h);
    do {
      data_5A04[sidx] = doff;
      doff += a;
      sidx--;
    } while (static_cast<std::uint16_t>(sidx) < 0x8000);
  }

  std::uint8_t byte_at(std::size_t i) const { return i < map.size() ? map[i] : 0; }

  void check_map_boundary_x() {
    std::uint8_t bl = bx & 0xFF;
    if (bl < bound_w) return;
    byte_551E = 0x80;
    if (bl < 0x80) { bl = bound_w; bl--; bx = (bx & 0xFF00) | bl; return; }
    bx = bx & 0xFF00;
  }
  void check_map_boundary_y() {
    std::uint8_t dl = dx & 0xFF;
    if (dl < bound_h) return;
    byte_551E = 0x80;
    if (dl > 0x80) dx = 0xFF00;
    else { dl = bound_h; dl--; dx = (dx & 0xFF00) | dl; }
  }
  // 讀 (cpu.bx=x, cpu.dx=y) 的 tile record → word_11C6;word_551F = 該 record offset。
  void get_map_tile_data() {
    check_map_boundary_x();
    check_map_boundary_y();
    dx = dx & 0xFF;
    std::uint16_t ax = dx; ax = static_cast<std::uint16_t>(ax << 1); ax = static_cast<std::uint16_t>(ax + dx);
    bx = bx & 0xFF;
    ax = static_cast<std::uint16_t>(ax + data_5A04[bx + 1]);
    word_551F = ax;
    di = ax;
    word_11C6 = static_cast<std::uint16_t>(byte_at(di) | (byte_at(di + 1) << 8));
    if ((byte_551E & 0x80) == 0x80) { byte_551E = 0; word_11C6 &= 0x3000; }
  }

  // 標記 (xx,yy) 為 seen (對齊 oracle refresh_viewport:
  // data_5521[word_551F+1] |= 0x8;boundary 命中 → byte_551E 觸發,視為 cf=1 不設)。
  void mark_seen(std::uint8_t xx, std::uint8_t yy) {
    bx = xx; dx = yy; byte_551E = 0;
    get_map_tile_data();
    if (byte_551E == 0 && word_551F + 1 < map.size()) map[word_551F + 1] |= 0x8;
  }

  void seed_explored(Minimap::Seed seed) {
    if (seed == Minimap::Seed::kNone) return;
    if (seed == Minimap::Seed::kPlayer) {
      mark_seen(gx, gy);
    } else {  // kAll
      for (std::uint8_t yy = 0; yy < bound_h; ++yy)
        for (std::uint8_t xx = 0; xx < bound_w; ++xx) mark_seen(xx, yy);
    }
    byte_551E = 0;
  }

  // 從外部 seen bitmap (row-major, w*h;非 0 = seen) 套用探索旗標。
  void seed_from_bitmap(const std::uint8_t* seen, int sw, int sh) {
    if (!seen) return;
    for (int yy = 0; yy < sh && yy < bound_h; ++yy)
      for (int xx = 0; xx < sw && xx < bound_w; ++xx)
        if (seen[(std::size_t)yy * sw + xx])
          mark_seen((std::uint8_t)xx, (std::uint8_t)yy);
    byte_551E = 0;
  }

  // engine.c:2955
  void calc_minimap_position() {
    std::uint8_t bl = 3; bl -= byte_1964; bl += byte_1960;
    bx = (bx & 0xFF00) | bl;
    std::uint8_t dl = byte_1961; dl -= 4; dl += byte_1962;
    dx = (dx & 0xFF00) | dl;
  }

  // engine.c:2979 — minimap_viewport 空地磚 @ (col*0x20, 0x18)。
  void draw_minimap_segment(int col) {
    viewport_data vp{};
    ctx.byte_104E = 0;  // word_104F = bx(=0x695C) 但 minimap_vp 不用 word_104F。
    vp.xpos = static_cast<std::uint16_t>(col << 5);
    vp.ypos = 0x18;
    vp.data = minimap_vp;
    decode_viewport_data(ctx, minimap_vp, &vp);
  }

  // engine.c:3150 — 把 data_59E4[val] sprite 畫到當前格。
  void plot_minimap_resource(std::uint8_t val, std::uint16_t di_in) {
    if (val > 0x7F) return;
    int tag = lc.tag_for(val);
    const std::vector<std::uint8_t>* comp = (tag >= 0) ? comps.get(tag) : nullptr;
    if (!comp || comp->empty()) return;

    std::uint16_t word_104F = 0;
    std::uint16_t ax = static_cast<std::uint16_t>(byte_1962 << 5);
    int d = di_in >> 1;
    std::uint16_t xpos = static_cast<std::uint16_t>(ax + data_19FE[d]);
    int ypos = static_cast<std::int16_t>(data_19FE[d + 1]);
    std::uint16_t sprite_offset = data_19FE[d + 2];

    // draw_sprite_to_viewport (engine.c:5512):header @ word_104F+sprite_offset。
    std::size_t hdr = (std::size_t)word_104F + sprite_offset;
    if (hdr + 1 >= comp->size()) return;
    std::uint16_t size = (*comp)[hdr] | ((*comp)[hdr + 1] << 8);
    if (size == 0) return;
    word_104F = static_cast<std::uint16_t>(word_104F + size);
    if (word_104F >= comp->size()) return;
    const std::uint8_t* payload = comp->data() + word_104F;

    viewport_data vp{};
    ctx.byte_104E = 0;
    vp.xpos = xpos;
    vp.ypos = ypos;
    vp.data = payload;
    decode_viewport_data(ctx, payload, &vp);
  }

  // engine.c:3184 — 玩家標記 (data_6820) @ (col*0x20, 0x18)。
  void draw_player_marker() {
    viewport_data vp{};
    ctx.byte_104E = 0;
    vp.xpos = static_cast<std::uint16_t>(byte_1962 << 5);
    vp.ypos = 0x18;
    vp.data = data_6820;
    decode_viewport_data(ctx, data_6820, &vp);
  }

  // engine.c:2994
  void draw_minimap_row(int input) {
    std::uint8_t al, bl, dl;
    calc_minimap_position();
    al = 0;
    bl = bx & 0xFF; dl = dx & 0xFF;
    if (bl == gx && dl == gy) al = 0xFF;
    byte_1966 = al;
    // calc 已留 cpu.bx=x, cpu.dx=y。
    get_map_tile_data();

    if ((((word_11C6 & 0xFF00) >> 8) & 0x08) != 0) {
      bl = (word_11C6 & 0xFF00) >> 8; bl = bl >> 4; bl = bl & 0x3;
      al = lc.a56E5[bl + 4]; plot_minimap_resource(al, 0);
      bl = word_11C6; bl = bl >> 4; bl = bl & 0xF;
      if (bl != 0) { al = lc.a56C6[bl]; plot_minimap_resource(al, 6); }
      bl = word_11C6; bl = bl & 0xF;
      if (bl != 0) { al = lc.a56C6[bl]; plot_minimap_resource(al, 0xC); }
      if ((byte_1966 & 0x80) != 0) { draw_player_marker(); return; }
      bl = (word_11C6 & 0xFF00) >> 8; bl &= 0x7;
      if (bl == 0) return;
      al = lc.a56E5[bl + 7]; plot_minimap_resource(al, 0);
      return;
    }
    // 空地磚 + 鄰格物件投射 (18E4)
    draw_minimap_segment(input);
    al = word_11C6 & 0xFF; byte_1949 = al;

    calc_minimap_position();
    bl = bx & 0xFF; bl++;
    bx = (bx & 0xFF00) | bl;
    get_map_tile_data();
    if ((((word_11C6 & 0xFF00) >> 8) & 0x08) != 0) {
      bl = byte_1949; bl = bl >> 4; bl &= 0xF;
      if (bl != 0) { al = lc.a56C6[bl]; plot_minimap_resource(al, 6); }
    }

    calc_minimap_position();
    dl = dx & 0xFF; dl--;
    dx = (dx & 0xFF00) | dl;
    get_map_tile_data();
    if ((((word_11C6 & 0xFF00) >> 8) & 0x08) != 0) {
      bl = byte_1949; bl &= 0xF;
      if (bl != 0) { al = lc.a56C6[bl]; plot_minimap_resource(al, 0xC); }
      if ((byte_1966 & 0x80) == 0) return;
      draw_player_marker();
      return;
    }
    if ((byte_1966 & 0x80) == 0) return;
    draw_player_marker();
  }

  // engine.c:3204 第一次完整 sweep (byte_1964 == 0)。
  // 注意:這只組「最頂一帶」的 9×N 格(對齊 golden_minimap.c 的 leaf-level dump)。
  //   真正的 `?` 平面地圖在原版是 8 趟(byte_1964 0..7)的捲動疊圖——見 draw_minimap_full。
  void draw_minimap_first_pass() {
    byte_1964 = 0;
    ctx.byte_104E = 0;
    std::uint8_t al = 0;
    do {
      byte_1962 = al;
      draw_minimap_row(al);
      al = byte_1962;
      al++;
    } while (al < 9);
  }

  // 單趟組圖(指定 byte_1964 帶)→ 把 9 格 row 解到 ctx.viewport_memory。
  //   呼叫端負責先清 viewport_memory(每趟獨立組),再從 ypos 0x18 區取結果。
  void draw_minimap_pass(std::uint8_t pass) {
    byte_1964 = pass;
    ctx.byte_104E = 0;
    std::uint8_t al = 0;
    do {
      byte_1962 = al;
      draw_minimap_row(al);
      al = byte_1962;
      al++;
    } while (al < 9);
  }
};

// engine.c:3204/3118 — 完整 8 趟平面地圖的螢幕疊圖映射。
//   每趟 byte_1964=bx 讀不同 map row 帶(calc_minimap_position: bl=3-byte_1964+byte_1960),
//   set_viewport_size(bx) 把該趟 viewport_memory 的 src row(data_19B7[bx]/2)起、
//   高度 (data_19A7-data_1997)[bx] 的帶,blit 到螢幕 row data_1997[bx]。
//   原版以 ui_viewport_reset memmove 捲動呈現;這裡直接把每趟結果疊進 out_mem 對應螢幕列,
//   使 to_framebuffer 取到的就是完整鋪滿的平面地圖(修稽核 #1:只畫一帶 → 鋪滿)。
const std::uint16_t kVpDst[8]  = {0x0000,0x0010,0x0028,0x0040,0x0058,0x0070,0x0088,0x00A0};
const std::uint16_t kVpDstE[8] = {0x0010,0x0028,0x0040,0x0058,0x0070,0x0088,0x00A0,0x00A8};
const std::uint16_t kVpSrc[8]  = {0x0030,0x0020,0x0020,0x0020,0x0020,0x0020,0x0020,0x0020};

}  // namespace

bool Minimap::load_templates(const std::string& minimap_bin, const std::string& player_bin) {
  auto read = [](const std::string& path, std::vector<std::uint8_t>& out) -> bool {
    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;
    std::fseek(f, 0, SEEK_END);
    long sz = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    bool ok = sz > 0;
    if (ok) {
      out.resize((std::size_t)sz);
      ok = std::fread(out.data(), 1, (std::size_t)sz, f) == (std::size_t)sz;
    }
    std::fclose(f);
    return ok;
  };
  bool a = read(minimap_bin, minimap_template_);
  bool b = read(player_bin, player_template_);
  return a && b;
}

namespace {
// 共用 setup:建 composer + 載模板 (避免 render / render_with_seen 重複)。
MinimapComposer make_composer(const res::Level& level, int px, int py,
                              const LevelComponents& lc, const ComponentStore& comps,
                              DecodeCtx& ctx,
                              const std::vector<std::uint8_t>& mm_tmpl,
                              const std::vector<std::uint8_t>& pl_tmpl) {
  MinimapComposer mc(level, lc, comps, ctx);
  mc.gx = static_cast<std::uint8_t>(px);
  mc.gy = static_cast<std::uint8_t>(py);
  mc.byte_1960 = static_cast<std::uint8_t>(px);
  mc.byte_1961 = static_cast<std::uint8_t>(py);
  mc.minimap_vp = mm_tmpl.data();
  mc.minimap_vp_len = mm_tmpl.size();
  mc.data_6820 = pl_tmpl.data();
  mc.data_6820_len = pl_tmpl.size();
  return mc;
}
}  // namespace

void Minimap::render(const res::Level& level, int px, int py,
                     const ComponentStore& comps, Seed seed) {
  reset();
  LevelComponents lc = parse_level_components(level);
  DecodeCtx ctx;
  ctx.viewport_memory = mem.data();
  ctx.init_offsets(0x90);
  ctx.byte_104E = 0;
  MinimapComposer mc =
      make_composer(level, px, py, lc, comps, ctx, minimap_template_, player_template_);
  mc.seed_explored(seed);
  mc.draw_minimap_first_pass();
}

namespace {
// 跑一趟 pass 的共用核心:清 ctx.viewport_memory、組 9×N 帶、把 src row 起的帶
//   複製到 out_mem 的螢幕列(stride 0x90)。seed 已在 composer 建立時套用,故每趟共用
//   同一 composer(map[] 的 seen 旗標跨趟保留)。
void blit_pass(MinimapComposer& mc, DecodeCtx& ctx, std::uint8_t pass,
               std::array<std::uint8_t, Minimap::kMemSize>& out_mem) {
  std::memset(ctx.viewport_memory, 0, (std::size_t)Minimap::kMemSize);
  mc.draw_minimap_pass(pass);
  int dst0 = kVpDst[pass];
  int h = kVpDstE[pass] - kVpDst[pass];
  int src_row = kVpSrc[pass] >> 1;  // get_offset(di/2) → di/2 列
  for (int r = 0; r < h; ++r) {
    int sr = src_row + r, dr = dst0 + r;
    if (sr >= Minimap::kRows || dr >= Minimap::kRows) break;
    std::memcpy(out_mem.data() + (std::size_t)dr * Minimap::kStride,
                ctx.viewport_memory + (std::size_t)sr * Minimap::kStride,
                Minimap::kStride);
  }
}
}  // namespace

void Minimap::render_full(const res::Level& level, int px, int py,
                          const ComponentStore& comps, Seed seed) {
  reset();
  LevelComponents lc = parse_level_components(level);
  // 暫存 viewport_memory(每趟獨立組);out = this->mem(疊好的螢幕鏡像)。
  std::vector<std::uint8_t> scratch((std::size_t)kMemSize, 0);
  DecodeCtx ctx;
  ctx.viewport_memory = scratch.data();
  ctx.init_offsets(0x90);
  ctx.byte_104E = 0;
  MinimapComposer mc =
      make_composer(level, px, py, lc, comps, ctx, minimap_template_, player_template_);
  mc.seed_explored(seed);
  for (std::uint8_t pass = 0; pass < 8; ++pass) blit_pass(mc, ctx, pass, mem);
}

void Minimap::render_full_with_seen(const res::Level& level, int px, int py,
                                    const ComponentStore& comps,
                                    const std::uint8_t* seen, int seen_w, int seen_h) {
  reset();
  LevelComponents lc = parse_level_components(level);
  std::vector<std::uint8_t> scratch((std::size_t)kMemSize, 0);
  DecodeCtx ctx;
  ctx.viewport_memory = scratch.data();
  ctx.init_offsets(0x90);
  ctx.byte_104E = 0;
  MinimapComposer mc =
      make_composer(level, px, py, lc, comps, ctx, minimap_template_, player_template_);
  mc.seed_from_bitmap(seen, seen_w, seen_h);
  for (std::uint8_t pass = 0; pass < 8; ++pass) blit_pass(mc, ctx, pass, mem);
}

void Minimap::render_with_seen(const res::Level& level, int px, int py,
                               const ComponentStore& comps,
                               const std::uint8_t* seen, int seen_w, int seen_h) {
  reset();
  LevelComponents lc = parse_level_components(level);
  DecodeCtx ctx;
  ctx.viewport_memory = mem.data();
  ctx.init_offsets(0x90);
  ctx.byte_104E = 0;
  MinimapComposer mc =
      make_composer(level, px, py, lc, comps, ctx, minimap_template_, player_template_);
  mc.seed_from_bitmap(seen, seen_w, seen_h);
  mc.draw_minimap_first_pass();
}

void Minimap::to_framebuffer(render::Framebuffer& fb, int ox, int oy, int rows) const {
  // 可見寬度 = 9 格 × 32 px = 288 px = 144 byte (整列 stride)。
  constexpr int cols = kStride;  // 144 byte
  const std::uint8_t* src = mem.data();
  for (int y = 0; y < rows; ++y) {
    int fx = ox;
    for (int x = 0; x < cols; ++x) {
      std::uint8_t al = src[y * kStride + x];
      fb.put(fx++, oy + y, (al >> 4) & 0x0F);
      fb.put(fx++, oy + y, al & 0x0F);
    }
  }
}

}  // namespace dw::render
