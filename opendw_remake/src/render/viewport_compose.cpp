#include "render/viewport_compose.hpp"

#include <cstdio>

namespace dw::render {
namespace {

// --- FOV 步進表 (engine.c:191/197),copied verbatim. ---
const unsigned short data_5303[] = {0x0016, 0x002E, 0x0046, 0x005E};
const unsigned char data_530B[] = {
    0xff, 0x03, 0x00, 0x03, 0x01, 0x03, 0xff, 0x02, 0x00, 0x02, 0x01, 0x02,
    0xff, 0x01, 0x00, 0x01, 0x01, 0x01, 0xff, 0x00, 0x00, 0x00, 0x01, 0x00,
    0x03, 0x01, 0x03, 0x00, 0x03, 0xff, 0x02, 0x01, 0x02, 0x00, 0x02, 0xff,
    0x01, 0x01, 0x01, 0x00, 0x01, 0xff, 0x00, 0x01, 0x00, 0x00, 0x00, 0xff,
    0x01, 0xfd, 0x00, 0xfd, 0xff, 0xfd, 0x01, 0xfe, 0x00, 0xfe, 0xff, 0xfe,
    0x01, 0xff, 0x00, 0xff, 0xff, 0xff, 0x01, 0x00, 0x00, 0x00, 0xff, 0x00,
    0xfd, 0xff, 0xfd, 0x00, 0xfd, 0x01, 0xfe, 0xff, 0xfe, 0x00, 0xfe, 0x01,
    0xff, 0xff, 0xff, 0x00, 0xff, 0x01, 0x00, 0xff, 0x00, 0x00, 0x00, 0x01};

// opendw cpu/global 模型的最小子集。逐指令 port refresh_viewport 取樣段,
// 變數對應:cpu.ax/bx/dx/si/di → 區域變數;data_5521 → level bytes;
// data_5A04 → 由 level.grid()/dimension 推得的 per-row offset。
struct Sampler {
  const res::Level& lvl;
  // game_state.unknown[] 子集
  std::uint8_t gx, gy, gfacing;
  std::uint8_t bound_h;  // unknown[0x21] = h
  std::uint8_t bound_w;  // unknown[0x22] = w
  std::uint8_t bound_f;  // unknown[0x23] = flags

  // cpu registers (16-bit)
  std::uint16_t ax = 0, bx = 0, dx = 0, si = 0, di = 0;

  // level globals
  std::uint16_t word_11C6 = 0, word_11C8 = 0, word_11CA = 0, word_11CC = 0;
  std::uint16_t word_551F = 0;
  std::uint8_t byte_551E = 0;

  // data_5A04: per-row offset table (read_level_metadata tail).
  std::array<std::uint16_t, 128> data_5A04{};

  std::uint16_t stack[64];
  int sp = 0;
  void push_word(std::uint16_t v) { stack[sp++] = v; }
  std::uint16_t pop_word() { return stack[--sp]; }

  explicit Sampler(const res::Level& L) : lvl(L) {
    bound_h = static_cast<std::uint8_t>(L.h);
    bound_w = static_cast<std::uint8_t>(L.w);
    bound_f = static_cast<std::uint8_t>(L.flags);
    // data_5A04: si=w; di=grid; ax=3*h; do { [si]=di; di+=ax; si--; } while(si<0x8000)
    int sidx = bound_w;
    std::uint16_t doff = static_cast<std::uint16_t>(L.grid());
    std::uint16_t a = static_cast<std::uint16_t>(3 * bound_h);
    do {
      data_5A04[sidx] = doff;
      doff += a;
      sidx--;
    } while (static_cast<std::uint16_t>(sidx) < 0x8000);
  }

  // engine.c:5147
  void check_map_boundary_x() {
    std::uint8_t bl = bx & 0xFF;
    if (bl < bound_w) return;
    // (flags&2) 分支在 opendw 為 unimplemented;此關 (Purgatory) 不走到。
    byte_551E = 0x80;
    if (bl < 0x80) {
      bl = bound_w;
      bl--;
      bx = (bx & 0xFF00) | bl;
      return;
    }
    bx = bx & 0xFF00;
  }

  // engine.c:5176
  void check_map_boundary_y() {
    std::uint8_t dl = dx & 0xFF;
    if (dl < bound_h) return;
    byte_551E = 0x80;
    if (dl > 0x80) {
      dx = 0xFF00;
    } else {
      dl = bound_h;
      dl--;
      dx = (dx & 0xFF00) | dl;
    }
  }

  // engine.c:5206 — 輸入實際走 cpu.dx(dl=row/y)、cpu.bx(bl=col/x)。
  void get_map_tile_data() {
    check_map_boundary_x();
    check_map_boundary_y();

    dx = dx & 0xFF;
    ax = dx;
    ax = static_cast<std::uint16_t>(ax << 1);
    ax = static_cast<std::uint16_t>(ax + dx);
    bx = bx & 0xFF;
    ax = static_cast<std::uint16_t>(ax + data_5A04[bx + 1]);
    word_551F = ax;

    di = ax;
    ax = lvl.byte_at(di);
    ax = static_cast<std::uint16_t>(ax + (lvl.byte_at(di + 1) << 8));

    word_11C6 = ax;
    std::uint8_t al = lvl.byte_at(di + 2);
    word_11C8 = al;
    if ((byte_551E & 0x80) == 0x80) {
      ax = 0;
      byte_551E = 0;
      word_11C8 = 0;
      word_11C6 &= 0x3000;
    }
  }

  // engine.c:5332
  void move_player_on_map() {
    std::uint8_t al, bl, dl;

    push_word(dx);
    push_word(bx);
    get_map_tile_data();
    bx = pop_word();
    dx = pop_word();

    word_11CC = word_11C8;
    ax = word_11C6;
    word_11CA = ax;

    if (gfacing != 0) {
      push_word(dx);
      push_word(bx);
      dx++;
      get_map_tile_data();
      bx = pop_word();
      dx = pop_word();

      al = word_11C6 & 0xFF;
      al = al & 0xF;
      ax = (ax & 0xFF00) | al;
      word_11CC = static_cast<std::uint16_t>((al << 8) | (word_11CC & 0xFF));

      push_word(dx);
      push_word(bx);
      bx--;
      get_map_tile_data();
      bx = pop_word();
      dx = pop_word();

      bl = word_11C6 & 0xFF;
      bl &= 0xF0;
      bl |= ((word_11CC & 0xFF00) >> 8);
      bx = (bx & 0xFF00) | bl;
      dl = word_11CA & 0xFF;
      dx = (dx & 0xFF00) | dl;
      al = gfacing;
      ax = (ax & 0xFF00) | al;
      if (al > 2) {
        dl = dx & 0xFF;
        dl = static_cast<std::uint8_t>(dl << 4);
        bl = bx & 0xFF;
        bl = static_cast<std::uint8_t>(bl >> 4);
        dl = dl | bl;
        bx = (bx & 0xFF00) | bl;
        dx = (dx & 0xFF00) | dl;
        word_11CA = (word_11CA & 0xFF00) | dl;
      } else if (al == 2) {
        word_11CA = (word_11CA & 0xFF00) | bl;
      } else {
        bl = bx & 0xFF;
        bl = static_cast<std::uint8_t>(bl << 4);
        dl = dx & 0xFF;
        dl = static_cast<std::uint8_t>(dl >> 4);
        dl = dl | bl;
        bx = (bx & 0xFF00) | bl;
        dx = (dx & 0xFF00) | dl;
        word_11CA = (word_11CA & 0xFF00) | dl;
      }
    }
  }

  // refresh_viewport FOV 取樣迴圈 (engine.c:5632..5661)
  void run(FovSample& out, std::array<std::uint8_t, 128>& a5a56) {
    std::uint8_t al, bl, dl;

    bx = gfacing;
    si = data_5303[bx];
    di = 0xB;
    do {
      push_word(di);
      push_word(si);

      dl = gy;
      dl = static_cast<std::uint8_t>(dl + data_530B[si]);
      dx = (dx & 0xFF00) | dl;
      bl = gx;
      bl = static_cast<std::uint8_t>(bl + data_530B[si + 1]);
      bx = bl;

      out.sx[di] = bl;
      out.sy[di] = dl;

      move_player_on_map();
      si = pop_word();
      di = pop_word();
      al = word_11CA & 0xFF;
      a5a56[di] = al;
      al = (word_11CA & 0xFF00) >> 8;
      al &= 0xF7;
      ax = (ax & 0xFF00) | al;
      a5a56[di + 0xC] = al;
      si--;
      si--;
      di--;
    } while (di != 0xFFFF);
  }
};

}  // namespace

FovSample sample_fov(const res::Level& level, int x, int y, int facing) {
  FovSample out;
  Sampler s(level);
  s.gx = static_cast<std::uint8_t>(x);
  s.gy = static_cast<std::uint8_t>(y);
  s.gfacing = static_cast<std::uint8_t>(facing);

  // 診斷:每槽 primary tile 的 word_11C6 (facing 暫設 0,避免 neighbour 旋轉覆蓋)。
  {
    int si = data_5303[facing];
    for (int di = 0xB; di >= 0; --di) {
      std::uint8_t dl = static_cast<std::uint8_t>(s.gy + data_530B[si]);
      std::uint8_t bl = static_cast<std::uint8_t>(s.gx + data_530B[si + 1]);
      out.sx[di] = bl;
      out.sy[di] = dl;
      std::uint8_t fsave = s.gfacing;
      s.gfacing = 0;
      s.dx = (s.dx & 0xFF00) | dl;
      s.bx = bl;
      s.byte_551E = 0;
      s.get_map_tile_data();
      out.raw[di] = s.word_11C6;
      s.gfacing = fsave;
      si -= 2;
    }
  }

  // authoritative 取樣 (verbatim refresh_viewport loop)。
  std::array<std::uint8_t, 128> a5a56{};
  s.byte_551E = 0;
  s.run(out, a5a56);
  for (int i = 0; i < 12; ++i) {
    out.wall[i] = a5a56[i];
    out.ground[i] = a5a56[i + 0xC];
  }
  return out;
}

// ===========================================================================
// Step 2:元件選擇 + 繪製指令序列。
// ===========================================================================
namespace {

// 靜態 viewport 落點 / 槽對應表 (engine.c:213..291),copied verbatim。
const unsigned short data_558F[] = {
    0x0020, 0x0000, 0x0080, 0xFFC0, 0x0080, 0x0020, 0xFFC0, 0x0080,
    0x0030, 0x0020, 0x0070, 0xFFF0, 0x0070, 0x0030, 0xFFF0, 0x0070,
    0x0040, 0x0030, 0x0060, 0x0020, 0x0060, 0x0040, 0x0020, 0x0060};
const unsigned short data_55BF[] = {
    0x0010, 0x0000, 0x0000, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010,
    0x0020, 0x0010, 0x0010, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020,
    0x0030, 0x0020, 0x0020, 0x0030, 0x0030, 0x0030, 0x0030, 0x0030};
const unsigned short data_55EF[] = {
    0x0016, 0x000A, 0x000B, 0x0015, 0x0017, 0x008A, 0x0089, 0x008B,
    0x0013, 0x0007, 0x0008, 0x0012, 0x0014, 0x0087, 0x0086, 0x0088,
    0x0010, 0x0004, 0x0005, 0x000F, 0x0011, 0x0084, 0x0083, 0x0085};
const unsigned short data_561F[] = {
    0x0004, 0x000C, 0x000C, 0x0004, 0x0004, 0x0004, 0x0004, 0x0004,
    0x0006, 0x000E, 0x000E, 0x0006, 0x0006, 0x0006, 0x0006, 0x0006,
    0x0008, 0x0010, 0x0010, 0x0008, 0x0008, 0x0008, 0x0008, 0x0008};
const unsigned short data_564F[] = {
    0x0001, 0x0000, 0x0080, 0x0001, 0x0001, 0x0000, 0x0000, 0x0000,
    0x0001, 0x0000, 0x0080, 0x0001, 0x0001, 0x0000, 0x0000, 0x0000,
    0x0001, 0x0000, 0x0080, 0x0001, 0x0001, 0x0000, 0x0000, 0x0000};
struct UiPoint {
  short x, y;
};
const UiPoint ground_points[] = {{16, 120}, {0, 120}, {128, 120},
                                 {32, 104}, {0, 104}, {112, 104},
                                 {48, 88},  {0, 88},  {96, 88}};
const unsigned short data_56A3[] = {0x000A, 0x0009, 0x000B, 0x0007, 0x0006,
                                    0x0008, 0x0004, 0x0003, 0x0005};
const unsigned short sprite_indices[] = {18, 16, 20, 12, 10, 14, 6, 4, 8};

// opendw resource 子系統的選擇相依子集:resource_load(sec).tag == sec,
// allocations[0]/[1] 為靜態保留 (rm_init),第一個動態 load 落在 index 2。
struct ResTable {
  struct Slot {
    int tag = 0;
    int usage = 0;
    int index = 0;
  };
  std::array<Slot, 128> slots{};

  ResTable() {
    slots[0].usage = 0xFF;
    slots[0].tag = 0xFFFF;
    slots[0].index = 0;
    slots[1].usage = 0xFF;
    slots[1].tag = 0xFFFE;
    slots[1].index = 1;
    // index 2 = 關卡資源本身 (level_res);本步不需其 tag 參與選擇,僅佔位。
    slots[2].usage = 1;
    slots[2].tag = 0x47;
    slots[2].index = 2;
  }
  int find_by_tag(int tag) const {
    for (int i = 0; i < 128; ++i)
      if (slots[i].tag == tag && slots[i].usage != 0) return i;
    return -1;
  }
  // 回傳 allocation index (= data_5897[i+0xF] 存的值)。
  int load(int sec) {
    int idx = find_by_tag(sec);
    if (idx != -1) return idx;
    int i = 0;
    for (; i < 128; ++i)
      if (slots[i].usage == 0) break;
    slots[i].usage = 1;
    slots[i].tag = sec;
    slots[i].index = i;
    return i;
  }
};

// read_level_metadata + cache_level_components + cache_resources 的選擇相依
// 部分,verbatim port (操作 level bytes,不讀 DATA1)。
struct MetaParser {
  const res::Level& lvl;
  std::array<std::uint8_t, 256>& a5897;
  std::array<std::uint8_t, 128>& a56C6;
  std::array<std::uint8_t, 128>& a56E5;
  ResTable& rt;

  std::uint16_t ax = 0, bx = 0, si = 0, di = 0;
  std::uint16_t stk[64];
  int sp = 0;
  void push(std::uint16_t v) { stk[sp++] = v; }
  std::uint16_t pop() { return stk[--sp]; }
  std::uint8_t byte_at(std::uint16_t i) const { return lvl.byte_at(i); }

  void advance_data_ptr() {
    di = static_cast<std::uint16_t>(di + bx);
    bx = 0;
  }

  // engine.c:5037
  void cache_level_components(int starting_index) {
    std::uint8_t al;
    do {
      al = byte_at(static_cast<std::uint16_t>(bx + di));
      ax = (ax & 0xFF00) | al;
      push(ax);
      al &= 0x7F;
      a56E5[starting_index] = al;
      bx++;
      starting_index++;
      ax = pop();
    } while ((ax & 0xFF) < 0x80);
    di = static_cast<std::uint16_t>(di + bx);
    bx = 0;
  }

  // engine.c:5061 (selection-relevant portion)
  void read_level_metadata() {
    std::uint8_t al;
    di = 0;
    while (di < 4) di++;  // header bytes已由 Level 解析;此處僅推進 di。
    bx = 0;
    do {
      al = byte_at(static_cast<std::uint16_t>(bx + di));
      a5897[bx] = al;
      bx++;
    } while (al < 0x80);
    advance_data_ptr();
    si = 0;
    do {
      al = byte_at(static_cast<std::uint16_t>(bx + di));
      ax = (ax & 0xFF00) | al;
      push(ax);
      al &= 0x7F;
      ax = (ax & 0xFF00) | al;
      a56C6[si + 1] = al;
      bx++;
      al = byte_at(static_cast<std::uint16_t>(bx + di));
      a56C6[si + 0xf + 1] = al;
      bx++;
      si++;
      ax = pop();
    } while ((ax & 0xFF) < 0x80);
    si = 0;
    cache_level_components(0);
    si = 4;
    cache_level_components(4);
    si = 8;
    cache_level_components(8);
    // (level 名稱 offset + data_5A04 尾段不影響元件選擇,略。)
  }

  // engine.c:5468 (selection-relevant portion;resource_load = tag 計算)。
  void cache_resources(std::array<int, 128>& tags) {
    std::uint8_t al, bl;
    bx = 0xFFFF;
    do {
      bx++;
      push(bx);
      bl = a5897[bx];
      bx = (bx & 0xFF00) | bl;
      bx &= 0x7F;
      bx += 0x6E;
      int idx = rt.load(bx);  // resource_load(sec).index
      bx = pop();
      a5897[bx + 0xf] = static_cast<std::uint8_t>(idx);
      al = a5897[bx];
    } while (al < 0x80);
    bx = 0xE;
    do {
      al = a5897[bx + 0xf];
      if (al < 0x80) {
        tags[bx] = rt.slots[al].tag;  // data_59E4[bx] = resource_get_by_index(al)
      }
      bx = static_cast<std::uint16_t>(bx - 1);
    } while (bx != 0xFFFF);
  }
};

}  // namespace

LevelComponents parse_level_components(const res::Level& level) {
  LevelComponents lc;
  lc.tags.fill(-1);
  ResTable rt;
  MetaParser mp{level, lc.a5897, lc.a56C6, lc.a56E5, rt};
  mp.read_level_metadata();
  mp.cache_resources(lc.tags);
  return lc;
}

std::vector<DrawCmd> compose_draw_sequence(const res::Level& level, int x, int y,
                                           int facing) {
  std::vector<DrawCmd> out;

  // 1) FOV 取樣 → data_5A56[0..23] (step1,已對拍)。
  FovSample fov = sample_fov(level, x, y, facing);
  std::array<std::uint8_t, 128> a5a56{};
  for (int i = 0; i < 12; ++i) {
    a5a56[i] = fov.wall[i];
    a5a56[i + 0xC] = fov.ground[i];
  }

  // 2) 元件表 (read_level_metadata + cache_*)。
  LevelComponents lc = parse_level_components(level);

  // 3) 選擇 + draw 呼叫序列 (refresh_viewport,sky → ground → other)。
  std::uint16_t bx = 0;

  // --- sky (draw_viewport_sky, engine.c:5533) ---
  {
    std::uint8_t bl = a5a56[0x16];
    int cf = 0;
    for (int i = 0; i < 3; ++i) {
      int tmp_carry = (bl & 0x80) ? 1 : 0;
      bl = static_cast<std::uint8_t>((bl << 1) + cf);
      cf = tmp_carry;
    }
    bx = (bx & 0xFF00) | bl;
    bx &= 3;
    bl = lc.a56E5[bx];
    bx = (bx & 0xFF00) | bl;
    std::uint8_t al = lc.a5897[bx];
    al &= 0x7F;
    if (al == 1) {
      out.push_back(DrawCmd{0, -1, lc.tags[bx], 4, 0, 0, 0});
    }
  }

  // --- ground 9 sprites (counter 8..0) ---
  for (int counter = 8; counter >= 0; --counter) {
    bx = data_56A3[counter];
    std::uint8_t bl = a5a56[bx + 0xC];
    bl = static_cast<std::uint8_t>(bl >> 4);
    bx = (bx & 0xFF00) | bl;
    bx &= 3;
    bl = lc.a56E5[bx + 4];
    bx = (bx & 0xFF00) | bl;
    int tag = lc.tags[bx];
    out.push_back(DrawCmd{1, counter,
                          tag,
                          sprite_indices[counter],
                          ground_points[counter].x,
                          ground_points[counter].y,
                          0});
  }

  // --- other components (counter 23..0) ---
  for (int counter = 23; counter >= 0; --counter) {
    std::uint16_t ax = data_55EF[counter];
    std::uint16_t di = ax;
    di &= 0x007F;
    std::uint8_t bl = a5a56[di];
    if ((ax & 0xFF) > 0x80) bl = static_cast<std::uint8_t>(bl >> 4);
    bx = (bx & 0xFF00) | bl;
    bx &= 0x000F;
    if (bx != 0) {
      std::uint16_t cx = data_564F[counter];
      std::uint8_t b104e = cx & 0xFF;
      std::uint8_t al = lc.a56C6[bx];
      if (((cx & 0xFF) & 1) == 1) al = lc.a56E5[bx + 0x7];
      if (al <= 0x7F) {
        int tag = lc.tags[al];
        out.push_back(DrawCmd{2, counter,
                              tag,
                              data_561F[counter],
                              static_cast<short>(data_558F[counter]),
                              static_cast<short>(data_55BF[counter]),
                              b104e});
      }
    }
  }

  return out;
}

// ===========================================================================
// Step 3:像素 blit。
// ===========================================================================

const std::vector<std::uint8_t>* ComponentStore::get(int tag) const {
  auto it = cache_.find(tag);
  if (it != cache_.end()) return &it->second;
  if (missing_.count(tag)) return nullptr;
  std::string path = dir_ + "/" + std::to_string(tag) + ".bin";
  std::FILE* f = std::fopen(path.c_str(), "rb");
  if (!f) { missing_[tag] = true; return nullptr; }
  std::vector<std::uint8_t> buf;
  std::fseek(f, 0, SEEK_END);
  long sz = std::ftell(f);
  std::fseek(f, 0, SEEK_SET);
  if (sz > 0) {
    buf.resize((std::size_t)sz);
    if (std::fread(buf.data(), 1, (std::size_t)sz, f) != (std::size_t)sz) buf.clear();
  }
  std::fclose(f);
  auto& slot = cache_[tag];
  slot = std::move(buf);
  return &slot;
}

void render_first_person(const res::Level& level, int x, int y, int facing,
                         ViewportDecoder& dec, const ComponentStore& comps) {
  dec.reset(0);

  std::vector<DrawCmd> seq = compose_draw_sequence(level, x, y, facing);

  // sky:序列若無 SKY 筆 (draw_viewport_sky 走 al!=1) → 平面兩色填色。
  bool has_sky = false;
  for (const auto& d : seq)
    if (d.batch == 0) { has_sky = true; break; }
  if (!has_sky) dec.fill_sky_flat();

  // 逐筆 draw_sprite_to_viewport (每元件 word_104F 重置 0,對齊 refresh_viewport)。
  for (const auto& d : seq) {
    const std::vector<std::uint8_t>* comp = comps.get(d.tag);
    if (!comp || comp->empty()) {
      std::fprintf(stderr, "render_first_person: missing component tag %d (batch %d)\n",
                   d.tag, d.batch);
      continue;
    }
    std::uint16_t word_104F = 0;
    dec.draw_sprite(comp->data(), comp->size(), word_104F, d.sprite_offset,
                    d.xpos, d.ypos, d.byte_104E);
  }
}

}  // namespace dw::render
