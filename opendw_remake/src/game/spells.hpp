// spells — 法術資料表 + 戰鬥施法結算(確定性切片)。
//
// Deep module:對外只露 (1) 全域法術表查詢、(2) 角色已習得/可施法判定、
// (3) 單次施法結算 cast()。內部隱藏 spells[60-67] bitfield 走訪、效果分派、骰式。
//
// ── 對齊狀態(務必誠實)────────────────────────────────────────────────
//  • 法術 id:對齊 **fraterrisus 法術索引**(docs/reverse-engineering/44 §3「法術索引 [60-67] bitfield」,
//    0x00 L:魔火 … 0x3F)。角色 record[60-67] 為 64-bit bitfield(byte b、bit i →
//    spell id = b*8 + i),記錄該角色已習得哪些法術。
//  • 效果值(骰式/目標/Power/類型/攻擊判定):**權威依據 = docs/gameplay/58_MAGIC_REFERENCE.md
//    (fraterrisus 攻略 v3.0 整理)**,比臺灣手冊完整(含召喚生物精確屬性、Zap 攻擊判定
//    公式、各系全法術 POW/dice)。傷害/治療以 dice_count d dice_sides 表(Mage Fire 1d8…),
//    變動消耗(var.)以「每點」骰式表(Inferno 1d4/pt),投入上限 = 2×魔法技能 ranks。
//  • 真值層級 = **remake 設計(grounded 攻略)**:攻略為約定俗成 + 反編佐證,非官方臺灣手冊、
//    **非 opendw bytecode oracle**(opendw C 碼未實作法術結算,只到選單/載圖)。誠實標示,
//    不謊稱 bytecode 真值。傷害/治療/判定擲骰走 CombatRng(確定性,可 seed)。
//  • Zap/Debuff 攻擊判定(docs/gameplay/58_MAGIC_REFERENCE.md):每目標 1d16+2 roll-under,門檻 12 + 魔法技能 ranks +
//    施法者 INT − 防禦方 DV;miss 仍吃半傷。與近戰公式(1d16+3、13+AV−(DV+AC)、bytecode
//    真值)刻意分開,見 zap_attack_roll。AC 不參與法術判定。
//  • 召喚:屬性照 docs/gameplay/58_MAGIC_REFERENCE.md 召喚表(HP/Dex/Armor/武器骰);make_summon 以攻略表值 grounded。
// ──────────────────────────────────────────────────────────────────────
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "game/combat.hpp"
#include "game/party.hpp"

namespace dw::game {

// 法術類別(對齊 docs/reverse-engineering/44 法術索引前綴 L/H/D/S/M)。
enum class SpellSchool : std::uint8_t {
  Low,    // L 初級(Low Magic)
  High,   // H 高級(High Magic)
  Druid,  // D 德魯伊(Druid Magic)
  Sun,    // S 太陽(Sun Magic)
  Misc,   // M 其他(Miscellaneous Magic)
};

// 效果類型(決定 cast() 如何套用)。
enum class SpellEffect : std::uint8_t {
  Damage,       // 傷害(固定骰):擲 dice_count d dice_sides → 扣目標 STUN(Zap;走攻擊判定)
  PowerScaled,  // 傷害(每點變動,docs/gameplay/58_MAGIC_REFERENCE.md「Nd? hp/pt」):每投入 1 點 Power 擲一次 dice_count d
                //   dice_sides,投入點數上限 = 2×魔法技能 ranks(docs/gameplay/58_MAGIC_REFERENCE.md §var.)。走攻擊判定。
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

// 召喚類效果的具體種類(decides 召喚出的臨時友方屬性)。
// 屬性照 **docs/gameplay/58_MAGIC_REFERENCE.md 召喚表精確值**(HP/Dex/Armor/武器骰);AV/DV=Dex/4。真值層級 =
// remake 設計(grounded 攻略),**非 bytecode oracle**。見 spells.cpp make_summon。
enum class SummonKind : std::uint8_t {
  None,         // 非召喚類
  AirElemental, // 召喚風元素(0x15):HP12/Dex12/Armor8/1d10
  EarthElemental,// 召喚地元素(0x16):HP15/Dex14/Armor10/1d20
  WaterElemental,// 召喚水元素(0x17):HP25/Dex15/Armor11/1d20
  FireElemental,// 召喚火元素(0x18):HP35/Dex18/Armor15/2d20(最強元素)
  Beast,        // 呼叫野獸(0x24):HP13/Dex16/Fur7/1d12
  WoodSpirit,   // 樹木精靈(0x25):HP19/Dex16/Bark9/1d12
  Spirit,       // 召喚精靈(0x23):HP13/Dex18/Aura12/3d10(高傷)
  Salamander,   // 召喚火蜥蜴(0x39):HP23/Dex24/Scales10/1d10(攻略:很危險的咒語)
};

// 控制類效果的具體種類(decides combat_loop 如何套用)。
// 全部 grounded 手冊 docs/manual/33「效果 Effect」欄位描述;確切持續回合數手冊未給,
// 用合理值並標「remake 設計」(見 spells.cpp kControlDazzleTurns)。
enum class ControlKind : std::uint8_t {
  None,      // 非控制類
  Daze,      // 使目標數回合無法行動(眩目/閃光/火燄柱/圍困/龍捲風/荊棘:迷失/停止前進/推開/障礙)
  Flee,      // 使目標逃離戰鬥(膽怯/驚嚇:使敵人逃跑)
  Disarm,    // 解除目標武裝(傷害降為徒手骰;手冊「解除武裝」)
  Dispel,    // 驅散幻影(幻影現形)— 無戰鬥數值意義 → combat_loop 視為 no-op(N/A)
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

// 單條法術定義。
//   • 數值權威依據 = **docs/gameplay/58_MAGIC_REFERENCE.md(fraterrisus 攻略 v3.0)**;真值層級
//     = remake 設計(grounded 攻略),非官方手冊、非 bytecode oracle(誠實標示)。
//   • 傷害/治療以 dice_count d dice_sides 表示(docs/gameplay/58_MAGIC_REFERENCE.md 骰式,如 Mage Fire 1d8、
//     Fire Storm 6d6、Inferno 1d4/pt);buff/debuff 固定加值放 amount_min(==amount_max)。
struct SpellDef {
  std::uint8_t id;           // fraterrisus 索引(0x00..0x3F)
  SpellSchool school;
  const char* name_key;      // i18n 鍵(英文法術名,對拍 assets/i18n/*/spells.tsv)
  SpellEffect effect;
  SpellTarget target;
  int power_cost;            // 使用力量 Power(法力消耗);variable_power 時為最低投入(1 點)
  bool variable_power;       // true = docs/gameplay/58_MAGIC_REFERENCE.md「var.」(可選投入多點,每點加一份;上限 2×ranks)
  int amount_min;            // buff/debuff:固定加值;Damage/PerPoint:= dice_count*1(骰下界,衍生)
  int amount_max;            // buff/debuff:== amount_min;Damage/PerPoint:= dice_count*dice_sides(骰上界,衍生)
  int dice_count;            // 傷害/治療骰數(docs/gameplay/58_MAGIC_REFERENCE.md「NdM」的 N);非擲骰類為 0
  int dice_sides;            // 傷害/治療骰面(docs/gameplay/58_MAGIC_REFERENCE.md「NdM」的 M);非擲骰類為 0
  bool zap;                  // true = Zap/Debuff 攻擊類,需法術攻擊判定(1d16+2 roll-under,miss 半傷)
  ControlKind control;       // Control 效果的具體種類(非控制類為 ControlKind::None)
  const char* note58;        // docs/gameplay/58_MAGIC_REFERENCE.md 出處/原文摘要(grounding 追溯)
  SummonKind summon = SummonKind::None;  // 召喚類的具體種類(非召喚類為 SummonKind::None)
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
  bool handled = false;       // 效果是否已數值結算(false = 工具/召喚類仍 TODO,只扣 Power)
  ControlKind control = ControlKind::None;  // 已套用的控制種類(Daze/Flee/Disarm/Dispel);非控制為 None
  int dazzle_turns = 0;       // Daze:套用的跳過回合數(remake 設計值);其餘 0
  bool target_fled = false;   // Flee:目標本次被逐出戰鬥
  SummonKind summon = SummonKind::None;  // 召喚類:已召喚出的種類(供 combat_loop 加臨時友方);非召喚為 None
  // ── Zap 攻擊判定明細(docs/gameplay/58_MAGIC_REFERENCE.md;僅 zap 類有意義)──────────────────────────
  bool is_zap = false;        // 此法術走 Zap 攻擊判定(Damage/PerPoint 且 zap==true)
  bool zap_hit = false;       // 攻擊判定命中(true=全傷;false=miss,amount 已折半)
  int zap_roll = 0;           // 1d16+2 擲值
  int zap_need = 0;           // 命中門檻(12 + ranks + INT − DV)
  std::string note;           // 簡短說明(英文鍵或 TODO 標記)
};

// 控制類:每次施放使目標暈眩(Daze)幾回合(remake 設計;手冊未給確切持續)。
// 取 2:足以讓隊伍把握 1 個完整回合再攻擊(對齊手冊「最好能有人把握時機攻擊敵人」),
// 又不致一發定生死。**非 oracle 真值**。
inline constexpr int kControlDazzleTurns = 2;

// ── Zap/Debuff 法術攻擊判定(docs/gameplay/58_MAGIC_REFERENCE.md §「法術攻擊判定」)──────────────────────
// 每目標一次 1d16+2 roll-under;命中條件:roll ≤ 12 + 魔法技能 ranks + 施法者 INT − 防禦方 DV。
//   • 與近戰 resolve_attack 的差異(刻意分開,標清楚):
//       近戰 = 1d16+3、門檻 13 + 攻方 AV − (DV+AC)(bytecode 真值);
//       法術 = 1d16+2、門檻 12 + ranks + INT − DV(攻略 grounded;**AC 不參與、用 INT 取代 AV、
//             魔法技能 ranks 取代武器技能**)。真值層級 = remake 設計(grounded docs/gameplay/58_MAGIC_REFERENCE.md)。
//   • 回傳 true = 命中(吃全傷);false = miss(docs/gameplay/58_MAGIC_REFERENCE.md:miss 仍吃半傷,由呼叫端折半)。
//   • 用 rng.below(16) 取 [0,16) 再 +2 → roll ∈ [2,17](對齊「1d16+2」)。
struct ZapHit { bool hit; int roll; int need; };
ZapHit zap_attack_roll(int caster_int, int magic_ranks, const Combatant& target,
                       CombatRng& rng);

// 施法:caster 對 target 施放 spell_id。
//   • caster_power:傳入施法者當前 Power(by-value 讀;扣除量回填 result.power_spent,
//     呼叫端負責把 power -= power_spent 寫回角色)。
//   • caster_str:施法者 STR(+STR/+DEX buff 與召喚傷害縮放用)。
//   • caster_int / magic_ranks:Zap 攻擊判定用(INT、魔法技能 ranks);預設值供舊呼叫端相容
//       (caster_int=0、ranks=0 → 判定門檻偏低,但 miss 仍半傷,效果不致歸零)。
//   • power_points:variable_power 法術實際投入的 Power 點數(每點擲一份 dice);<=0 → 取 1。
//       上限由呼叫端依 2×ranks 夾(此處只夾 >=1);固定 power 法術忽略此參數。
//   • target:傷害/治療作用對象(單體);group/all 由呼叫端對多目標各呼叫一次。
//   • rng:CombatRng(確定性,與物理攻擊共用同一序列)。
// 傷害/治療作用於 target.hp(=STUN),與物理攻擊一致(SDA:HP=Stun)。
//   Zap 類(Damage/PerPoint,zap==true)先走 zap_attack_roll:命中吃全傷、miss 吃半傷
//   (docs/gameplay/58_MAGIC_REFERENCE.md),result.zap_hit / zap_roll / zap_need 回報判定明細。
// 控制類(Daze/Flee/Disarm)直接作用於 target 的控制狀態;Dispel 為 N/A no-op。
//   工具/召喚類仍 handled=false(只扣 Power),召喚回填 r.summon。
CastResult cast_spell(std::uint8_t spell_id, int caster_power, int caster_str,
                      Combatant& target, CombatRng& rng, int caster_int = 0,
                      int magic_ranks = 0, int power_points = 1);

// ── 召喚 ───────────────────────────────────────────────────────────────
// 建立一個「召喚出的臨時友方 combatant」(remake 設計,grounded 手冊 docs/manual/33 效果欄)。
//   kind:召喚種類(決定屬性傾向,見 SummonKind);caster_str:施法者 STR(用於略微
//         隨施法者強度縮放傷害修正,呼應手冊「使用力量可隨意改變」)。
//   回傳的 Combatant:is_player=true(我方陣營)、summoned=true(戰鬥結束消失,見
//   combat_loop)、name 為英文鍵(對拍 i18n;UI tr 在地化)。
//   屬性數值為 **remake 設計**:手冊未給召喚物戰鬥屬性,只給「效果」文字差異;依該文字
//   差異化(火高攻、地高防…),並夾在「比一般雜兵略強、但不破壞平衡」的範圍。**非 oracle**。
Combatant make_summon(SummonKind kind, int caster_str);

// 法術 id → 召喚種類(查表;非召喚類回 SummonKind::None)。
SummonKind summon_kind_of(std::uint8_t spell_id);

}  // namespace dw::game
