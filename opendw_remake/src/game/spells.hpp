// spells — 法術資料表 + 戰鬥施法結算(確定性切片)。
//
// Deep module:對外只露 (1) 全域法術表查詢、(2) 角色已習得/可施法判定、
// (3) 單次施法結算 cast()。內部隱藏 spells[60-67] bitfield 走訪、效果分派、骰式。
//
// ── 對齊狀態(務必誠實)────────────────────────────────────────────────
//  • 法術 id:對齊 **fraterrisus 法術索引**(docs/44 §3「法術索引 [60-67] bitfield」,
//    0x00 L:魔火 … 0x3F)。角色 record[60-67] 為 64-bit bitfield(byte b、bit i →
//    spell id = b*8 + i),記錄該角色已習得哪些法術。
//  • 效果值(傷害/治療範圍、目標、Power 消耗):**全部取自臺灣中文版手冊**
//    (docs/33_MANUAL_TRANSCRIPTION.md 第 20-34 頁,附錄二咒語說明)。
//    手冊未明列確切骰分布/隱藏修正者標 TODO,**不臆造**。
//  • 施法結算公式 = **依手冊規格 grounded**,非 opendw byte-for-byte 移植
//    (opendw C 碼未實作法術結算,只到選單/載圖;真正 bytecode 結算 op 未逆出)。
//    因此 **不可宣稱為 oracle 真值**。傷害/治療擲骰走 CombatRng(確定性,可 seed)。
//  • 「使用力量的 N 倍」類(火燄之光 Str×1–6、日灼術 Str×1–8…):手冊明記
//    「使用者力量愈強效果愈大」「使用力量可隨意改變」。本切片以「施法者 STR ×
//    rng[1..N]」近似(power_scaled),確切倍率基準(STR? 投入 Power?)待 DOS 校準。
// ──────────────────────────────────────────────────────────────────────
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "game/combat.hpp"
#include "game/party.hpp"

namespace dw::game {

// 法術類別(對齊 docs/44 法術索引前綴 L/H/D/S/M)。
enum class SpellSchool : std::uint8_t {
  Low,    // L 初級(Low Magic)
  High,   // H 高級(High Magic)
  Druid,  // D 德魯伊(Druid Magic)
  Sun,    // S 太陽(Sun Magic)
  Misc,   // M 其他(Miscellaneous Magic)
};

// 效果類型(決定 cast() 如何套用)。
enum class SpellEffect : std::uint8_t {
  Damage,       // 傷害:擲範圍 → 扣目標 STUN
  PowerScaled,  // 傷害(力量倍率):施法者 STR × rng[1..mult] → 扣目標 STUN
  Heal,         // 治療:回復我方 STUN(擲範圍)
  BuffAv,       // +AV(攻擊值)
  BuffDv,       // +DV(防禦值)
  BuffAc,       // +AC(護甲)
  BuffStr,      // +STR(力量)
  BuffDex,      // +DEX(敏捷)
  DebuffAvDv,   // -AV/-DV(蟲災)
  Control,      // 控制類(逃跑/迷失/停止前進…)— 數值結算 TODO
  Utility,      // 工具類(光源/召喚/感測/補給…)— 戰鬥外或未結算,TODO
};

// 目標範圍。
enum class SpellTarget : std::uint8_t {
  OneEnemy,    // 一個敵人
  GroupEnemy,  // 一群敵人
  AllEnemy,    // 所有敵人
  OneAlly,     // 一個人物/隊員
  AllAllies,   // 整個小組
  Item,        // 一個物品(補給類)
  Special,     // 其他(召喚/工具)
};

// 單條法術定義。效果值全 grounded 手冊(docs/33);手冊缺值處標 TODO 並設 0。
struct SpellDef {
  std::uint8_t id;           // fraterrisus 索引(0x00..0x3F)
  SpellSchool school;
  const char* name_key;      // i18n 鍵(英文法術名,對拍 assets/i18n/*/spells.tsv)
  SpellEffect effect;
  SpellTarget target;
  int power_cost;            // 使用力量 Power(法力消耗);0 = 可隨意改變/不定(見 variable_power)
  bool variable_power;       // true = 手冊記「可隨意改變」(power_cost 為最低投入暫定值)
  int amount_min;            // 效果值下界(傷害/治療點數;buff 為固定加值放 amount_min)
  int amount_max;            // 效果值上界(buff 固定加值時 == amount_min)
  int power_mult;            // PowerScaled 倍率上界(火燄之光=6 → STR×rng[1..6]);其餘 0
  const char* manual_note;   // 手冊頁/原文摘要(grounding 追溯)
};

// 全域法術表(依手冊建構,id 對齊 fraterrisus 索引)。回傳唯讀靜態表。
const std::vector<SpellDef>& spell_table();

// 依 id 查法術定義;查無回 nullptr。
const SpellDef* find_spell(std::uint8_t id);

// 角色是否已習得某法術(走訪 spells[60-67] bitfield:id=b*8+i)。
bool character_knows_spell(const CharacterRecord& c, std::uint8_t id);

// 角色當前可施法清單:已習得 且 Power 足夠(variable_power 視為需 >= power_cost 最低投入)。
// 回傳法術 id(已排序,對齊 spell_table 順序)。
std::vector<std::uint8_t> castable_spells(const CharacterRecord& c, int current_power);

// ── 施法結算 ───────────────────────────────────────────────────────────
// 施法結果(供逐回合對拍 / log / UI)。
struct CastResult {
  bool ok = false;            // 是否成功施放(已習得 + Power 足夠 + 已結算)
  std::uint8_t spell_id = 0;
  int power_spent = 0;        // 實扣 Power
  int amount = 0;             // 傷害/治療點數(或 buff 加值);控制/工具類為 0
  int target_hp_after = 0;    // 傷害/治療後目標 STUN(單體)
  bool target_died = false;   // 傷害致死
  bool handled = false;       // 效果是否已數值結算(false = 控制/工具類 TODO,只扣 Power)
  std::string note;           // 簡短說明(英文鍵或 TODO 標記)
};

// 施法:caster 對 target 施放 spell_id。
//   • caster_power:傳入施法者當前 Power(by-value 讀;扣除量回填 result.power_spent,
//     呼叫端負責把 power -= power_spent 寫回角色)。
//   • caster_str:施法者 STR(PowerScaled / buff 結算用)。
//   • target:傷害/治療作用對象(單體);group/all 由呼叫端對多目標各呼叫一次。
//   • rng:CombatRng(確定性,與物理攻擊共用同一序列)。
// 傷害/治療作用於 target.hp(=STUN),與物理攻擊一致(SDA:HP=Stun)。
// 控制/工具類目前只扣 Power 並標 handled=false(TODO),不改數值。
CastResult cast_spell(std::uint8_t spell_id, int caster_power, int caster_str,
                      Combatant& target, CombatRng& rng);

}  // namespace dw::game
