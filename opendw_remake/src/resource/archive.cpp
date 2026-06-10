#include "archive.hpp"

#include <cstdio>
#include <cstring>

namespace dw::res {
namespace {

std::optional<std::vector<std::uint8_t>> read_file(const std::filesystem::path& p) {
  std::FILE* f = std::fopen(p.string().c_str(), "rb");
  if (!f) return std::nullopt;
  std::fseek(f, 0, SEEK_END);
  long n = std::ftell(f);
  std::fseek(f, 0, SEEK_SET);
  std::vector<std::uint8_t> buf(n > 0 ? static_cast<std::size_t>(n) : 0);
  if (!buf.empty() && std::fread(buf.data(), 1, buf.size(), f) != buf.size()) {
    std::fclose(f);
    return std::nullopt;
  }
  std::fclose(f);
  return buf;
}

void parse_header(const std::vector<std::uint8_t>& file,
                  std::array<std::uint16_t, 384>& sizes) {
  for (std::size_t i = 0; i < 384 && (i * 2 + 1) < file.size(); ++i)
    sizes[i] = static_cast<std::uint16_t>(file[i * 2] | (file[i * 2 + 1] << 8));
}

constexpr std::size_t kHeaderBytes = 768;
constexpr std::uint16_t kAbsent = 0xFF00;  // header >= 0xFF00 → 不在此檔

}  // namespace

std::optional<Archive> Archive::open(const std::filesystem::path& dir) {
  auto d1 = read_file(dir / "data1");
  if (!d1 || d1->size() < kHeaderBytes) return std::nullopt;
  Archive a;
  a.data1_ = std::move(*d1);
  parse_header(a.data1_, a.hdr1_.sizes);
  if (auto d2 = read_file(dir / "data2"); d2 && d2->size() >= kHeaderBytes) {
    a.data2_ = std::move(*d2);
    parse_header(a.data2_, a.hdr2_.sizes);
    a.has_data2_ = true;
  }
  return a;
}

// 決定 resource 在哪個檔、offset、size。內建 DATA2 fallback。
std::optional<Archive::Located> Archive::locate(ResourceId id) const {
  if (id < 0 || id >= 384) return std::nullopt;
  const bool in_data2 = hdr1_.sizes[id] >= kAbsent && has_data2_;
  const auto& sizes = in_data2 ? hdr2_.sizes : hdr1_.sizes;
  const auto& file = in_data2 ? data2_ : data1_;
  if (sizes[id] >= kAbsent) return std::nullopt;  // 兩檔皆無
  std::size_t off = kHeaderBytes;
  for (int i = 0; i < id; ++i)
    if (sizes[i] < kAbsent) off += sizes[i];
  return Located{&file, off, sizes[id]};
}

std::optional<std::vector<std::uint8_t>> Archive::load_raw(ResourceId id) const {
  auto loc = locate(id);
  if (!loc) return std::nullopt;
  if (loc->offset + loc->size > loc->file->size()) return std::nullopt;
  return std::vector<std::uint8_t>(loc->file->begin() + loc->offset,
                                   loc->file->begin() + loc->offset + loc->size);
}

std::optional<std::vector<std::uint8_t>> Archive::load(ResourceId id) const {
  auto raw = load_raw(id);
  if (!raw) return std::nullopt;
  if (id <= 0x17) return raw;  // 未壓縮 section 直接回傳
  // TODO(R0→R1): section > 0x17 需 decompress(對照 compress.c decompress_data1)。
  // 由 resource/decompress 模組提供;在 decompress 完成前先回 raw 並標記。
  return raw;
}

}  // namespace dw::res
