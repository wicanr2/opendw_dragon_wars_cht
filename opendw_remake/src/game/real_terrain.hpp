// real_terrain — 從真實 .lvl 識別「原版實際的陷阱格」(路徑 B)。
//
// ── 真值層級(誠實標示,見 docs/gameplay/57_DOORS_TRAPS_TERRAIN.md)─────────────────────────────────────
//   • 陷阱「位置」= 可識別(bytecode 真值):每個特殊事件格(word_11C8≥2)的
//     tile → script_pc → VM 跑出的 emit 字串已逐指令對拍 opendw(op_71/run_level_script)。
//     字串屬「對隊伍施加即時傷害 / 敵意環境」語意類者(攻略 docs/walkthrough/38 描述的陷阱:
//     尼塞山腹「floor moved / icy winds / 灼燒走廊」、矮人鑄爐耗命、魔法學院 tripwire
//     巨石)→ 該格 = 原版真實陷阱格。座標來自原版資料,非保留值臆造。
//   • 陷阱「傷害結算」= 受阻 → remake 設計:傷害 HP/Stun 實際扣減走 op_58 跨資源
//     呼叫 + 未反編的 settlement(opendw 乾淨反編無此 C 碼)。本模組只給「位置」。
//
//   取代 #135 的 0x30..0x34 純保留約定(真實 .lvl 從不含那些值)。門的真實格無法
//   由牆 sprite 乾淨識別(全 40 關牆 nibble 只選 5 個牆/岩 tag,無專屬門 sprite;
//   hilo 標記是重載的牆變體碼,area34 整間 64 格 hilo=1 是標準牆)→ 門位置受阻
//   (見 docs/gameplay/57_DOORS_TRAPS_TERRAIN.md §2 + render_fp_cell 目視證據),故本模組僅處理陷阱。
// ──────────────────────────────────────────────────────────────────────
#pragma once

#include <cstdint>
#include <set>
#include <utility>

#include "resource/level.hpp"

namespace dw::game {

// 某 area 的真實陷阱格集合(座標來自原版事件 script 行為)。
class RealTraps {
public:
  // 對 level(對應 area)逐事件格跑 VM,識別陷阱格。area 用於資源 index(area+0x46)。
  //   只在進關時呼叫一次(per-area 快取由呼叫端持有)。
  static RealTraps identify(const res::Level& level, int area);

  bool is_trap(int x, int y) const {
    return cells_.count(std::make_pair(x, y)) != 0;
  }
  std::size_t count() const { return cells_.size(); }
  const std::set<std::pair<int, int>>& cells() const { return cells_; }

private:
  std::set<std::pair<int, int>> cells_;
};

}  // namespace dw::game
