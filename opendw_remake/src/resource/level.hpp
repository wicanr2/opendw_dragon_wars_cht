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

  // ── wrap 邊界(.lvl flags bit1 = 0x2)────────────────────────────────────
  // opendw `check_map_boundary_x/y`(engine.c:5147/5176)在「座標越界 && flag&2」
  // 時走 `exit(1)`(未實作)。前後文(非 wrap 分支對 x>=W 夾到 W-1、x<0 夾到 0,
  // 並設 blocked 旗標)透露 wrap 分支本應做「modular 環繞」:走出東緣→西緣、
  // 走出南緣→北緣。此處以該環繞慣例補上(誠實標示:opendw oracle 未實作)。
  bool wraps() const { return (flags & 0x2) != 0; }
  // 把座標折回有效範圍(僅 wrap 關卡;非 wrap 關卡原樣回傳,由呼叫端 in_bounds 判定)。
  int wrap_x(int x) const { return (w > 0) ? ((x % w) + w) % w : x; }
  int wrap_y(int y) const { return (h > 0) ? ((y % h) + h) % h : y; }
  // wrap 關卡:座標先 modular 環繞再查 tile(永遠在界內,故可走 = tile!=0)。
  // 非 wrap 關卡:退回一般 walkable(越界即不可走)。
  bool walkable_wrap(int x, int y) const {
    if (!wraps()) return walkable(x, y);
    return tile(wrap_x(x), wrap_y(y)) != 0;
  }

  // 關卡 bytecode(level 資源本身也是 script);供 VM 執行事件腳本。
  const std::vector<std::uint8_t>& data() const { return b_; }
  // tile 格起點 offset(= read_level_metadata 解析完 header 後的 di;= data_5A04 基準)。
  std::size_t grid() const { return grid_; }
  // 原始 byte 存取(供 viewport_compose port 直接模擬 data_5521[di])。
  std::uint8_t byte_at(std::size_t i) const { return i < b_.size() ? b_[i] : 0; }
  std::size_t size() const { return b_.size(); }
  // 特殊格事件腳本入口(對拍 opendw op_71/run_level_script):
  //   script 表起點 = grid + w*3*h(= data_5A04[0]);entry = base + (tile_value+1)*2;
  //   該處 16-bit = script PC(level bytecode 內)。
  std::size_t script_table() const { return grid_ + (std::size_t)w * 3 * h; }
  std::uint16_t script_pc(std::uint8_t tile_value) const {
    std::size_t e = script_table() + (std::size_t)(tile_value + 1) * 2;
    return (e + 1 < b_.size()) ? (std::uint16_t)(b_[e] | (b_[e + 1] << 8)) : 0;
  }

private:
  std::vector<std::uint8_t> b_;
  std::size_t grid_ = 0;
  std::size_t off(int x, int y) const {
    return grid_ + (std::size_t)(w - 1 - x) * 3 * h + (std::size_t)y * 3;
  }
};

}  // namespace dw::res
