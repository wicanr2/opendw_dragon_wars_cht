// level — 解析原版關卡(.lvl,= resource_load(area+0x46) 的解壓資料)。
//
// 依 opendw read_level_metadata:前 4 byte = 高/寬/旗標 + 變長 section + 關卡名 offset
// + tile 格(column-major 反序,每格 3 byte:word_11C6 牆屬性 + word_11C8 tile 型/事件)。
// tile 型(word_11C8):0=void/牆、1=可走地面、2..F=特殊格(水/建築/事件/門…)。
#pragma once
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace dw::res {

class Level {
public:
  int h = 0, w = 0, flags = 0;
  std::string name;

  static std::optional<Level> load_file(const std::filesystem::path& lvl);   // 讀 .lvl
  static std::optional<Level> from_bytes(std::vector<std::uint8_t> bytes);    // 直接給 bytes

  // tile 型(word_11C8 第 3 byte):0=void、1=地面、其他=特殊/事件。
  std::uint8_t tile(int x, int y) const;
  // 牆屬性(word_11C6,2 byte)。
  std::uint16_t wall(int x, int y) const;
  bool in_bounds(int x, int y) const { return x >= 0 && y >= 0 && x < w && y < h; }
  bool walkable(int x, int y) const { return in_bounds(x, y) && tile(x, y) != 0; }

private:
  std::vector<std::uint8_t> b_;
  std::size_t grid_ = 0;
  std::size_t off(int x, int y) const {
    return grid_ + (std::size_t)(w - 1 - x) * 3 * h + (std::size_t)y * 3;
  }
};

}  // namespace dw::res
