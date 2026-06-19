// progression — RPG 成長迴圈(升級 / X 配點 / 使用物品 / 裝備穿脫 / 技能檢定)。
//
// Deep module:對外只露「對一名 CharacterRecord 做成長操作」的窄介面;內部隱藏
//   XP 門檻曲線、升級結算、AP 守恆、512B raw record 同步(存檔 round-trip 一致)、
//   物品魔法效果套用、裝備位元翻轉、技能檢定骰式。**不重算戰鬥命中/傷害公式**
//   (那是 combat.hpp resolve_attack 的真值範疇;本模組只在戰鬥外/戰後成長)。
//
// 純資料 + 規則,無 SDL/render 相依 → 可獨立進 ctest(tools/verify/verify_progression.cpp)。
// 確定性:所有隨機(技能檢定)走呼叫端帶入的 CombatRng,固定 seed → 固定結果。
//
// ── 鐵則:raw[] 與解析欄位同步 ──────────────────────────────────────────────
//   savegame 存的是 512B 原始 record(savegame.hpp)。本模組每次改 level/xp/AP/
//   屬性/技能/HP/STUN/PWR/裝備位元,**必同步寫回 CharacterRecord.raw[]**(對齊
//   party.cpp award_xp 的作法),確保「存→讀→存」byte-for-byte 一致、且 512B
//   fraterrisus 格式不破壞。offset 全部對齊 docs/reverse-engineering/44 §1/§2。
//
// ── grounded 來源 vs remake 設計(誠實標示)──────────────────────────────────
//   [手冊33] 升級提升 STR / HP / STUN(p.12 #1「每一等級提高力量值」、#6/#7
//            生命暈眩隨等級;p.12 #10「每升一級電腦會問是否分配額外點數」=X 配點)。
//   [SDA/docs44 L103] 升級唯一可花用好處 = +2 技能點(AP[59])。
//   [手冊33 p.8 U] 使用物品 / 技能 / 屬性;[手冊 p.13/14] 包紮回 HP、開鎖開鎖。
//   [docs44 §2] 物品魔法效果:Use 時施法 / 教法術 / 回復法力(magic_effect 解碼)。
//   [docs44 §2] 裝備 bit[00]=已裝備;穿脫即翻此位元 → effective_av/ac 隨之變。
//   [remake 設計,手冊未明列確切常數 → 明標]:
//     • XP 曲線(每級門檻)= 80 × 當前等級(threshold_for_next)。理由:戰鬥每員
//       +80 XP(docs43 有據)→ 一級需「約等級數場」勝利,難度隨等級遞增。XP[80]
//       為 1 byte:升級時**消耗**門檻(xp -= threshold,不累積爆 byte),故 xp 永
//       落在 [0,255]、可反覆升級,存檔 round-trip 安全。
//     • 每級屬性成長:STR +1(手冊「每等級提高力量值」有定性,+1 為 remake 量化)。
//     • 每級 HP/STUN 成長:+(2 + STR/4)(手冊「隨等級提高」定性,公式為 remake)。
//       cur 與 max 同加(滿血升級)。PWR 升級不變(手冊未明列;留保守)。
//     • 技能檢定:d20 風格 → roll = rng.below(20)+1;success ⇔ roll + skill_level
//       ≥ difficulty。SDA 提「lockpick 4 多數夠」→ 取一般鎖 difficulty=10
//       (skill 4 時需 roll≥6,70% 命中,吻合「多數夠」)。難度為 remake 量化。
//     • 包紮回 HP:heal = bandage_skill × rng[1..(STR/4+1)](remake;手冊只定性「回 HP」)。
#pragma once

#include <array>
#include <cstdint>

#include "game/combat.hpp"   // CombatRng(確定性技能檢定)
#include "game/party.hpp"     // CharacterRecord

namespace dw::game {

// ── 成長常數(remake 設計;見檔頭來源標示)──────────────────────────────────
namespace progression {
inline constexpr int kApPerLevel        = 2;    // [SDA/docs44] 每升一級 +2 AP
inline constexpr int kXpPerLevelFactor  = 80;   // [remake] 門檻 = 80 × 當前等級
inline constexpr int kStrGainPerLevel   = 1;    // [remake] 每級 STR +1(手冊定性)
inline constexpr int kHpGainBase        = 2;    // [remake] 每級 HP/STUN +(2 + STR/4)
inline constexpr int kAttrMaxInGame     = 30;   // [remake] in-game 屬性上限(升級可破建角 18 上限)
inline constexpr int kSkillMax          = 30;   // [remake] 技能等級上限
inline constexpr int kSkillCheckDie     = 20;   // [remake] 技能檢定 d20

// 常見技能檢定難度(remake 量化;SDA「lockpick 4 多數夠」校準)。
inline constexpr int kLockEasy   = 6;
inline constexpr int kLockNormal = 10;
inline constexpr int kLockHard   = 16;

// 技能陣列索引(skills[0..26],index = selector − 0x24;docs/reverse-engineering/44 §3)。
inline constexpr int kBandage  = 0x29 - 0x24;  // 5  包紮
inline constexpr int kLockpick = 0x2D - 0x24;  // 9  開鎖

// 可花 AP 的屬性(對齊 chargen::Attr 順序;X 配點 UI 用)。
enum AttrTarget { kAttrStr = 0, kAttrDex = 1, kAttrInt = 2, kAttrSpi = 3, kAttrTargetCount = 4 };
}  // namespace progression

// 升級一次的結算明細(供 UI 戰報 / 驗證)。
struct LevelUpResult {
  int levels_gained = 0;   // 本次升了幾級(可一次連升)
  int ap_gained     = 0;   // 共獲得 AP(= levels_gained × kApPerLevel)
  int hp_gained     = 0;   // max HP 總增量
  int stun_gained   = 0;   // max STUN 總增量
  int str_gained    = 0;   // STR 總增量
  int new_level     = 0;   // 升級後等級
  bool leveled() const { return levels_gained > 0; }
};

// 檢查並執行升級(可一次連升多級):當 xp ≥ 當前等級門檻時消耗門檻、level+1、
//   給 AP、提升 STR / HP / STUN(cur=max 同加)。回傳結算明細。
//   無達門檻 → levels_gained=0、record 不變。
//   同步 raw[] 全部相關 offset(level/xp/AP/STR/HP/STUN)→ 存檔 round-trip 一致。
//   level[79] 在 raw 以 1 byte 寫(party.cpp 雖以 rd16 讀,高位元組恆 0)。
LevelUpResult check_level_up(CharacterRecord& c);

// 升級所需門檻(達到下一級需要的 xp)= kXpPerLevelFactor × 當前等級。level≤0 視為 1。
int xp_threshold_for_next(int current_level);

// ── X 配點(in-game,花 AP[59])────────────────────────────────────────────
// 花 1 AP 給屬性 +1(STR/DEX/INT/SPI;同步 cur=max)。AP 不足或已達上限 → 回 false 不變。
//   STR↑ 不自動回算 HP(僅升級時依公式長);DEX↑ → effective_av/dv(=DEX/4)自動隨之變。
bool spend_ap_on_attr(CharacterRecord& c, int attr_target);
// 花 1 AP 給技能(skills[0..26])+1。AP 不足 / 索引越界 / 已達上限 → false。
bool spend_ap_on_skill(CharacterRecord& c, int skill_index);
// 可花用的 AP(= raw[59])。
int available_ap(const CharacterRecord& c);

// ── 使用物品 U(套魔法效果 + 消耗)──────────────────────────────────────────
// 使用第 slot 格物品的效果(docs/reverse-engineering/44 §2 magic_effect):
//   restores_power → 回 Power(數量 = magic_lo,夾到 max_power)
//   casts_spell    → 對自己施該法術(回傳 spell_id;結算由呼叫端 cast,本模組只解效果類型)
//   teaches_spell  → 習得該法術(設 spells bitfield 對應 bit)
// 效果套用後扣 1 充能;充能歸 0(原本無充能的消耗品)→ 物品自該格移除(清 23B)。
// raw[] 同步(power / spells bitfield / 物品格)。回傳結構描述發生了什麼。
struct UseItemResult {
  bool ok = false;            // 該格有物品且有可用效果
  bool restored_power = false; int power_restored = 0;
  bool taught_spell = false;  int taught_spell_id = -1;
  bool casts_spell = false;   int cast_spell_id = -1;  // 呼叫端負責實際施法結算
  bool consumed = false;      // 物品本次被消耗移除
};
UseItemResult use_item(CharacterRecord& c, int slot);

// ── 裝備穿脫(翻 bit[00];effective_av/ac 隨之變)──────────────────────────────
// 切換第 slot 格物品的已裝備狀態。空格 / 越界 → false。回傳切換後是否為「已裝備」。
// 同種互斥規則(remake 設計,避免裝兩件主武器/兩件身甲):裝備一件武器時自動卸下
//   其他已裝備武器;裝備一件身甲時自動卸下其他身甲。盾/盔/一般物品不互斥。
// 同步 raw[](物品格 bit[00])→ inventory()/main_weapon()/effective_ac() 立即反映。
struct EquipResult {
  bool ok = false;        // 該格有物品
  bool now_equipped = false;
};
EquipResult toggle_equip(CharacterRecord& c, int slot);

// ── 技能檢定(成長 payoff)─────────────────────────────────────────────────
// 通用檢定:roll = rng.below(kSkillCheckDie)+1;success ⇔ roll + skill_level ≥ difficulty。
// 確定性(同 rng 狀態 → 同結果)。回傳是否成功。
struct SkillCheckResult { bool success = false; int roll = 0; int total = 0; };
SkillCheckResult skill_check(int skill_level, int difficulty, CombatRng& rng);

// 開鎖檢定:用角色 lockpick 技能對指定難度檢定。
SkillCheckResult try_lockpick(const CharacterRecord& c, int difficulty, CombatRng& rng);

// 包紮:healer 用 bandage 技能替 target 回 HP(STUN 同步上限)。
//   回血量 = bandage_skill × rng[1..(healer.STR/4 + 1)];target 已死(status bit0)→ 不可包紮。
//   實際回血夾到 target.max_health;同步 target.raw[]。回傳實際回血量(0=未生效)。
// 來源:手冊「包紮回 HP」(定性);骰式為 remake 量化。
int bandage(const CharacterRecord& healer, CharacterRecord& target, CombatRng& rng);

}  // namespace dw::game
