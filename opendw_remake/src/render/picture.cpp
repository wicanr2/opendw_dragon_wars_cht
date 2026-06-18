#include "picture.hpp"

#include <vector>

namespace dw::render {

void decode_fullscreen(Framebuffer& fb, std::span<const std::uint8_t> data) {
  // title_adjust:垂直 XOR delta 還原(對照 opendw main.c,in-place)。
  std::vector<std::uint8_t> buf(data.begin(), data.end());
  std::size_t src = 0, dst = 0xA0;
  for (int i = 0; i < 0x3E30; ++i) {
    if (src + 0x9F >= buf.size() || dst + 1 >= buf.size()) break;
    std::uint16_t ax = buf[src] | (buf[src + 1] << 8);
    src += 2;
    ax ^= buf[src + 0x9E] | (buf[src + 0x9F] << 8);
    buf[dst] = ax & 0xFF;
    buf[dst + 1] = (ax >> 8) & 0xFF;
    dst += 2;
  }
  // nibble → 像素(每 byte 高低各一像素)。
  std::size_t p = 0;
  for (int y = 0; y < kH; ++y) {
    for (int x = 0; x < kW; x += 2) {
      std::uint8_t b = p < buf.size() ? buf[p] : 0;
      ++p;
      fb.put(x, y, (b >> 4) & 0xF);
      fb.put(x + 1, y, b & 0xF);
    }
  }
}

std::array<Rgb, 16> read_amiga_palette(std::span<const std::uint8_t> data) {
  std::array<Rgb, 16> pal{};
  for (int i = 0; i < 16; ++i) {
    std::size_t o = static_cast<std::size_t>(i) * 2;
    if (o + 1 >= data.size()) { pal[i] = {0, 0, 0}; continue; }
    std::uint16_t w = (static_cast<std::uint16_t>(data[o]) << 8) | data[o + 1];
    // 12-bit 0x0RGB:每 nibble ×17 把 0–15 線性展成 0–255(對齊 amiga_pic_extract.py)。
    std::uint8_t r = static_cast<std::uint8_t>(((w >> 8) & 0xF) * 17);
    std::uint8_t g = static_cast<std::uint8_t>(((w >> 4) & 0xF) * 17);
    std::uint8_t b = static_cast<std::uint8_t>((w & 0xF) * 17);
    pal[i] = {r, g, b};
  }
  return pal;
}

void decode_amiga_planar(Framebuffer& fb, std::span<const std::uint8_t> data) {
  constexpr int kPlanes = 4;
  constexpr std::size_t kDataOff = 32;          // 跳過 palette
  const int bpr = kW / 8;                        // 每列 bytes(每 plane)
  const std::size_t plane_size = static_cast<std::size_t>(bpr) * kH;
  for (int y = 0; y < kH; ++y) {
    for (int x = 0; x < kW; ++x) {
      const std::size_t bi = static_cast<std::size_t>(y) * bpr + x / 8;
      const int bit = 7 - (x % 8);
      std::uint8_t val = 0;
      for (int pl = 0; pl < kPlanes; ++pl) {
        std::size_t o = kDataOff + static_cast<std::size_t>(pl) * plane_size + bi;
        if (o < data.size()) val |= static_cast<std::uint8_t>(((data[o] >> bit) & 1) << pl);
      }
      fb.put(x, y, val);
    }
  }
}

}  // namespace dw::render
