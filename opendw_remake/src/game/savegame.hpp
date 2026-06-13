// savegame — 自包含、版本化的存檔/讀檔(對齊原版手冊 S=儲存遊戲 / C=繼續舊遊戲)。
//
// Deep module:對外只露 SaveState(純資料) + save()/load()(讀寫一個 .sav 檔)。
// 內部隱藏二進位佈局(魔數 + 版本 + 各欄位定長序列化)。
//
// 存檔內容(完全還原所需的最小集合):
//   - 玩家位置/朝向/所在區域:area(int32)、x(int32)、y(int32)、facing(int32)
//   - 完整 VM 遊戲狀態:game_state[256]
//   - 隊伍 records:N × 512-byte 原始 record(對拍 opendw player_record;結構由 game::Party 解析)
//
// 設計取捨:
//   - 所有整數以 little-endian 定長序列化(跨機器確定性)。
//   - party records 直接存「原始 512B bytes」而非解析後欄位 → 存→讀→存 byte-for-byte 一致,
//     且未來 record 欄位擴充不影響存檔格式。
//   - 自包含:不依賴 DATA1/bundle 以外的任何外部狀態即可還原(area 重載走 bundle/maps/<area>.lvl)。
#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace dw::game {

// 一份存檔的完整可還原狀態(純資料,POD-like)。
struct SaveState {
  std::int32_t area = -1;     // 所在區域(level = bundle/maps/<area>.lvl;level_res = area+0x46)
  std::int32_t x = 0;         // 玩家格座標 x
  std::int32_t y = 0;         // 玩家格座標 y
  std::int32_t facing = 0;    // 朝向(0=N,1=E,2=S,3=W)

  std::array<std::uint8_t, 256> game_state{};  // 完整 VM game_state

  // 隊伍:每名角色一份 512-byte 原始 record(對拍 opendw player_record)。
  std::vector<std::array<std::uint8_t, 512>> party_records;
};

// 存檔魔數 + 版本(寫在檔頭;load 嚴格校驗)。
inline constexpr char     kSaveMagic[4] = {'D', 'W', 'S', 'V'};  // Dragon Wars SaVe
inline constexpr std::uint16_t kSaveVersion = 1;

// 把 SaveState 序列化寫到 path(會自動建立上層目錄)。回傳是否成功。
bool save(const SaveState& st, const std::filesystem::path& path);

// 從 path 讀回 SaveState(校驗魔數/版本/欄位完整性)。失敗回傳 false 且不改 out。
bool load(const std::filesystem::path& path, SaveState& out);

}  // namespace dw::game
