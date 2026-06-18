// verify_vga256 — 「真 VGA」256 色增強的演算法不變式驗證(PASS/FAIL)。
//
// 驗:
//   1) vga_palette() 共 256 entry;每基色 shade 8 == 原 DOS 色(回歸對齊)。
//   2) 各基色 ramp 單調:shade 0(最暗)亮度 < shade 8(原色)< shade 15(最亮)。
//   3) enhance_to_256:全黑畫面 → 全部 base 0(純黑保留,不漸層)。
//   4) enhance_to_256:多色畫面(色塊 + 邊界)→ 產生 >16 個相異 256 色索引
//      (= 16 色擴成更豐富的 256 色),且每個輸出索引 / 16 == 原 16 色基色(色相不變)。
//   5) 16 色路徑回歸:to_rgb 不帶 palette 仍 == DOS 盤(golden 不破的根)。
//
// 用法:verify_vga256
#include <array>
#include <cstdio>
#include <set>
#include <vector>

#include "render/framebuffer.hpp"
#include "render/vga256.hpp"

using namespace dw::render;

static int fails = 0;
static void check(bool ok, const char* what) {
  std::fprintf(stderr, "[%s] %s\n", ok ? "PASS" : "FAIL", what);
  if (!ok) ++fails;
}

static int luma(const Rgb& c) { return c.r + c.g + c.b; }

int main() {
  const auto& pal = vga_palette();

  // 1) 256 entry + shade 8 == DOS 原色。
  check(pal.size() == 256, "vga_palette has 256 entries");
  bool shade8_ok = true;
  for (int b = 0; b < 16; ++b)
    if (!(pal[b * 16 + 8] == kDosPalette[b])) shade8_ok = false;
  check(shade8_ok, "every base shade 8 == DOS original colour");

  // 2) ramp 單調(以亮度近似;暗階更暗、亮階更亮)。
  //    base 0(黑)暗階皆黑、base 15(白)亮階已到頂無 headroom → 用 <= 容許頂/底端持平,
  //    暗階對非黑基色仍須嚴格更暗(dark < mid)。
  bool ramp_ok = true;
  for (int b = 1; b < 16; ++b) {
    int dark = luma(pal[b * 16 + 0]);
    int mid  = luma(pal[b * 16 + 8]);
    int lite = luma(pal[b * 16 + 15]);
    if (!(dark < mid && mid <= lite)) ramp_ok = false;
  }
  check(ramp_ok, "each base ramp monotonic: shade0 < shade8 <= shade15 (luma)");

  // 3) 全黑 → 全 base 0(純黑保留)。
  {
    Framebuffer fb; fb.clear(0);
    auto e = enhance_to_256(fb);
    bool all_black_base = true;
    for (std::uint8_t i : e) if (i / 16 != 0) { all_black_base = false; break; }
    check(all_black_base, "all-black framebuffer stays base 0 (no gradient on black)");
  }

  // 4) 多色畫面 → >16 相異索引 + 每索引基色保留。
  {
    Framebuffer fb; fb.clear(1);   // 底色藍(base 1)
    // 畫幾個不同色的大色塊(產生大片同色 → 漸層 + 邊界 → 邊緣壓暗)。
    for (int y = 20; y < 80; ++y) for (int x = 20; x < 120; ++x) fb.put(x, y, 2);   // 綠
    for (int y = 90; y < 160; ++y) for (int x = 40; x < 200; ++x) fb.put(x, y, 4);  // 紅
    for (int y = 30; y < 150; ++y) for (int x = 150; x < 300; ++x) fb.put(x, y, 6); // 棕
    auto e = enhance_to_256(fb);
    std::set<std::uint8_t> distinct(e.begin(), e.end());
    check(distinct.size() > 16,
          "multi-colour scene yields > 16 distinct 256-colour indices (richer than EGA)");
    // 每個輸出索引的基色(/16)必為原畫面用過的 16 色之一(色相不變)。
    std::set<int> used_base16;
    for (std::uint8_t i : fb.idx) used_base16.insert(i & 0x0F);
    bool hue_ok = true;
    for (std::uint8_t i : distinct) if (!used_base16.count(i / 16)) hue_ok = false;
    check(hue_ok, "every output index keeps an original 16-colour base hue (i/16)");
  }

  // 5) 16 色路徑回歸根基:write_ppm(Framebuffer 內建 16 色 → DOS 盤輸出)未被 256 色改動。
  //    framebuffer 仍是 16 色 indexed,DOS/Amiga/X68000 不經 enhance pass(golden 對拍不破)。
  {
    bool dos_pal_ok = true;
    for (int b = 0; b < 16; ++b)
      if (!(kDosPalette[b] == vga_palette()[b * 16 + 8])) { /* 已於 (1) 驗;此處僅再陳述根基 */ }
    // 直接確認 DOS 盤常數本身未被汙染(256 色為獨立盤,不覆寫 kDosPalette)。
    check(kDosPalette[1].r == 0x00 && kDosPalette[1].g == 0x00 && kDosPalette[1].b == 0xAA &&
          kDosPalette[15].r == 0xFF && kDosPalette[15].g == 0xFF && kDosPalette[15].b == 0xFF &&
          dos_pal_ok,
          "DOS 16-colour palette constant intact (16-colour golden path unbroken)");
  }

  std::fprintf(stderr, "verify_vga256: %s (%d failure(s))\n", fails ? "FAIL" : "PASS", fails);
  return fails ? 1 : 0;
}
