#include "render/vga256.hpp"

#include <algorithm>

namespace dw::render {

namespace {

inline std::uint8_t clamp8(int v) {
  return static_cast<std::uint8_t>(v < 0 ? 0 : (v > 255 ? 255 : v));
}

// 線性插值 a→b(t/den)。
inline std::uint8_t lerp(int a, int b, int t, int den) {
  return clamp8(a + (b - a) * t / den);
}

// 由一個 DOS 基色生成 16 階 ramp:
//   shade 0..7  = 原色往黑插值(暗階;0 最暗),shade 8 = 原色,
//   shade 9..15 = 原色往白插值(亮階;15 最亮)。
//   暗/亮兩端不插到全黑/全白(留 0.85 比例),避免大色塊失去色相、顯髒。
Rgb make_shade(const Rgb& c, int shade) {
  if (shade == 8) return c;
  if (shade < 8) {
    // 8→0:t = (8-shade)/8,往黑插值(最暗保留 15% 原色,維持色相)。
    int t = 8 - shade;                  // 1..8
    Rgb dark{ static_cast<std::uint8_t>(c.r * 15 / 100),
              static_cast<std::uint8_t>(c.g * 15 / 100),
              static_cast<std::uint8_t>(c.b * 15 / 100) };
    return Rgb{ lerp(c.r, dark.r, t, 8),
               lerp(c.g, dark.g, t, 8),
               lerp(c.b, dark.b, t, 8) };
  }
  // 8→15:t = (shade-8)/7,往(略帶原色的)白插值(高光,最亮 85% 白)。
  int t = shade - 8;                    // 1..7
  Rgb bright{ static_cast<std::uint8_t>(c.r + (255 - c.r) * 85 / 100),
              static_cast<std::uint8_t>(c.g + (255 - c.g) * 85 / 100),
              static_cast<std::uint8_t>(c.b + (255 - c.b) * 85 / 100) };
  return Rgb{ lerp(c.r, bright.r, t, 7),
             lerp(c.g, bright.g, t, 7),
             lerp(c.b, bright.b, t, 7) };
}

}  // namespace

const std::array<Rgb, 256>& vga_palette() {
  static const std::array<Rgb, 256> pal = [] {
    std::array<Rgb, 256> p{};
    for (int base = 0; base < 16; ++base) {
      const Rgb& c = kDosPalette[base];
      for (int s = 0; s < kVgaShades; ++s)
        p[base * kVgaShades + s] = make_shade(c, s);
    }
    return p;
  }();
  return pal;
}

std::array<std::uint8_t, kW * kH> enhance_to_256(const Framebuffer& fb) {
  std::array<std::uint8_t, kW * kH> out{};
  const auto& src = fb.idx;

  auto at = [&](int x, int y) -> std::uint8_t {
    return src[y * kW + x] & 0x0F;
  };

  for (int y = 0; y < kH; ++y) {
    for (int x = 0; x < kW; ++x) {
      const std::uint8_t base = at(x, y);

      // 黑色(0)維持純黑,不漸層(避免邊框/留白變灰、避免破壞 letterbox 觀感)。
      if (base == 0) { out[y * kW + x] = vga_index(0, 8); continue; }

      // 1) 垂直漸層:量測本像素所在「同色直向連續區段」的高度與在區段內的相對位置,
      //    由上(亮)到下(暗)鋪一個輕微漸層 → 大色塊產生光照/景深立體感。
      int up = 0;
      for (int yy = y - 1; yy >= 0 && at(x, yy) == base; --yy) ++up;
      int down = 0;
      for (int yy = y + 1; yy < kH && at(x, yy) == base; ++yy) ++down;
      int run = up + down + 1;

      int shade = 8;
      if (run >= 4) {
        // rel 0(段頂)→ +2 高光;rel 1(段底)→ -2 陰影。中段平滑過渡。
        // 只有夠大的色塊才施加(run>=4),小細節維持原色避免雜訊。
        double rel = static_cast<double>(up) / (run - 1);   // 0..1
        shade = 8 + static_cast<int>((0.5 - rel) * 4.0 + (rel < 0.5 ? 0.5 : -0.5));
      }

      // 2) 邊緣壓暗:與相鄰(上下左右)異色交界處再壓暗一階 → 描邊/柔邊立體感。
      bool edge = false;
      if (x > 0      && at(x - 1, y) != base) edge = true;
      if (x < kW - 1 && at(x + 1, y) != base) edge = true;
      if (y > 0      && at(x, y - 1) != base) edge = true;
      if (y < kH - 1 && at(x, y + 1) != base) edge = true;
      if (edge) shade -= 2;

      shade = std::clamp(shade, 0, 15);
      out[y * kW + x] = vga_index(base, shade);
    }
  }
  return out;
}

}  // namespace dw::render
