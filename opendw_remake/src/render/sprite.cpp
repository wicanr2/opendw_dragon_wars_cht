#include "sprite.hpp"

#include <cstdio>
#include <cstring>

namespace dw::render {

std::optional<Sprite> Sprite::load(const std::filesystem::path& path) {
  std::FILE* f = std::fopen(path.string().c_str(), "rb");
  if (!f) return std::nullopt;
  char magic[4];
  std::uint16_t w, h;
  std::uint8_t paln;
  if (std::fread(magic, 1, 4, f) != 4 || std::memcmp(magic, "DWSP", 4) != 0 ||
      std::fread(&w, 2, 1, f) != 1 || std::fread(&h, 2, 1, f) != 1 ||
      std::fread(&paln, 1, 1, f) != 1) { std::fclose(f); return std::nullopt; }
  Sprite s;
  s.w = w; s.h = h;
  s.palette.resize(paln);
  for (auto& c : s.palette) {
    std::uint8_t rgb[3];
    if (std::fread(rgb, 1, 3, f) != 3) { std::fclose(f); return std::nullopt; }
    c = {rgb[0], rgb[1], rgb[2]};
  }
  s.idx.resize(static_cast<std::size_t>(w) * h);
  if (std::fread(s.idx.data(), 1, s.idx.size(), f) != s.idx.size()) {
    std::fclose(f); return std::nullopt;
  }
  std::fclose(f);
  return s;
}

void Sprite::blit(Framebuffer& fb, int px, int py, int transparent) const {
  // DOS sprite:索引即 framebuffer 調色盤索引(兩者皆 DOS 16 色),直接畫。
  // (X68000/PC-9801 自有調色盤 → remaster 模式另需 palette 對映,屬後續工作。)
  for (int y = 0; y < h; ++y)
    for (int x = 0; x < w; ++x) {
      std::uint8_t i = idx[static_cast<std::size_t>(y) * w + x];
      if (transparent >= 0 && i == transparent) continue;
      fb.put(px + x, py + y, i);
    }
}

}  // namespace dw::render
