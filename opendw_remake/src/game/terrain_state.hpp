// terrain_state — 探索互動狀態(門開啟 / 密門粉碎 / 陷阱解除 / 陷阱已觸發 /
// 陷阱已感知)的 per-area 座標旗標。
//
// Deep module:對外只露「標記 / 查詢某格某旗標」+「序列化/反序列化」。
// 內部隱藏 per-area 容器與旗標 bit 佈局。
//
// ── 真值層級(務必誠實,見 docs/57)─────────────────────────────────────
//   opendw 乾淨反編**無主遊戲 K 開門 handler、無陷阱結算**(門/陷阱在原版是
//   特殊事件格 word_11C8≥2,行為由未反編的 bytecode 決定)。本模組為 **remake
//   設計**:.lvl 為只讀資產(每次進關重載),不就地改 byte,改維護獨立的
//   (area,x,y)→旗標 狀態;存檔一併保存(同 SeenMap 模式)。grounded 手冊
//   (K=開門/破密門;Disarm Trap/Sense Traps/Soften Stone 法術)。
// ──────────────────────────────────────────────────────────────────────
#pragma once

#include <cstdint>
#include <map>
#include <vector>

namespace dw::game {

// 單格可帶的旗標(bitmask,可疊加)。
enum TerrainFlag : std::uint8_t {
  TF_DoorOpen     = 0x01,  // 門已開(關閉的門 0x30 / 鎖門 0x31 被 K 開啟)
  TF_SecretBroken = 0x02,  // 密門已粉碎(0x32)/ 石牆已軟化(0x34)→ 可走
  TF_TrapDisarmed = 0x04,  // 陷阱已解除(踩過不再觸發;Disarm Trap 法術或解除技能)
  TF_TrapSprung   = 0x08,  // 陷阱已觸發過(踩中扣血後標記,不重複扣)
  TF_TrapSensed   = 0x10,  // 陷阱已感知(Sense Traps;UI 顯示提示用)
  TF_WallPlaced   = 0x20,  // 已放置石牆障礙(Create Wall;原為可走的格 → 變不可走,直到離關重載)
};

class TerrainState {
public:
  // 設某格旗標(OR 進去)。x/y 越界忽略。
  void set(int area, int x, int y, std::uint8_t flag) {
    if (x < 0 || y < 0) return;
    cells_[key(area, x, y)] |= flag;
  }
  // 查某格是否帶某旗標(任一 bit)。
  bool has(int area, int x, int y, std::uint8_t flag) const {
    auto it = cells_.find(key(area, x, y));
    return it != cells_.end() && (it->second & flag) != 0;
  }
  // 取某格完整旗標(查無回 0)。
  std::uint8_t get(int area, int x, int y) const {
    auto it = cells_.find(key(area, x, y));
    return it == cells_.end() ? 0 : it->second;
  }

  // 序列化(供存檔)。格式:u32 count; repeat: i32 area, i32 x, i32 y, u8 flags。
  std::vector<std::uint8_t> serialize() const;
  // 從 serialize() 還原(失敗回 false 且不改本物件)。
  bool deserialize(const std::uint8_t* data, std::size_t len);

  void clear() { cells_.clear(); }
  bool empty() const { return cells_.empty(); }

private:
  // (area,x,y) 打包成 64-bit key:area 高 32 bit、x 16 bit、y 16 bit(座標 < 65536)。
  static std::uint64_t key(int area, int x, int y) {
    return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(area)) << 32) |
           (static_cast<std::uint64_t>(x & 0xFFFF) << 16) |
           static_cast<std::uint64_t>(y & 0xFFFF);
  }
  std::map<std::uint64_t, std::uint8_t> cells_;
};

}  // namespace dw::game
