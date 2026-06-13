#include "font.hpp"

#include <cstdio>

namespace dw::render {

std::optional<Font8x8> Font8x8::load(const std::filesystem::path& dragon_com) {
  std::FILE* f = std::fopen(dragon_com.string().c_str(), "rb");
  if (!f) return std::nullopt;
  // chr_table:com_extract(0xBF52) - COM_ORG_START(0x100) = 檔案 0xBE52
  if (std::fseek(f, 0xBF52 - 0x100, SEEK_SET) != 0) { std::fclose(f); return std::nullopt; }
  Font8x8 font;
  std::size_t n = std::fread(font.table_.data(), 1, font.table_.size(), f);
  std::fclose(f);
  if (n != font.table_.size()) return std::nullopt;
  return font;
}

std::optional<Font8x8> Font8x8::load_table(const std::filesystem::path& raw) {
  std::FILE* f = std::fopen(raw.string().c_str(), "rb");
  if (!f) return std::nullopt;
  Font8x8 font;
  std::size_t n = std::fread(font.table_.data(), 1, font.table_.size(), f);
  std::fclose(f);
  if (n != font.table_.size()) return std::nullopt;
  return font;
}

void Font8x8::draw_char(Framebuffer& fb, int px, int py, std::uint8_t ch,
                        std::uint8_t fg, std::uint8_t bg) const {
  const std::uint8_t* g = &table_[(ch & 0x7F) * 8];
  for (int j = 0; j < 8; ++j) {
    std::uint8_t al = g[j];
    for (int i = 0; i < 8; ++i) {
      std::uint8_t bl = (al << 1) & 0xFF;     // MSB-first(對照 draw_character)
      fb.put(px + i, py + j, bl < al ? fg : bg);
      al = bl;
    }
  }
}

void Font8x8::draw_string(Framebuffer& fb, int px, int py, const char* s,
                          std::uint8_t fg, std::uint8_t bg) const {
  for (int x = px; *s; ++s, x += 8)
    draw_char(fb, x, py, static_cast<std::uint8_t>(*s), fg, bg);
}

}  // namespace dw::render
