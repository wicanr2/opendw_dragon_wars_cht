#include "render/vga256.hpp"

#include <algorithm>
#include <cmath>

namespace dw::render {

namespace {

inline std::uint8_t clamp8(int v) {
  return static_cast<std::uint8_t>(v < 0 ? 0 : (v > 255 ? 255 : v));
}

// 浮點插值 a→b(t∈[0,1]),四捨五入。
inline std::uint8_t lerpf(int a, int b, double t) {
  return clamp8(static_cast<int>(a + (b - a) * t + 0.5));
}

// 由一個 DOS 基色生成 16 階 ramp:
//   shade 0..7  = 原色往黑(暗階;0 最暗),shade 8 = 原色,
//   shade 9..15 = 原色往白(亮階;15 最亮)。
//   明度走「非線性曲線」而非等步(更自然的明度感知曲線):
//     暗階用 gamma>1(中段壓得少、靠近最暗才快速變暗)→ 陰影柔、不死黑;
//     亮階用 gamma<1(高光前段抬升明顯、末端收斂)→ 立體高光但不過曝。
//   暗/亮兩端不插到全黑/全白(暗端保留 18% 原色、亮端最亮 80% 白),維持色相不顯髒。
Rgb make_shade(const Rgb& c, int shade) {
  if (shade == 8) return c;
  if (shade < 8) {
    // t_lin = (8-shade)/8 ∈ (0,1];往黑方向。曲線:t = t_lin^1.45(陰影集中於末端)。
    double t_lin = (8 - shade) / 8.0;
    double t = std::pow(t_lin, 1.45);
    Rgb dark{ static_cast<std::uint8_t>(c.r * 18 / 100),
              static_cast<std::uint8_t>(c.g * 18 / 100),
              static_cast<std::uint8_t>(c.b * 18 / 100) };
    return Rgb{ lerpf(c.r, dark.r, t),
                lerpf(c.g, dark.g, t),
                lerpf(c.b, dark.b, t) };
  }
  // t_lin = (shade-8)/7 ∈ (0,1];往白方向。曲線:t = t_lin^0.80(高光前段抬升明顯)。
  double t_lin = (shade - 8) / 7.0;
  double t = std::pow(t_lin, 0.80);
  Rgb bright{ static_cast<std::uint8_t>(c.r + (255 - c.r) * 80 / 100),
              static_cast<std::uint8_t>(c.g + (255 - c.g) * 80 / 100),
              static_cast<std::uint8_t>(c.b + (255 - c.b) * 80 / 100) };
  return Rgb{ lerpf(c.r, bright.r, t),
              lerpf(c.g, bright.g, t),
              lerpf(c.b, bright.b, t) };
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

// 4×4 Bayer ordered-dither 矩陣(值 0..15,中心化為 -7.5..+7.5 / 16)。
//   用途:把連續 shade 量化成 16 階時,以空間抖動打散「整列同階」造成的水平條紋
//   ——尤其大片背景(海/天)垂直漸層極淺、相鄰列容易跳同一階界,抖動使邊界呈點狀
//   交錯而非整條橫線,觀感更平整(模擬抗鋸齒/誤差擴散)。
constexpr int kBayer4[4][4] = {
  { 0,  8,  2, 10},
  {12,  4, 14,  6},
  { 3, 11,  1,  9},
  {15,  7, 13,  5},
};

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

      // 量測本像素所在「同色直向連續區段」高度與相對位置(段頂=亮、段底=暗)。
      int up = 0;
      for (int yy = y - 1; yy >= 0 && at(x, yy) == base; --yy) ++up;
      int down = 0;
      for (int yy = y + 1; yy < kH && at(x, yy) == base; ++yy) ++down;
      int run = up + down + 1;

      // 同步量測水平連續區段(供雙向梯度與背景判定)。
      int left = 0;
      for (int xx = x - 1; xx >= 0 && at(xx, y) == base; --xx) ++left;
      int right = 0;
      for (int xx = x + 1; xx < kW && at(xx, y) == base; ++xx) ++right;
      int hrun = left + right + 1;

      // shadef:以浮點累積各項光照貢獻,最後才量化 → 過渡平滑、無提前跳階。
      double shadef = 8.0;

      // bg_flat ∈ [0,1]:本像素「屬於大片背景平面」的程度(0=物體/細節,1=超大平面)。
      //   用 min(run, hrun):真正的平面是兩個維度都大(海面/天空/UI 底);細長條(牆磚柱、
      //   地形帶)某一維度小 → 不算背景,保留立體漸層。>=96 視為純背景。
      //   背景程度越高,漸層與抖動越收斂,讓大平面回歸乾淨純色,物體尺寸區保留浮雕。
      const int span = std::min(run, hrun);
      double bg_flat = 0.0;
      if (span > 32) bg_flat = std::min(1.0, (span - 32) / 64.0);

      // 1) 自適應垂直漸層(光照/景深立體感):
      //    run<4   : 不漸層(小細節維持原色,避免雜訊)。
      //    run 4..28: 「物體尺寸」(單塊牆磚/地形 tile)→ amp 最強(立體浮雕)。
      //    run>28  : 大片背景平面 → amp 隨 bg_flat 衰減趨近 0,避免整片背景被鋪漸層
      //              而出現水平條紋。
      if (run >= 4) {
        double rel = static_cast<double>(up) / (run - 1);   // 0(頂)..1(底)
        double amp = 2.2 * (1.0 - 0.55 * bg_flat);          // 物體 2.2 → 純背景 ≈1.0
        double s = rel * rel * (3.0 - 2.0 * rel);           // smoothstep:過渡更柔
        shadef += (0.5 - s) * 2.0 * amp;                    // 頂 +amp、底 -amp
      }

      // 2) 水平微梯度(僅在「物體尺寸」橫段施加):左略亮、右略暗,
      //    與垂直漸層合成出柔和的方向光,讓牆磚/地形塊更有體積感、不死板。
      if (hrun >= 4 && hrun <= 28) {
        double relx = static_cast<double>(left) / (hrun - 1);  // 0..1
        shadef += (0.5 - relx) * 1.1;                          // ±0.55
      }

      // 3) 智慧描邊(立體感、柔邊):只在與「更亮鄰色」交界處壓暗(模擬該方向受光、
      //    本塊在其陰影側),與更暗鄰色交界則不壓——避免兩塊深色交界出現雙重黑線、
      //    避免大片背景被自身內部 tile 接縫切成格。壓暗幅度依鄰色明暗差調節。
      auto luma16 = [](std::uint8_t b) {
        const Rgb& c = kDosPalette[b & 0x0F];
        return c.r + c.g + c.b;
      };
      // 同色系判定:兩色色相方向接近(僅明度不同),如深藍↔亮藍、深綠↔亮綠。
      //   海波(深藍/亮藍隔列交替)、同材質明暗紋理屬此類,不該被當「物體交界」描邊
      //   ——否則同色系明度交替會被描邊放大成硬條紋。用歸一化向量夾角(整數近似)判定。
      auto same_family = [](std::uint8_t a, std::uint8_t b) {
        const Rgb& ca = kDosPalette[a & 0x0F];
        const Rgb& cb = kDosPalette[b & 0x0F];
        long dot = (long)ca.r * cb.r + (long)ca.g * cb.g + (long)ca.b * cb.b;
        long na  = (long)ca.r * ca.r + (long)ca.g * ca.g + (long)ca.b * ca.b;
        long nb  = (long)cb.r * cb.r + (long)cb.g * cb.g + (long)cb.b * cb.b;
        if (na == 0 || nb == 0) return false;            // 黑不歸任何色系
        // cos^2 >= 0.94 ⇔ dot^2 >= 0.94 * na * nb(夾角 <≈14°,同色相不同明度)。
        return (double)dot * dot >= 0.94 * (double)na * nb;
      };
      const int self_l = luma16(base);
      double edge_dark = 0.0;
      auto consider = [&](int nx, int ny) {
        if (nx < 0 || nx >= kW || ny < 0 || ny >= kH) return;
        std::uint8_t nb = at(nx, ny);
        if (nb == base) return;
        if (same_family(base, nb)) return;    // 同色系明暗紋理不描邊(海波、漸層材質)
        int d = luma16(nb) - self_l;          // 鄰色比自己亮多少
        if (d > 0) edge_dark = std::max(edge_dark, std::min(2.0, d / 220.0 + 0.6));
      };
      consider(x - 1, y); consider(x + 1, y);
      consider(x, y - 1); consider(x, y + 1);
      shadef -= edge_dark;

      // 4) ordered-dither:量化前加入空間抖動,打散漸層的水平跳階線(見 kBayer4 註解)。
      //    幅度 = 基礎 ±0.30 階 ×(1 - bg_flat):只在「物體尺寸」漸層區作用(那裡才有跨階
      //    需打散);純背景平面 bg_flat→1 使抖動→0,避免大平面被點上規律顆粒。
      //    邊緣像素(描邊)不抖,保描邊乾淨。
      if (edge_dark == 0.0) {
        // 全域保留適中抖動(物體區滿幅、純背景仍 0.55 倍):把殘留的淡漸層跨階邊界
        // 打散成點狀交錯而非整條橫線 → 海面/天空條紋消解,同時維持色深擴展。
        double d = ((kBayer4[y & 3][x & 3] / 15.0) - 0.5) * 0.85 * (1.0 - 0.45 * bg_flat);
        shadef += d;
      }

      int shade = std::clamp(static_cast<int>(std::lround(shadef)), 0, 15);
      out[y * kW + x] = vga_index(base, shade);
    }
  }
  return out;
}

}  // namespace dw::render
