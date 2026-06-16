// recruit — 酒館招募 NPC(讓隊伍可構築:把名角加進隊伍)。
//
// Deep module:對外只露「可招募 NPC 名單」+「招募一名 NPC 進隊伍」窄介面;內部隱藏
//   512B NPC record 組裝、NPC 識別碼 [77] gate(防重複招募)、隊伍上限管理。
//   **不重算戰鬥 / 成長公式**(那是 combat.hpp / progression.hpp 的範疇)。
//
// 純資料 + 規則,無 SDL/render 相依 → 可獨立進 ctest(tools/verify/verify_recruit.cpp)。
//
// ── grounded 來源 vs remake 設計(誠實標示)──────────────────────────────────
//   [grounded,fraterrisus / docs44 §1]:NPC 識別碼存於角色 record [77]。
//       可招募 NPC 與其識別碼(brief / fraterrisus NPC identifier 表):
//         Ulrik=0x01、Louie=0x03、Valar=0x04、Halifax=0x05。
//       512B record 佈局(名 [00-11]、屬性、HP/STUN/PWR、level、identifier[77])
//       = fraterrisus 格式(與 party.cpp parse_record / chargen.serialize 一致)。
//   [remake 設計,明標——招募邏輯 + NPC 屬性 opendw C 未實作,非原版 byte-for-byte]:
//       • NPC 各項屬性 / HP / 技能 = curated 平衡值(見 recruit.cpp 模板;非萃取自
//         DATA1 真實 NPC blob,故誠實標為 remake 設計)。名字 grounded(fraterrisus)。
//       • 隊伍上限 7 槽(docs/44 §1:最多 7 員 record;前 4 主戰、NPC 佔 slot 4-6)。
//         招募 → append 到隊伍尾(若 < 7);滿 7 → 擋下。
//       • identifier[77] gate:隊伍中已有同 identifier 的成員 → 不可重複招募。
//       • 招募點(酒館)由遊戲層(main.cpp / 事件)決定;本模組只管「名單 + 招募動作」。
#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "game/party.hpp"

namespace dw::game {

// 隊伍上限(docs/44 §1:最多 7 員 record)。
inline constexpr int kMaxPartyMembers = 7;

// 可招募的 NPC 模板(名 + 識別碼 + curated 屬性)。
struct NpcTemplate {
  std::uint8_t identifier = 0;   // [77] NPC 識別碼(grounded;0x01/0x03/0x04/0x05)
  std::string name;              // 顯示名(grounded,fraterrisus)
  // curated 屬性(remake 設計;見檔頭)。
  std::uint8_t strength = 0, dexterity = 0, intel = 0, spirit = 0;
  std::uint16_t health = 0, stun = 0, power = 0;
  std::uint8_t level = 1;
  std::uint8_t gender = 0;        // 0=男 1=女
  // 序列化成合法 512B record(fraterrisus 佈局;identifier 寫 [77])。
  std::array<std::uint8_t, 512> serialize() const;
};

// 招募結果(供 UI 提示 + 驗證)。
struct RecruitResult {
  bool ok = false;
  const char* reason = "";  // 失敗原因鍵(i18n):
                            //   "recruit_already" / "recruit_full" / "recruit_unknown"
  int party_size_after = 0;
};

// 招募名冊:內建可招募 NPC 模板表(grounded 名 + curated 屬性)。
class RecruitRoster {
public:
  // 取得內建可招募 NPC 名單(固定;見 recruit.cpp)。
  static const std::vector<NpcTemplate>& roster();

  // 依 identifier 找模板;無則回 nullptr。
  static const NpcTemplate* find(std::uint8_t identifier);
};

// 隊伍中是否已有某 identifier 的成員(掃每員 raw[77])。
bool party_has_npc(const Party& party, std::uint8_t identifier);

// 把指定 identifier 的 NPC 招募進隊伍:
//   identifier 不在名冊 → reason="recruit_unknown";
//   隊伍已有同 identifier → reason="recruit_already";
//   隊伍滿 7 員 → reason="recruit_full";
//   否則 append 一名新 NPC(512B record,identifier 寫入 [77])到隊伍尾。
// 隊伍以 raw_records()/from_raw_records() round-trip(存檔相容)。
RecruitResult recruit_npc(Party& party, std::uint8_t identifier);

}  // namespace dw::game
