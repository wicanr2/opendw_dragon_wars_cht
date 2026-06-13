#include "cjk_font.hpp"

#include <cstdio>
#include <cstring>
#include <vector>

namespace dw::render {

std::optional<CjkFont> CjkFont::load(const std::filesystem::path& atlas) {
  std::FILE* f = std::fopen(atlas.string().c_str(), "rb");
  if (!f) return std::nullopt;
  char magic[4];
  std::uint32_t count;
  if (std::fread(magic, 1, 4, f) != 4 || std::memcmp(magic, "CJK1", 4) != 0 ||
      std::fread(&count, 4, 1, f) != 1) { std::fclose(f); return std::nullopt; }
  CjkFont font;
  for (std::uint32_t i = 0; i < count; ++i) {
    std::uint32_t cp;
    std::array<std::uint8_t, 72> g{};
    if (std::fread(&cp, 4, 1, f) != 1 || std::fread(g.data(), 1, 72, f) != 72) {
      std::fclose(f); return std::nullopt;
    }
    font.glyphs_[cp] = g;
  }
  std::fclose(f);
  return font;
}

bool CjkFont::draw(Framebuffer& fb, int px, int py, std::uint32_t cp, std::uint8_t color) const {
  auto it = glyphs_.find(cp);
  if (it == glyphs_.end()) return false;
  const auto& g = it->second;
  for (int y = 0; y < 24; ++y)
    for (int x = 0; x < 24; ++x) {
      std::uint8_t byte = g[y * 3 + x / 8];
      if (byte & (0x80 >> (x % 8))) fb.put(px + x, py + y, color);
    }
  return true;
}

bool CjkFont::draw_half(Framebuffer& fb, int px, int py, std::uint32_t cp, std::uint8_t color) const {
  auto it = glyphs_.find(cp);
  if (it == glyphs_.end()) return false;
  const auto& g = it->second;
  auto on = [&](int x, int y) -> bool {
    std::uint8_t byte = g[y * 3 + x / 8];
    return (byte & (0x80 >> (x % 8))) != 0;
  };
  for (int y = 0; y < 12; ++y)
    for (int x = 0; x < 12; ++x) {
      int sx = x * 2, sy = y * 2;  // 2×2 區塊 OR 降採樣
      if (on(sx, sy) || on(sx + 1, sy) || on(sx, sy + 1) || on(sx + 1, sy + 1))
        fb.put(px + x, py + y, color);
    }
  return true;
}

std::uint32_t utf8_next(const char*& p) {
  std::uint8_t c = static_cast<std::uint8_t>(*p++);
  if (c < 0x80) return c;
  if ((c >> 5) == 0x6) {
    std::uint32_t cp = (c & 0x1F) << 6;
    cp |= (static_cast<std::uint8_t>(*p++) & 0x3F);
    return cp;
  }
  if ((c >> 4) == 0xE) {
    std::uint32_t cp = (c & 0x0F) << 12;
    cp |= (static_cast<std::uint8_t>(*p++) & 0x3F) << 6;
    cp |= (static_cast<std::uint8_t>(*p++) & 0x3F);
    return cp;
  }
  if ((c >> 3) == 0x1E) {
    std::uint32_t cp = (c & 0x07) << 18;
    cp |= (static_cast<std::uint8_t>(*p++) & 0x3F) << 12;
    cp |= (static_cast<std::uint8_t>(*p++) & 0x3F) << 6;
    cp |= (static_cast<std::uint8_t>(*p++) & 0x3F);
    return cp;
  }
  return c;
}

}  // namespace dw::render
