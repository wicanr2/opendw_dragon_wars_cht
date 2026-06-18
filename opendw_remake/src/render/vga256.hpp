// vga256 — 「真 VGA」256 色增強(remake 加值;原版無此版本)。
//
// 設計動機:本引擎的世界畫面(第一人稱 viewport 牆面、俯視世界地圖地形)是
//   **程式化即時繪製**到 320×200 indexed framebuffer(每像素一個 0–15 DOS EGA 索引),
//   不是讀預烤的 tile bitmap。因此 256 色增強不採「離線重畫 tile」而採
//   **演算法增強(algorithmic enhancement)**:以 DOS EGA 16 色畫面為來源,
//   程式化擴成 256 色更精緻版本。
//
// 誠實標示(這是演算法增強,非逐像素手繪重畫):
//   1) palette:DOS 16 基色,每色生成 16 階陰影/高光 ramp(共 256 entry)。
//      vga_palette()[base*16 + s]:s=8 ≈ 原 DOS 色;s<8 漸暗、s>8 漸亮。
//   2) 增強 pass(enhance_to_256):讀 16 色 framebuffer,對每像素依「局部脈絡」
//      選一個 shade,輸出 256 色索引——
//        - 垂直漸層:大片同色區由上而下微微變化(模擬光照/景深立體感)。
//        - 邊緣柔化:與相鄰異色交界處壓暗一階(描邊→立體感、柔邊)。
//      → flat EGA 色塊變成有漸層、有邊緣立體感的 256 色畫面。
//
// Deep module:對外只露 vga_palette()(256 色盤)+ enhance_to_256(fb)(16 色→256 色索引);
//   呼叫端(SdlVideo)拿 256 色 buffer + 盤直接 to_rgb,不需理解 ramp / 邊緣演算法。
//
// 影響範圍誠實揭露:此 pass 作用於「整張已繪製的 16 色 framebuffer」,故 viewport
//   牆面、worldmap 地形 tile、UI 純色塊一律受惠;文字層(host TTF)在像素層之上、
//   不經此 pass。哪些畫面最有感:第一人稱 viewport(--fp)與世界地圖(--automap)。
#pragma once

#include <array>
#include <cstdint>

#include "framebuffer.hpp"

namespace dw::render {

// 每個 DOS 基色展開的 shade 階數(總盤 = 16 * kVgaShades = 256)。
inline constexpr int kVgaShades = 16;

// 256 色 VGA 盤:16 DOS 基色 × 16 階陰影/高光 ramp。
//   index = base*16 + shade(shade 0=最暗 … 8≈原色 … 15=最亮)。
//   ramp 以「往黑插值(暗階)/ 往白插值(亮階)」生成,維持各基色色相一致協調。
const std::array<Rgb, 256>& vga_palette();

// 把一個基色(0–15)+ shade(0–15)合成 256 色索引(夾在合法範圍)。
inline std::uint8_t vga_index(int base, int shade) {
  if (base < 0) base = 0; else if (base > 15) base = 15;
  if (shade < 0) shade = 0; else if (shade > 15) shade = 15;
  return static_cast<std::uint8_t>(base * kVgaShades + shade);
}

// 增強 pass:16 色 framebuffer → 256 色索引 buffer(同 320×200,逐像素一個 0–255)。
//   演算法(垂直漸層 + 邊緣壓暗)詳見檔頭。輸出可直接用 vga_palette() to_rgb。
std::array<std::uint8_t, kW * kH> enhance_to_256(const Framebuffer& fb);

}  // namespace dw::render
