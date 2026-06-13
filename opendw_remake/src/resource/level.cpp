#include "level.hpp"

#include <cstdio>
#include "text_codec.hpp"

namespace dw::res {

static std::size_t skip_until_high(const std::vector<std::uint8_t>& b, std::size_t p) {
  while (p < b.size() && b[p] < 0x80) ++p;
  return p + 1;  // 含終止 byte(≥0x80)
}

std::optional<Level> Level::from_bytes(std::vector<std::uint8_t> bytes) {
  if (bytes.size() < 8) return std::nullopt;
  Level L;
  L.b_ = std::move(bytes);
  const auto& b = L.b_;
  L.h = b[0]; L.w = b[1]; L.flags = b[2];
  std::size_t p = 4;
  p = skip_until_high(b, p);                                  // data_5897
  while (p + 1 < b.size()) { std::uint8_t f = b[p]; p += 2; if (f >= 0x80) break; }  // data_56C6 pairs
  for (int k = 0; k < 3; ++k) p = skip_until_high(b, p);      // 3× 元件清單
  if (p + 1 >= b.size()) return std::nullopt;
  std::uint16_t name_off = b[p] | (b[p + 1] << 8); p += 2;
  L.grid_ = p;
  if (name_off < b.size()) L.name = dw::text::decode(b, name_off).first;
  return L;
}

std::optional<Level> Level::load_file(const std::filesystem::path& lvl) {
  std::FILE* f = std::fopen(lvl.string().c_str(), "rb");
  if (!f) return std::nullopt;
  std::fseek(f, 0, SEEK_END); long n = std::ftell(f); std::fseek(f, 0, SEEK_SET);
  if (n <= 0) { std::fclose(f); return std::nullopt; }
  std::vector<std::uint8_t> bytes((std::size_t)n);
  std::size_t got = std::fread(bytes.data(), 1, bytes.size(), f);
  std::fclose(f);
  if (got != bytes.size()) return std::nullopt;
  return from_bytes(std::move(bytes));
}

std::uint8_t Level::tile(int x, int y) const {
  std::size_t o = off(x, y);
  return (o + 2 < b_.size()) ? b_[o + 2] : 0;
}

std::uint16_t Level::wall(int x, int y) const {
  std::size_t o = off(x, y);
  return (o + 1 < b_.size()) ? (std::uint16_t)(b_[o] | (b_[o + 1] << 8)) : 0;
}

}  // namespace dw::res
