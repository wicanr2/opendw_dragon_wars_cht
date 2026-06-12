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

}  // namespace dw::render
