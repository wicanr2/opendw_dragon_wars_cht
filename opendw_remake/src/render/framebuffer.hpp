// framebuffer — 320×200 indexed(每像素一個 0–15 調色盤索引),遊戲原生解析度。
//
// Deep module:對外露 indexed buffer + DOS 16 色盤 + PPM 輸出;SDL2 視窗層(R2 後段)
// 只是把這個 buffer 放大顯示。
#pragma once
#include <array>
#include <cstdint>
#include <cstdio>

namespace dw::render {

inline constexpr int kW = 320;
inline constexpr int kH = 200;

struct Rgb { std::uint8_t r, g, b; };

// DOS 16 色標準調色盤。
inline constexpr std::array<Rgb, 16> kDosPalette = {{
  {0x00,0x00,0x00},{0x00,0x00,0xAA},{0x00,0xAA,0x00},{0x00,0xAA,0xAA},
  {0xAA,0x00,0x00},{0xAA,0x00,0xAA},{0xAA,0x55,0x00},{0xAA,0xAA,0xAA},
  {0x55,0x55,0x55},{0x55,0x55,0xFF},{0x55,0xFF,0x55},{0x55,0xFF,0xFF},
  {0xFF,0x55,0x55},{0xFF,0x55,0xFF},{0xFF,0xFF,0x55},{0xFF,0xFF,0xFF}}};

struct Framebuffer {
  std::array<std::uint8_t, kW * kH> idx{};  // 每像素 0–15

  void clear(std::uint8_t color = 0) { idx.fill(color); }
  void put(int x, int y, std::uint8_t color) {
    if (x >= 0 && x < kW && y >= 0 && y < kH) idx[y * kW + x] = color & 0x0F;
  }

  // 輸出 P6 PPM(RGB),供 golden 對拍。
  void write_ppm(std::FILE* f) const {
    std::fprintf(f, "P6\n%d %d\n255\n", kW, kH);
    for (std::uint8_t i : idx) {
      const Rgb& c = kDosPalette[i & 0x0F];
      std::fputc(c.r, f); std::fputc(c.g, f); std::fputc(c.b, f);
    }
  }
};

}  // namespace dw::render
