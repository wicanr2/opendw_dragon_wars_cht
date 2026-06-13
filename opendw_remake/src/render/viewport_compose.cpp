#include "render/viewport_compose.hpp"

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

}  // namespace dw::render
