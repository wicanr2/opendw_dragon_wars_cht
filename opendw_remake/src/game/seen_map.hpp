// seen_map — 俯視平面地圖 (`?` automap) 的 fog of war:逐關 (per-area) 累積
// 「已看過 (seen)」的格。
//
// Deep module:對外只露「標記玩家當前格 seen」+「查某格是否 seen」+
// 「序列化/反序列化」。內部隱藏 per-area bitmap 容器與索引。
//
// 對齊 oracle (opendw refresh_viewport, engine.c:5680-5689):
//   每次 refresh_viewport 只把「玩家當前格」標記為 seen
//   (data_5521[word_551F+1] |= 0x8;word_551F = 該格 tile record 在關卡 bytes
//    的 byte offset = 3*y + data_5A04[x+1])。
//   ── 不是整個 FOV 視野,只有玩家站的那一格。FOV 取樣 (data_5A56) 只供渲染,
//      不寫 seen 位元。
//
// remake 不就地改 .lvl bytes (Level 是只讀資產,每次進關重載),而是維護一份
// 與 (area, x, y) 對應的獨立 seen bitmap;automap 渲染時據此決定畫不畫該格。
// 換 area → 各關 seen 獨立;存檔 → seen 一併保存,讀回後探索進度還原。
#pragma once

#include <cstdint>
#include <map>
#include <vector>

namespace dw::game {

class SeenMap {
public:
  // 把 (area, x, y) 的格標記為 seen。x/y 越界則忽略 (對齊 oracle:boundary
  // 命中時 cf=1,不設位元)。w/h = 該關尺寸 (Level::w / Level::h)。
  void mark(int area, int x, int y, int w, int h) {
    if (x < 0 || y < 0 || x >= w || y >= h) return;
    auto& bm = bitmap_for(area, w, h);
    bm[(std::size_t)y * (std::size_t)w + (std::size_t)x] = 1;
  }

  // 查 (area, x, y) 是否 seen。
  bool seen(int area, int x, int y, int w, int h) const {
    if (x < 0 || y < 0 || x >= w || y >= h) return false;
    auto it = areas_.find(area);
    if (it == areas_.end()) return false;
    const auto& bm = it->second;
    std::size_t idx = (std::size_t)y * (std::size_t)w + (std::size_t)x;
    return idx < bm.size() && bm[idx] != 0;
  }

  // 取某關的 seen bitmap (row-major, w*h bytes;0/1)。不存在回 nullptr。
  const std::vector<std::uint8_t>* bitmap(int area) const {
    auto it = areas_.find(area);
    return it == areas_.end() ? nullptr : &it->second;
  }

  // 序列化所有 area 的 seen bitmap (供存檔)。格式:
  //   u16 area_count;
  //   repeat: i32 area, u16 w, u16 h, (w*h) bytes bitmap。
  std::vector<std::uint8_t> serialize() const;

  // 從 serialize() 的 bytes 還原 (失敗回 false 且不改本物件)。
  bool deserialize(const std::uint8_t* data, std::size_t len);

  void clear() { areas_.clear(); }
  bool empty() const { return areas_.empty(); }

private:
  // area → row-major (w*h) bitmap;w/h 由首次 mark 固定。
  std::map<int, std::vector<std::uint8_t>> areas_;
  std::map<int, std::pair<int, int>> dims_;  // area → (w,h)

  std::vector<std::uint8_t>& bitmap_for(int area, int w, int h) {
    auto it = areas_.find(area);
    if (it == areas_.end()) {
      dims_[area] = {w, h};
      auto& bm = areas_[area];
      bm.assign((std::size_t)w * (std::size_t)h, 0);
      return bm;
    }
    return it->second;
  }
};

}  // namespace dw::game
