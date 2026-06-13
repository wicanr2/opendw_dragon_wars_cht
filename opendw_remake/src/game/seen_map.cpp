#include "seen_map.hpp"

#include <cstring>

namespace dw::game {

namespace {
void put_u16(std::vector<std::uint8_t>& b, std::uint16_t v) {
  b.push_back((std::uint8_t)(v & 0xFF));
  b.push_back((std::uint8_t)((v >> 8) & 0xFF));
}
void put_i32(std::vector<std::uint8_t>& b, std::int32_t v) {
  auto u = (std::uint32_t)v;
  b.push_back((std::uint8_t)(u & 0xFF));
  b.push_back((std::uint8_t)((u >> 8) & 0xFF));
  b.push_back((std::uint8_t)((u >> 16) & 0xFF));
  b.push_back((std::uint8_t)((u >> 24) & 0xFF));
}
}  // namespace

std::vector<std::uint8_t> SeenMap::serialize() const {
  std::vector<std::uint8_t> b;
  put_u16(b, (std::uint16_t)areas_.size());
  for (const auto& [area, bm] : areas_) {
    auto dit = dims_.find(area);
    int w = (dit != dims_.end()) ? dit->second.first : 0;
    int h = (dit != dims_.end()) ? dit->second.second : 0;
    put_i32(b, area);
    put_u16(b, (std::uint16_t)w);
    put_u16(b, (std::uint16_t)h);
    b.insert(b.end(), bm.begin(), bm.end());
  }
  return b;
}

bool SeenMap::deserialize(const std::uint8_t* data, std::size_t len) {
  std::size_t i = 0;
  auto need = [&](std::size_t k) { return i + k <= len; };
  auto get_u16 = [&](std::uint16_t& v) {
    if (!need(2)) return false;
    v = (std::uint16_t)(data[i] | (data[i + 1] << 8));
    i += 2;
    return true;
  };
  auto get_i32 = [&](std::int32_t& v) {
    if (!need(4)) return false;
    v = (std::int32_t)((std::uint32_t)data[i] | ((std::uint32_t)data[i + 1] << 8) |
                       ((std::uint32_t)data[i + 2] << 16) |
                       ((std::uint32_t)data[i + 3] << 24));
    i += 4;
    return true;
  };

  std::uint16_t count;
  if (!get_u16(count)) return false;

  std::map<int, std::vector<std::uint8_t>> areas;
  std::map<int, std::pair<int, int>> dims;
  for (std::uint16_t c = 0; c < count; ++c) {
    std::int32_t area;
    std::uint16_t w, h;
    if (!get_i32(area) || !get_u16(w) || !get_u16(h)) return false;
    std::size_t n = (std::size_t)w * (std::size_t)h;
    if (!need(n)) return false;
    std::vector<std::uint8_t> bm(n);
    if (n) std::memcpy(bm.data(), data + i, n);
    i += n;
    areas[area] = std::move(bm);
    dims[area] = {(int)w, (int)h};
  }
  areas_ = std::move(areas);
  dims_ = std::move(dims);
  return true;
}

}  // namespace dw::game
