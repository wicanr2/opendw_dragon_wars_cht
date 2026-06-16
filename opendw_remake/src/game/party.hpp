// party — 隊伍角色資料 + 右側狀態面板渲染。
//
// Deep module:對外只露「載入預設 4 人隊伍」與「把面板畫進畫面」兩個窄介面;
// 內部隱藏 opendw player_record 512-byte 佈局、名字高位元終止編碼、
// 以及原版 draw_player_status_panel / draw_player_stat 的 VGA 座標移植。
//
// 來源對照(opendw,唯讀 oracle):
//   - player.c struct player_record(欄位 offset 見下)
//   - engine.c draw_player_status_panel(0x1A72)/draw_player_status(0x1ABD)/
//     draw_player_stat(0x1C0F? 實作於 6599)/write_character_name(0x1A40)
//   - game_state:gs[0x0A+i]=角色 record 選擇子(=record_index*2)、
//     gs[0x18+i]=在隊旗標(0=在)、gs[0x1F]=隊伍人數
//
// 像素層 vs 文字層分工(遵循專案雙層渲染規則):
//   - 角色列底框 / 三條 HP/暈眩/法力 狀態條 → 走 Framebuffer(像素層)
//   - 角色名字 → 走 TextLayer(TTF,可 i18n;角色名通常英文)
#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "../render/framebuffer.hpp"
#include "../render/text_layer.hpp"
#include "game/damage_dice.hpp"
#include "game/equipment.hpp"

namespace dw::game {

// EquipItem — 物品欄一格的「戰鬥精簡視圖」(回歸相容保留)。
// 完整 23B 解析改由 equipment.hpp 的 ItemInstance 負責;本結構僅保留戰鬥結算
// 與既有呼叫端用到的欄位,由 ItemInstance 投影而來(見 party.cpp from_item)。
struct EquipItem {
  bool present = false;       // 該欄是否有物品(非全 0)
  bool equipped = false;      // bit[00]
  int av_mod = 0;             // bit[24-27] AV 修正(bit[08]=1 時為負)
  int ac_mod = 0;             // bit[28-31] AC 修正(恆正)
  std::uint8_t item_type = 0; // bit[40-47] 低 5 bit(00 一般 / 03 斧 / 05 劍 …)
  DamageDice primary_dmg;     // bit[64-71] 主傷害骰(decode_damage_dice)
};

// opendw player_record / fraterrisus 512-byte 佈局關鍵欄位 offset。
struct CharacterRecord {
  std::string name;          // name[0..]:除末位元組外皆設高位元,末位元組高位元清除
  std::uint8_t strength = 0, max_strength = 0;       // 0x0C,0x0D = [12,13]
  std::uint8_t dexterity = 0, max_dexterity = 0;     // 0x0E,0x0F = [14,15]
  std::uint8_t intel = 0, max_intel = 0;             // 0x10,0x11 = [16,17]
  std::uint8_t spirit = 0, max_spirit = 0;           // 0x12,0x13 = [18,19]
  std::uint16_t health = 0, max_health = 0;          // 0x14,0x16 = [20,22]
  std::uint16_t stun = 0, max_stun = 0;              // 0x18,0x1A = [24,26]
  std::uint16_t power = 0, max_power = 0;             // 0x1C,0x1E = [28,30]

  // ── 戰鬥相關欄位(fraterrisus 512B 格式;本次擴充)──────────────────
  std::array<std::uint8_t, 27> skills{};  // [32-58] 每技能 1B = 等級(順序見 docs/44)
  std::array<std::uint8_t, 8> spells{};   // [60-67] 法術 bitfield(順序見 docs/44)
  std::uint8_t xp = 0;       // [80] 經驗值 XP
  std::uint8_t gold8 = 0;    // [81] 金幣(fraterrisus 記為 1B;見 gold 欄註)
  std::uint8_t av = 0;       // [82] AV 攻擊值(命中)— 注意:起始隊伍為 0,runtime 計算
  std::uint8_t dv = 0;       // [83] DV 防禦值(閃避)— 同上
  std::uint8_t ac = 0;       // [84] AC 護甲等級    — 同上
  std::uint8_t flags = 0;    // [85] 祝福旗標

  std::uint8_t status = 0;   // [76] bitfield:0x01 dead / 0x02 chained / 0x04 poisoned / 0x80 stunned
  std::uint8_t gender = 0;   // [78]
  std::uint16_t level = 0;   // [79]
  // 注意:remake 既有把 gold 讀為 0x55 起 4B(little-endian);fraterrisus 標 gold 在 [81] 1B。
  //       兩者來源不同(opendw player.c vs fraterrisus hex guide),起始隊伍此區皆 0,無法以資料判別。
  //       保留既有 0x55 4B 解析(回歸不變),另存 fraterrisus 的 gold8[81] 供對照。
  std::uint32_t gold = 0;    // 0x55(既有解析,回歸保留)

  std::array<std::uint8_t, 512> raw{};  // 完整原始 record

  // 物品欄 13 格 [236-511](每格 23B = 11B bit-packed + 12B 名)。
  // 來源:docs/44 §2(fraterrisus inventory slot A..M)。回傳全 13 格的完整解析
  // (空格 present=false)。供背包 UI / 裝備邏輯使用。
  static constexpr int kInventorySlots = 13;
  static constexpr int kInventoryBase = 236;
  static constexpr int kItemStride = 23;
  std::array<ItemInstance, kInventorySlots> inventory() const;
  // 第 slot 格(0..12)的完整解析;越界回傳空 ItemInstance。
  ItemInstance item_at(int slot) const;

  // 主武器 = 第一件「已裝備且為武器」的物品;無則回退第 0 格的精簡視圖。
  // 起始隊伍無裝備(全 0)→ present=false → 結算回退徒手。
  EquipItem main_weapon() const;

  // 戰鬥用「有效 AV/DV」:stored 欄為 0 時改用 SDA 公式 base = DEX/4。
  //   eff_av = (stored_av>0 ? stored_av : DEX/4 + 武器 AV 修正) + 武器技能(隱形)
  //   eff_dv = (stored_dv>0 ? stored_dv : DEX/4)
  // 來源:SDA(base AV/DV = DEX÷4;最終 AV 含武器技能 1:1)。
  int effective_av() const;
  int effective_dv() const;
  // eff_ac:stored_ac>0 採用;否則累加「所有已裝備物品(盾/甲/盔)」的 AC 修正(起始 0)。
  int effective_ac() const;
};

// 技能陣列索引(skills[0..26] 對應 selector 0x24..0x3E;index = selector − 0x24)。
// 武器技能(docs/44):0x37 斧 … 0x3E 投擲。
enum SkillIndex {
  kSkillAxe = 0x37 - 0x24,     // 19
  kSkillFlail = 0x38 - 0x24,   // 20
  kSkillMace = 0x39 - 0x24,    // 21
  kSkillSword = 0x3A - 0x24,   // 22
  kSkillTwoHand = 0x3B - 0x24, // 23
  kSkillBow = 0x3C - 0x24,     // 24
  kSkillCrossbow = 0x3D - 0x24,// 25
  kSkillThrown = 0x3E - 0x24,  // 26
};

class Party {
public:
  // 從 bundle 載入預設隊伍(assets/bundle/party/default_party.bin,自包含,
  // 抽自 DATA1 @ 0x2E26 起 4×512B:Muskels / Theb / Elendil / Cheetah)。
  // 失敗回傳空隊伍(size()==0)。
  static Party load_default(const std::filesystem::path& bundle_dir);

  // 從原始位元組(N×512)建立(供測試)。
  static Party from_records(const std::vector<std::uint8_t>& bytes);

  // 從一組 512B 原始 record 建立(供讀檔還原;不過濾空槽 → 與存檔內容一一對應)。
  static Party from_raw_records(
      const std::vector<std::array<std::uint8_t, 512>>& records);

  // 取每名角色的完整 512B 原始 record(供存檔;與 at(i) 同順序)。
  std::vector<std::array<std::uint8_t, 512>> raw_records() const;

  std::size_t size() const { return members_.size(); }
  const CharacterRecord& at(std::size_t i) const { return members_.at(i); }
  // 可變存取(供成長 / 戰後結算改角色;progression 模組用)。越界丟例外。
  CharacterRecord& at(std::size_t i) { return members_.at(i); }

  // 戰鬥勝利後給每名隊員加 XP(DOS 實機:清怪每員 +80,見 combat_loop.hpp)。
  // 同步更新 CharacterRecord.xp 與 raw[80](1 byte,飽和到 255 → 存檔 round-trip 一致)。
  void award_xp(int per_member);

  // status bitfield → 英文狀態鍵(供 i18n tr() 在地化);正常回 "normal"。
  // 檢查順序與 opendw 一致(dead > stunned > poisoned > chained)。
  static const char* status_key(std::uint8_t status);

  // 在畫面右側畫隊伍狀態面板(對拍 opendw draw_player_status_panel)。
  //   - 像素層:每名角色一列底框 + HP(亮紅)/暈眩(亮綠)/法力(亮藍)狀態條
  //   - 文字層:角色名字(置中於列頂),異常狀態以文字標示("is dead" 等)
  // name_px:名字字級(視窗 px),0 用預設。
  void draw_status_panel(render::Framebuffer& fb, render::TextLayer& tl,
                         int name_px = 0) const;

private:
  std::vector<CharacterRecord> members_;
  static CharacterRecord parse_record(const std::uint8_t* p);
};

}  // namespace dw::game
