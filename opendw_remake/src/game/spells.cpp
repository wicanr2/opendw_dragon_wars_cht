// spells — 法術表 + 施法結算實作。對齊狀態見 spells.hpp 檔頭。
//
// 法術表建構原則:
//   • id 對齊 fraterrisus 法術索引(docs/44 §3)。L/H 段 doc 44 已逐一給出 0x00..0x14;
//     召喚 0x15-0x18;D 段 0x19-0x25;S 段自 0x26 起。doc 44 在 S 段之後標「…完整見
//     fraterrisus / 手冊」,故 S/M 段的確切 id 連號為 **remake 依 doc 44 既有規律延伸**
//     (L→H→D→S→M、各 school 內按手冊列序),已標於各條 manual_note;若日後 fraterrisus
//     完整索引釋出與此有出入,只需改此表的 id 欄,bitfield 走訪邏輯不動。
//   • 效果值(min/max/mult/power/target)**全部取自手冊**(docs/33 第 20-34 頁)。
//     手冊未給數值者(控制/工具/召喚)設 amount=0 並於 manual_note 標 TODO。
#include "game/spells.hpp"

#include <algorithm>

namespace dw::game {

namespace {

using S = SpellSchool;
using E = SpellEffect;
using T = SpellTarget;

// 全域法術表。每條:{id, school, name_key, effect, target, power, variable_power,
//                    amount_min, amount_max, power_mult, manual_note}。
// 註:name_key 用英文法術名(與手冊括號內原文一致),對拍 assets/i18n/*/spells.tsv。
const std::vector<SpellDef> kSpells = {
    // ── A. 初級法術 Low Magic(doc 44 index 0x00-0x05;手冊 p20-21)──
    {0x00, S::Low, "Mage Fire", E::Damage, T::OneEnemy, 2, false, 1, 8, 0,
     "p20 魔火 1-8 點/30呎/一個敵人/Power 2"},
    {0x01, S::Low, "Disarm", E::Control, T::OneEnemy, 4, false, 0, 0, 0,
     "p20 解除武裝/30呎/一個敵人/Power 4 — 控制類,結算 TODO"},
    {0x02, S::Low, "Charm", E::BuffAv, T::OneAlly, 2, false, 1, 1, 0,
     "p20 吸引力 +1 AV(並治療一些)/一個人物/Power 2;時機:戰鬥 — 治療量手冊未給,TODO"},
    {0x03, S::Low, "Luck", E::BuffDv, T::OneAlly, 3, false, 2, 2, 0,
     "p21 祈福 +2 DV/一個人物/Power 3;時機:戰鬥"},
    {0x04, S::Low, "Lesser Heal", E::Heal, T::OneAlly, 2, false, 1, 4, 0,
     "p20 輕微醫療 1-4 點/治療/一個人物/Power 2"},
    {0x05, S::Low, "Mage Light", E::Utility, T::AllAllies, 1, false, 0, 0, 0,
     "p21 法師魔光/亮光/全個小組/1 點=3 小時 — 光源,戰鬥外,TODO"},

    // ── B. 高級法術 High Magic ──
    // (一)戰鬥咒語(doc 44 0x06-0x10;手冊 p21-24)
    {0x06, S::High, "Fire Light", E::PowerScaled, T::OneEnemy, 1, true, 0, 0, 6,
     "p21 火燄之光 使用力量的 1-6 倍/30呎/一個敵人/Power 可隨意改變"},
    {0x07, S::High, "Elvar's Fire", E::Damage, T::GroupEnemy, 6, false, 2, 12, 0,
     "p21 大火 2-12 點/30呎/一群敵人/Power 6"},
    {0x08, S::High, "Poog's Vortex", E::Damage, T::GroupEnemy, 11, false, 4, 24, 0,
     "p22 烈燄旋渦 4-24 點/20呎/一群敵人/Power 11"},
    {0x09, S::High, "Ice Chill", E::PowerScaled, T::OneEnemy, 1, true, 0, 0, 4,
     "p22 寒冰術 使用力量的 1-4 倍/50呎/一個敵人/Power 可隨意改變"},
    {0x0A, S::High, "Big Chill", E::Damage, T::AllEnemy, 15, false, 4, 24, 0,
     "p22 大寒冰術 4-24 點/30呎/所有敵人/Power 15"},
    {0x0B, S::High, "Dazzle", E::Control, T::OneEnemy, 3, false, 0, 0, 0,
     "p22 眩目強光/使敵人迷失方向/30呎/一個敵人/Power 3 — 控制類,TODO"},
    {0x0C, S::High, "Mystic Might", E::BuffStr, T::OneAlly, 4, false, 15, 15, 0,
     "p23 神力 +15 STR/時機:戰鬥/Power 4(手冊目標欄誤植「一個敵人」,效果為增益→視為隊員)"},
    {0x0D, S::High, "Reveal Glamour", E::Utility, T::GroupEnemy, 2, false, 0, 0, 0,
     "p23 幻影現形/驅散幻影/40呎/一群敵人/Power 2 — 工具類,TODO"},
    {0x0E, S::High, "Sala's Swift", E::BuffDex, T::OneAlly, 5, false, 8, 8, 0,
     "p24 迅捷術 +8 DEX/一個隊員;時機:戰鬥/Power 5"},
    {0x0F, S::High, "Vorn's Guard", E::BuffAc, T::AllAllies, 6, false, 2, 2, 0,
     "p23 保護罩 +2 AC(不影響 DV)/整個小組/Power 6"},
    {0x10, S::High, "Cowardice", E::Control, T::GroupEnemy, 8, false, 0, 0, 0,
     "p23 膽怯術/使敵人逃跑/60呎/一群敵人/Power 8 — 控制類,TODO"},
    // (二)醫療咒語(doc 44 0x11-0x12;手冊 p24)
    {0x11, S::High, "Healing", E::Heal, T::OneAlly, 3, false, 1, 6, 0,
     "p24 治療 1-6 點/一個人物/Power 3"},
    {0x12, S::High, "Group Heal", E::Heal, T::AllAllies, 6, false, 1, 6, 0,
     "p24 集體治療 1-6 點/整個小組/Power 6"},
    // (三)其他咒語(doc 44 0x13 遮蔽奧術、0x14 感知陷阱、0x15-0x18 召喚;手冊 p24-25)
    {0x13, S::High, "Cloak Arcane", E::BuffAc, T::AllAllies, 1, true, 2, 2, 0,
     "p25 隱身術 +2 AC/整個小組/可隨意改變;1 點=1 小時"},
    {0x14, S::High, "Sense Traps", E::Utility, T::AllAllies, 2, false, 0, 0, 0,
     "p25 偵測陷阱/感測/整個小組/2 小時 — 工具類,TODO"},
    {0x15, S::High, "Air Summon", E::Utility, T::Special, 1, true, 0, 0, 0,
     "p24 召喚風元素/召喚/1 點=4 小時 — 召喚類,TODO"},
    {0x16, S::High, "Earth Summon", E::Utility, T::Special, 1, true, 0, 0, 0,
     "p25 召喚地元素/召喚/1 點=4 小時 — 召喚類,TODO"},
    {0x17, S::High, "Water Summon", E::Utility, T::Special, 1, true, 0, 0, 0,
     "p25 召喚水元素/召喚/1 點=4 小時 — 召喚類,TODO"},
    {0x18, S::High, "Fire Summon", E::Utility, T::Special, 1, true, 0, 0, 0,
     "p25 召喚火元素/召喚/1 點=4 小時 — 召喚類,TODO"},

    // ── D. 德魯伊法術 Druid Magic(doc 44 0x19-0x25;手冊 p30-33)──
    // (一)戰鬥咒語
    {0x19, S::Druid, "Death Curse", E::Damage, T::OneEnemy, 6, false, 3, 18, 0,
     "p30 死亡的詛咒 3-18 點/40呎/一個敵人/Power 6"},
    {0x1A, S::Druid, "Fire Blast", E::Damage, T::GroupEnemy, 12, false, 4, 24, 0,
     "p31 爆炸術 4-24 點/30呎/一群敵人/Power 12"},
    {0x1B, S::Druid, "Insect Plague", E::DebuffAvDv, T::GroupEnemy, 4, false, 2, 2, 0,
     "p31 蟲害 -2 AV/DV/60呎/一群敵人;時機:戰鬥/Power 4"},
    {0x1C, S::Druid, "Whirl Wind", E::Control, T::GroupEnemy, 4, false, 0, 0, 0,
     "p31 龍捲風/推開 30 呎/40呎/一群敵人/Power 4 — 控制類,TODO"},
    {0x1D, S::Druid, "Scare", E::BuffAv, T::AllAllies, 4, false, 2, 2, 0,
     "p31 懼怕 +2 AV/整個小組;時機:戰鬥/Power 4"},
    {0x1E, S::Druid, "Brambles", E::Control, T::GroupEnemy, 5, false, 0, 0, 0,
     "p31 荊棘/產生障礙(得到一回合)/60呎/一群敵人/Power 5 — 控制類,TODO"},
    // (二)醫療咒語(手冊 p33)
    {0x1F, S::Druid, "Creater Healing", E::Heal, T::OneAlly, 4, false, 1, 6, 0,
     "p33 造物者治療 1-6 點/一個隊員/Power 4"},
    {0x20, S::Druid, "Cure All", E::Heal, T::AllAllies, 6, false, 1, 8, 0,
     "p33 治療全部 1-8 點/整個小組/Power 6"},
    // (三)其他咒語(手冊 p32-33)
    {0x21, S::Druid, "Create Wall", E::Utility, T::Special, 6, false, 0, 0, 0,
     "p32 建立石牆/Power 6 — 工具類,TODO"},
    {0x22, S::Druid, "Soften Stone", E::Utility, T::Special, 6, false, 0, 0, 0,
     "p32 軟化石頭/移開/Power 6 — 工具類,TODO"},
    {0x23, S::Druid, "Invoke Spirit", E::Utility, T::Special, 1, true, 0, 0, 0,
     "p33 召喚精靈/召喚/1 點=4 小時 — 召喚類,TODO"},
    {0x24, S::Druid, "Beast Call", E::Utility, T::Special, 1, true, 0, 0, 0,
     "p32 呼叫野獸/召喚/1 點=4 小時 — 召喚類,TODO"},
    {0x25, S::Druid, "Wood Spirit", E::Utility, T::Special, 1, true, 0, 0, 0,
     "p32 樹木精靈/召喚/1 點=4 小時 — 召喚類,TODO"},

    // ── C. 太陽法術 Sun Magic(doc 44 自 0x26 起;手冊 p26-30)──
    // 註:doc 44 僅明列 0x26 日炙、0x27 驅魔;以下 id 為 remake 依「S 段接 D 段、按手冊列序」
    //     延伸(見檔頭原則)。
    // (一)戰鬥咒語
    {0x26, S::Sun, "Sun Stroke", E::PowerScaled, T::OneEnemy, 1, true, 0, 0, 8,
     "p26 日灼術 使用力量的 1-8 倍/20呎/一個敵人/Power 可隨意改變"},
    {0x27, S::Sun, "Exorcism", E::Damage, T::GroupEnemy, 5, false, 6, 36, 0,
     "p26 驅鬼術 6-36 點/50呎/一群敵人/Power 5"},
    {0x28, S::Sun, "Rage of Mithras", E::PowerScaled, T::OneEnemy, 1, true, 0, 0, 6,
     "p26 光神之怒 使用力量的 1-6 倍/70呎/一個敵人/Power 可隨意改變"},
    {0x29, S::Sun, "Wrath of Mithras", E::PowerScaled, T::GroupEnemy, 1, true, 0, 0, 4,
     "p26 光神之怒(群) 使用力量的 1-4 倍/90呎/一群敵人/Power 可隨意改變"},
    {0x2A, S::Sun, "Fire Storm", E::Damage, T::AllEnemy, 20, false, 6, 36, 0,
     "p27 狂燄暴風 6-36 點/60呎/所有敵人/Power 20"},
    {0x2B, S::Sun, "Inferno", E::PowerScaled, T::AllEnemy, 1, true, 0, 0, 4,
     "p27 地獄之火 使用力量的 1-4 倍/40呎/所有敵人/Power 可隨意改變"},
    {0x2C, S::Sun, "Holy Aim", E::BuffAv, T::AllAllies, 5, false, 2, 2, 0,
     "p27 聖戰光輝 +2 AV/整個小組;時機:戰鬥/Power 5"},
    {0x2D, S::Sun, "Battle Power", E::BuffStr, T::AllAllies, 8, false, 10, 10, 0,
     "p27 戰鬥力 +10 STR/整個小組;時機:戰鬥/Power 8"},
    {0x2E, S::Sun, "Column of Fire", E::Control, T::GroupEnemy, 5, false, 0, 0, 0,
     "p28 火燄柱/停止前進/40呎/一群敵人/Power 5 — 控制類,TODO"},
    {0x2F, S::Sun, "Mithra's Bless", E::BuffDv, T::AllAllies, 5, false, 3, 3, 0,
     "p28 光神的祝福 +3 DV/整個小組;時機:戰鬥/Power 5"},
    {0x30, S::Sun, "Light Flash", E::Control, T::GroupEnemy, 6, false, 0, 0, 0,
     "p28 閃光術/迷失方向/50呎/一群敵人/Power 6 — 控制類,TODO"},
    {0x31, S::Sun, "Armor of Light", E::BuffAc, T::OneAlly, 6, false, 2, 2, 0,
     "p28 光的武裝 +2 AC/一個隊員;時機:戰鬥/Power 6"},
    // (二)醫療咒語(手冊 p29)
    {0x32, S::Sun, "Sun Light", E::Heal, T::OneAlly, 3, false, 1, 6, 0,
     "p29 陽光醫療 1-6 點/一個隊員/Power 3"},
    {0x33, S::Sun, "Heal", E::Heal, T::OneAlly, 4, false, 2, 8, 0,
     "p29 治療 2-8 點/一個隊員/Power 4"},
    {0x34, S::Sun, "Major Heal", E::Heal, T::AllAllies, 6, false, 1, 6, 0,
     "p29 主要治療 1-6 點/整個小組/Power 6"},
    // (三)其他咒語(手冊 p29-30)
    {0x35, S::Sun, "Charger", E::Utility, T::Item, 8, false, 0, 0, 0,
     "p29 補給/對耗盡的法術物品補充/一個物品/Power 8 — 工具類,TODO"},
    {0x36, S::Sun, "Disarm Trap", E::Utility, T::Special, 1, true, 0, 0, 0,
     "p29 解除陷阱/1 點=2 小時 — 工具類,TODO"},
    {0x37, S::Sun, "Guidance", E::Utility, T::AllAllies, 1, true, 0, 0, 0,
     "p30 導引術/指引/1 點=3 小時 — 工具類,TODO"},
    {0x38, S::Sun, "Radiance", E::Utility, T::AllAllies, 1, true, 0, 0, 0,
     "p30 輻射光輝/光線/40呎/整個小組/1 點=2 小時 — 工具類,TODO"},
    {0x39, S::Sun, "Summon Salamander", E::Utility, T::Special, 1, true, 0, 0, 0,
     "p30 召喚火蜥蜴/召喚/1 點=4 小時 — 召喚類,TODO"},

    // ── E. 其他法術 Miscellaneous Magic(手冊 p33-34;id 接 S 段續延伸)──
    {0x3A, S::Misc, "Zak's Speed", E::BuffDex, T::AllAllies, 10, false, 15, 15, 0,
     "p33 增加敏捷 +15 DEX/整個小組;時機:戰鬥/Power 10"},
    {0x3B, S::Misc, "Kill Ray", E::Damage, T::OneEnemy, 15, false, 10, 80, 0,
     "p33 死光 10-80 點/50呎/一個敵人/Power 15"},
    {0x3C, S::Misc, "Prison", E::Control, T::GroupEnemy, 8, false, 0, 0, 0,
     "p34 圍困/停止前進/60呎/一群敵人/Power 8;時機:戰鬥 — 控制類,TODO"},
};

}  // namespace

const std::vector<SpellDef>& spell_table() { return kSpells; }

const SpellDef* find_spell(std::uint8_t id) {
  for (const auto& s : kSpells)
    if (s.id == id) return &s;
  return nullptr;
}

bool character_knows_spell(const CharacterRecord& c, std::uint8_t id) {
  // spells[60-67] 為 64-bit bitfield:spell id = byte_index*8 + bit_index。
  int b = id / 8, bit = id % 8;
  if (b < 0 || b >= static_cast<int>(c.spells.size())) return false;
  return (c.spells[b] >> bit) & 0x01;
}

std::vector<std::uint8_t> castable_spells(const CharacterRecord& c,
                                          int current_power) {
  std::vector<std::uint8_t> out;
  for (const auto& s : kSpells) {
    if (!character_knows_spell(c, s.id)) continue;
    // variable_power 視為需 >= power_cost(最低投入);固定 power 需 >= power_cost。
    if (current_power < s.power_cost) continue;
    out.push_back(s.id);
  }
  return out;
}

CastResult cast_spell(std::uint8_t spell_id, int caster_power, int caster_str,
                      Combatant& target, CombatRng& rng) {
  CastResult r;
  r.spell_id = spell_id;
  const SpellDef* sp = find_spell(spell_id);
  if (!sp) {
    r.note = "unknown spell";
    return r;
  }
  if (caster_power < sp->power_cost) {
    r.note = "insufficient power";
    return r;
  }
  r.ok = true;
  r.power_spent = sp->power_cost;  // 扣 Power(variable_power 暫扣最低投入,確切投入待 DOS 校準)

  switch (sp->effect) {
    case E::Damage: {
      // 擲手冊範圍 [min..max]:以 min + rng.below(span+1) 均勻取值(確定性)。
      int span = sp->amount_max - sp->amount_min;
      int dmg = sp->amount_min + (span > 0 ? static_cast<int>(rng.below(
                                                 static_cast<std::uint16_t>(span + 1)))
                                           : 0);
      r.amount = dmg;
      target.hp -= dmg;  // 作用於 STUN(HP=Stun)
      if (target.hp <= 0) {
        target.hp = 0;
        target.status |= 0x01;
        r.target_died = true;
      }
      r.handled = true;
      break;
    }
    case E::PowerScaled: {
      // 「使用力量的 1..mult 倍」:以施法者 STR × rng[1..mult] 近似(手冊:力量愈強效果愈大)。
      // 確切倍率基準(STR? 投入 Power?)待 DOS 校準 — 此處 grounded 於手冊文字描述。
      int mult = 1 + (sp->power_mult > 1
                          ? static_cast<int>(rng.below(
                                static_cast<std::uint16_t>(sp->power_mult)))
                          : 0);
      int dmg = caster_str * mult;
      r.amount = dmg;
      target.hp -= dmg;
      if (target.hp <= 0) {
        target.hp = 0;
        target.status |= 0x01;
        r.target_died = true;
      }
      r.handled = true;
      break;
    }
    case E::Heal: {
      int span = sp->amount_max - sp->amount_min;
      int heal = sp->amount_min + (span > 0 ? static_cast<int>(rng.below(
                                                  static_cast<std::uint16_t>(span + 1)))
                                            : 0);
      r.amount = heal;
      target.hp += heal;  // 回復 STUN
      if (target.hp > target.max_hp && target.max_hp > 0) target.hp = target.max_hp;
      r.handled = true;
      break;
    }
    case E::BuffAv:
      r.amount = sp->amount_min;
      target.av += sp->amount_min;
      r.handled = true;
      break;
    case E::BuffDv:
      r.amount = sp->amount_min;
      target.dv += sp->amount_min;
      r.handled = true;
      break;
    case E::BuffAc:
      r.amount = sp->amount_min;
      target.ac += sp->amount_min;
      r.handled = true;
      break;
    case E::BuffStr:
      // +STR 透過提升傷害修正反映(SDA:STR→傷害修正)。直接加到 dmg_bonus。
      r.amount = sp->amount_min;
      target.dmg_bonus += str_damage_bonus(sp->amount_min);
      r.handled = true;
      break;
    case E::BuffDex:
      // +DEX 對 AV/DV 都有幫助(手冊 p24)。SDA base AV/DV = DEX/4 → 各 +DEX/4。
      r.amount = sp->amount_min;
      target.av += sp->amount_min / 4;
      target.dv += sp->amount_min / 4;
      r.handled = true;
      break;
    case E::DebuffAvDv:
      // -AV/-DV(蟲害);amount_min 存正值,套用為負。
      r.amount = sp->amount_min;
      target.av -= sp->amount_min;
      target.dv -= sp->amount_min;
      r.handled = true;
      break;
    case E::Control:
      // 控制類(逃跑/迷失/停止前進…):手冊未給數值結算 → 只扣 Power,標 TODO。
      r.note = "control: TODO (flee/disorient/halt)";
      r.handled = false;
      break;
    case E::Utility:
      // 工具類(光源/召喚/感測/補給…):戰鬥外或未結算 → 只扣 Power,標 TODO。
      r.note = "utility: TODO (light/summon/sense/charge)";
      r.handled = false;
      break;
  }
  r.target_hp_after = target.hp;
  return r;
}

}  // namespace dw::game
