#include "terrain_state.hpp"

namespace dw::game {

namespace {
void put_i32(std::vector<std::uint8_t>& b, std::int32_t v) {
  auto u = (std::uint32_t)v;
  b.push_back((std::uint8_t)(u & 0xFF));
  b.push_back((std::uint8_t)((u >> 8) & 0xFF));
  b.push_back((std::uint8_t)((u >> 16) & 0xFF));
  b.push_back((std::uint8_t)((u >> 24) & 0xFF));
}
void put_u32(std::vector<std::uint8_t>& b, std::uint32_t v) {
  put_i32(b, (std::int32_t)v);
}
}  // namespace

std::vector<std::uint8_t> TerrainState::serialize() const {
  std::vector<std::uint8_t> b;
  put_u32(b, (std::uint32_t)cells_.size());
  for (const auto& [k, flags] : cells_) {
    int area = (int)(std::int32_t)(std::uint32_t)(k >> 32);
    int x = (int)((k >> 16) & 0xFFFF);
    int y = (int)(k & 0xFFFF);
    put_i32(b, area);
    put_i32(b, x);
    put_i32(b, y);
    b.push_back(flags);
  }
  return b;
}

bool TerrainState::deserialize(const std::uint8_t* data, std::size_t len) {
  std::size_t i = 0;
  auto need = [&](std::size_t k) { return i + k <= len; };
  auto get_i32 = [&](std::int32_t& v) {
    if (!need(4)) return false;
    v = (std::int32_t)((std::uint32_t)data[i] | ((std::uint32_t)data[i + 1] << 8) |
                       ((std::uint32_t)data[i + 2] << 16) |
                       ((std::uint32_t)data[i + 3] << 24));
    i += 4;
    return true;
  };

  std::int32_t count32;
  if (!get_i32(count32)) return false;
  std::uint32_t count = (std::uint32_t)count32;

  std::map<std::uint64_t, std::uint8_t> cells;
  for (std::uint32_t c = 0; c < count; ++c) {
    std::int32_t area, x, y;
    if (!get_i32(area) || !get_i32(x) || !get_i32(y)) return false;
    if (!need(1)) return false;
    std::uint8_t flags = data[i++];
    cells[key(area, x, y)] = flags;
  }
  cells_ = std::move(cells);
  return true;
}

}  // namespace dw::game
