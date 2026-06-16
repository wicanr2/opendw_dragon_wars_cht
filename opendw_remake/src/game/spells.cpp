// spells — 法術表 + 施法結算實作。對齊狀態見 spells.hpp 檔頭。
//
// 法術表建構原則(**數值權威依據 = docs/58_MAGIC_REFERENCE.md,fraterrisus 攻略 v3.0**):
//   • id 對齊 fraterrisus 法術索引(docs/44 §3);L/H/D/S/M 連號(見各條 note58)。
//   • POW / 目標 / 範圍 / 類型 / 傷害骰式 **一律以 docs/58 為準**(比手冊完整,含
//     召喚屬性與攻擊判定);真值層級 = remake 設計(grounded 攻略),非官方手冊、非
//     bytecode oracle,誠實標示(見 spells.hpp 檔頭)。
//   • 傷害/治療以 dice_count d dice_sides 表(docs/58 骰式);變動消耗(var.)法術以
//     PerPoint 表「每點」骰式(如 Inferno 1d4/pt),投入上限 = 2×魔法技能 ranks。
//   • Zap/Debuff 攻擊類 zap=true,走 zap_attack_roll 判定(1d16+2 roll-under,miss 半傷)。
//   • amount_min/amount_max 對 Damage/PerPoint 為「單份骰式」下/上界(= count、count*sides),
//     供既有範圍測試引用;實際傷害由 dice_count/dice_sides(+ 投入點數)擲出。
#include "game/spells.hpp"

#include <algorithm>

namespace dw::game {

namespace {

using S = SpellSchool;
using E = SpellEffect;
using T = SpellTarget;
using K = ControlKind;

// 全域法術表(數值權威 = docs/58)。每條:
//   {id, school, name_key, effect, target, power, var, amount_min, amount_max,
//    dice_count, dice_sides, zap, control, note58 [, summon]}。
// name_key 用英文法術名,對拍 assets/i18n/*/spells.tsv。
const std::vector<SpellDef> kSpells = {
    // ── A. 初級法術 Low Magic(docs/58)──
    {0x00, S::Low, "Mage Fire", E::Damage, T::OneEnemy, 2, false, 1, 8, 1, 8, true, K::None,
     "58 Mage Fire 1d8/30'/1 enemy/POW 2/Zap"},
    {0x01, S::Low, "Disarm", E::Control, T::OneEnemy, 4, false, 0, 0, 0, 0, false, K::Disarm,
     "58 Disarm/30'/1 enemy/POW 4/Debuff — 卸目標武裝(部分怪不可卸;remake 降徒手骰)"},
    {0x02, S::Low, "Charm", E::BuffAv, T::OneAlly, 3, false, 1, 1, 1, 4, false, K::None,
     "58 Charm 治 1d4 + 整場 +1 AV/1 ally/POW 3/Buff — 治療量 dice 1d4"},
    {0x03, S::Low, "Luck", E::BuffDv, T::OneAlly, 3, false, 2, 2, 0, 0, false, K::None,
     "58 Luck 整場 +2 DV/1 ally/POW 3/Buff"},
    {0x04, S::Low, "Lesser Heal", E::Heal, T::OneAlly, 2, false, 1, 4, 1, 4, false, K::None,
     "58 Lesser Heal 1d4 hp/1 ally/POW 2/Heal"},
    {0x05, S::Low, "Mage Light", E::Utility, T::AllAllies, 1, true, 0, 0, 0, 0, false, K::None,
     "58 Mage Light 光源 3hr/pt/var./Misc — 探索態(terrain.hpp SP_MageLight)"},

    // ── B. 高級法術 High Magic(docs/58)──
    {0x06, S::High, "Fire Light", E::PowerScaled, T::OneEnemy, 1, true, 1, 6, 1, 6, true, K::None,
     "58 Fire Light 1d6 hp/pt/30'/1 enemy/var./Zap — 每點 1d6,上限 2×ranks"},
    {0x07, S::High, "Elvar's Fire", E::Damage, T::GroupEnemy, 6, false, 2, 12, 2, 6, true, K::None,
     "58 Elvar's Fire 2d6/30'/group/POW 6/Zap"},
    {0x08, S::High, "Poog's Vortex", E::Damage, T::GroupEnemy, 11, false, 4, 24, 4, 6, true, K::None,
     "58 Poog's Vortex 4d6/20'/group/POW 11/Zap"},
    {0x09, S::High, "Ice Chill", E::PowerScaled, T::OneEnemy, 1, true, 1, 4, 1, 4, true, K::None,
     "58 Ice Chill 1d4 hp/pt/50'/1 enemy/var./Zap — 每點 1d4,上限 2×ranks"},
    {0x0A, S::High, "Big Chill", E::Damage, T::AllEnemy, 15, false, 4, 24, 4, 6, true, K::None,
     "58 Big Chill 4d6/30'/all/POW 15/Zap"},
    {0x0B, S::High, "Dazzle", E::Control, T::OneEnemy, 3, false, 0, 0, 0, 0, false, K::Daze,
     "58 Dazzle 敵下回合 miss/30'/1 enemy/POW 3/Debuff — 控制:Daze"},
    {0x0C, S::High, "Mystic Might", E::BuffStr, T::OneAlly, 4, false, 15, 15, 0, 0, false, K::None,
     "58 Mystic Might 整場 +15 STR/1 ally/POW 4/Buff"},
    {0x0D, S::High, "Reveal Glamour", E::Control, T::GroupEnemy, 2, false, 0, 0, 0, 0, false, K::Dispel,
     "58 Reveal Glamour 破幻象(僅魔法學院測驗)/40'/POW 2/Misc — 控制:Dispel(戰鬥 N/A)"},
    {0x0E, S::High, "Sala's Swift", E::BuffDex, T::OneAlly, 8, false, 8, 8, 0, 0, false, K::None,
     "58 Sala's Swift 整場 +8 DEX/1 ally/POW 8/Buff"},
    {0x0F, S::High, "Vorn's Guard", E::BuffAc, T::AllAllies, 6, false, 2, 2, 0, 0, false, K::None,
     "58 Vorn's Guard 整場 +2 AC/party/POW 6/Buff"},
    {0x10, S::High, "Cowardice", E::Control, T::GroupEnemy, 8, false, 0, 0, 0, 0, false, K::Flee,
     "58 Cowardice 敵群逃跑/60'/group/POW 8/Debuff — 控制:Flee"},
    {0x11, S::High, "Healing", E::Heal, T::OneAlly, 3, false, 1, 6, 1, 6, false, K::None,
     "58 Healing 1d6 hp/1 ally/POW 3/Heal"},
    {0x12, S::High, "Group Heal", E::Heal, T::AllAllies, 6, false, 1, 6, 1, 6, false, K::None,
     "58 Group Heal 1d6 hp/party/POW 6/Heal"},
    {0x13, S::High, "Cloak Arcane", E::BuffAc, T::AllAllies, 1, true, 2, 2, 0, 0, false, K::None,
     "58 Cloak Arcane +2 AC,持續 1hr/pt/party/var./Buff"},
    {0x14, S::High, "Sense Traps", E::Utility, T::AllAllies, 2, true, 0, 0, 0, 0, false, K::None,
     "58 Sense Traps 無視陷阱 2hr/pt/var./Misc — 探索態(terrain.hpp SP_SenseTraps)"},
    {0x15, S::High, "Air Summon", E::Utility, T::Special, 1, true, 0, 0, 0, 0, false, K::None,
     "58 Air Summon → Air Element(HP12/Dex12/Armor8/1d10)/var./Summon",
     SummonKind::AirElemental},
    {0x16, S::High, "Earth Summon", E::Utility, T::Special, 1, true, 0, 0, 0, 0, false, K::None,
     "58 Earth Summon → Earth Elemnt(HP15/Dex14/Armor10/1d20)/var./Summon",
     SummonKind::EarthElemental},
    {0x17, S::High, "Water Summon", E::Utility, T::Special, 1, true, 0, 0, 0, 0, false, K::None,
     "58 Water Summon → Water Elemnt(HP25/Dex15/Armor11/1d20)/var./Summon",
     SummonKind::WaterElemental},
    {0x18, S::High, "Fire Summon", E::Utility, T::Special, 1, true, 0, 0, 0, 0, false, K::None,
     "58 Fire Summon → Fire Element(HP35/Dex18/Armor15/2d20)/var./Summon",
     SummonKind::FireElemental},

    // ── D. 德魯伊法術 Druid Magic(docs/58)──
    {0x19, S::Druid, "Death Curse", E::Damage, T::OneEnemy, 6, false, 3, 18, 3, 6, true, K::None,
     "58 Death Curse 3d6/40'/1 enemy/POW 6/Zap"},
    {0x1A, S::Druid, "Fire Blast", E::Damage, T::GroupEnemy, 12, false, 4, 24, 4, 6, true, K::None,
     "58 Fire Blast 4d6/30'/group/POW 12/Zap"},
    {0x1B, S::Druid, "Insect Plague", E::DebuffAvDv, T::GroupEnemy, 4, false, 2, 2, 0, 0, true, K::None,
     "58 Insect Plague 整場 −2 AV、−2 DV/60'/group/POW 4/Debuff"},
    {0x1C, S::Druid, "Whirl Wind", E::Control, T::GroupEnemy, 4, false, 0, 0, 0, 0, false, K::Daze,
     "58 Whirl Wind 把敵群推後 30'/40'/group/POW 4/Debuff — 控制:Daze(推開=跳過)"},
    {0x1D, S::Druid, "Scare", E::BuffAv, T::AllAllies, 4, false, 2, 2, 0, 0, false, K::None,
     "58 Scare 整場 +2 AV(命中對手群即生效,適用全體)/20'/party/POW 4/Buff"},
    {0x1E, S::Druid, "Brambles", E::Control, T::GroupEnemy, 5, false, 0, 0, 0, 0, false, K::Daze,
     "58 Brambles 敵下回合 miss/60'/group/POW 5/Debuff — 控制:Daze"},
    {0x1F, S::Druid, "Creater Healing", E::Heal, T::OneAlly, 4, false, 1, 6, 1, 6, false, K::None,
     "58 Greater Healing 1d6 hp/1 ally/POW 4/Heal"},
    {0x20, S::Druid, "Cure All", E::Heal, T::AllAllies, 6, false, 1, 8, 1, 8, false, K::None,
     "58 Cure All 1d8 hp(最佳群補)/party/POW 6/Heal"},
    {0x21, S::Druid, "Create Wall", E::Utility, T::Special, 5, false, 0, 0, 0, 0, false, K::None,
     "58 Create Wall 修復泥神神廟用/POW 5/Misc — 探索態(terrain.hpp SP_CreateWall)"},
    {0x22, S::Druid, "Soften Stone", E::Utility, T::Special, 6, false, 0, 0, 0, 0, false, K::None,
     "58 Soften Stone 通過地牢障礙/POW 6/Misc — 探索態(terrain.hpp SP_SoftenStone)"},
    {0x23, S::Druid, "Invoke Spirit", E::Utility, T::Special, 1, true, 0, 0, 0, 0, false, K::None,
     "58 Invoke Spirit → Spirit(HP13/Dex18/Aura12/3d10)/var./Summon",
     SummonKind::Spirit},
    {0x24, S::Druid, "Beast Call", E::Utility, T::Special, 1, true, 0, 0, 0, 0, false, K::None,
     "58 Beast Call → Beast(HP13/Dex16/Fur7/1d12)/var./Summon",
     SummonKind::Beast},
    {0x25, S::Druid, "Wood Spirit", E::Utility, T::Special, 1, true, 0, 0, 0, 0, false, K::None,
     "58 Wood Spirit → Wood Spirit(HP19/Dex16/Bark9/1d12)/var./Summon",
     SummonKind::WoodSpirit},

    // ── C. 太陽法術 Sun Magic(docs/58)──
    {0x26, S::Sun, "Sun Stroke", E::PowerScaled, T::OneEnemy, 1, true, 1, 8, 1, 8, true, K::None,
     "58 Sun Stroke 1d8 hp/pt/20'/1 enemy/var./Zap — 每點 1d8,上限 2×ranks"},
    {0x27, S::Sun, "Exorcism", E::Damage, T::GroupEnemy, 5, false, 6, 36, 6, 6, true, K::None,
     "58 Exorcism 6d6(僅不死)/50'/group/POW 5/Zap"},
    {0x28, S::Sun, "Rage of Mithras", E::PowerScaled, T::OneEnemy, 1, true, 1, 6, 1, 6, true, K::None,
     "58 Rage of Mithras 1d6 hp/pt/70'/1 enemy/var./Zap — 每點 1d6,上限 2×ranks"},
    {0x29, S::Sun, "Wrath of Mithras", E::PowerScaled, T::GroupEnemy, 1, true, 1, 4, 1, 4, true, K::None,
     "58 Wrath of Mithras 1d4 hp/pt/90'/group/var./Zap(玩家學不到,怪會用)"},
    {0x2A, S::Sun, "Fire Storm", E::Damage, T::AllEnemy, 20, false, 6, 36, 6, 6, true, K::None,
     "58 Fire Storm 6d6/60'/all/POW 20/Zap"},
    {0x2B, S::Sun, "Inferno", E::PowerScaled, T::AllEnemy, 1, true, 1, 4, 1, 4, true, K::None,
     "58 Inferno 1d4 hp/pt/40'/all/var./Zap(全場最佳 zap)— 每點 1d4,上限 2×ranks"},
    {0x2C, S::Sun, "Holy Aim", E::BuffAv, T::AllAllies, 5, false, 2, 2, 0, 0, false, K::None,
     "58 Holy Aim 整場 +2 AV/party/POW 5/Buff"},
    {0x2D, S::Sun, "Battle Power", E::BuffStr, T::AllAllies, 8, false, 10, 10, 0, 0, false, K::None,
     "58 Battle Power 整場 +10 STR/party/POW 8/Buff"},
    {0x2E, S::Sun, "Column of Fire", E::Control, T::GroupEnemy, 5, false, 0, 0, 0, 0, false, K::Daze,
     "58 Column of Fire 阻止敵群前進(一次)/40'/group/POW 5/Debuff — 控制:Daze"},
    {0x2F, S::Sun, "Mithra's Bless", E::BuffDv, T::AllAllies, 5, false, 3, 3, 0, 0, false, K::None,
     "58 Mithras' Bless 整場 +3 DV/party/POW 5/Buff"},
    {0x30, S::Sun, "Light Flash", E::Control, T::GroupEnemy, 6, false, 0, 0, 0, 0, false, K::Daze,
     "58 Light Flash 敵下回合 miss(彩蛋)/50'/group/POW 6/Debuff — 控制:Daze"},
    {0x31, S::Sun, "Armor of Light", E::BuffDv, T::OneAlly, 6, false, 2, 2, 0, 0, false, K::None,
     "58 Armor of Light 整場 +2 DV(手冊寫 AC 是錯的)/1 ally/POW 6/Buff"},
    {0x32, S::Sun, "Sun Light", E::Heal, T::OneAlly, 3, false, 1, 6, 1, 6, false, K::None,
     "58 Sun Light 1d6 hp/1 ally/POW 3/Heal"},
    {0x33, S::Sun, "Heal", E::Heal, T::OneAlly, 4, false, 1, 8, 1, 8, false, K::None,
     "58 Heal 1d8 hp/1 ally/POW 4/Heal"},
    {0x34, S::Sun, "Major Heal", E::Heal, T::AllAllies, 6, false, 1, 6, 1, 6, false, K::None,
     "58 Major Healing 1d6 hp/party/POW 6/Heal"},
    {0x35, S::Sun, "Charger", E::Utility, T::Item, 8, false, 0, 0, 0, 0, false, K::None,
     "58 Charger 為法術物品加 1 charge/1 item/POW 8/Misc — 探索態(物品系統未對接,受阻)"},
    {0x36, S::Sun, "Disarm Trap", E::Utility, T::Special, 1, true, 0, 0, 0, 0, false, K::None,
     "58 Disarm Trap 無視陷阱 2hr/pt/var./Misc — 探索態(terrain.hpp SP_DisarmTrap)"},
    {0x37, S::Sun, "Guidance", E::Utility, T::AllAllies, 1, true, 0, 0, 0, 0, false, K::None,
     "58 Guidance UI 加指南針 3hr/pt/var./Misc — 探索態(UI 指南針未對接,受阻)"},
    {0x38, S::Sun, "Radiance", E::Utility, T::AllAllies, 1, true, 0, 0, 0, 0, false, K::None,
     "58 Radiance 光源 2hr/pt(比 Mage Light 範圍長)/40'/var./Misc — 探索態"},
    {0x39, S::Sun, "Summon Salamander", E::Utility, T::Special, 1, true, 0, 0, 0, 0, false, K::None,
     "58 Summon Salamander → Salamander(HP23/Dex24/Scales10/1d10)/var./Summon",
     SummonKind::Salamander},

    // ── E. 其他法術 Miscellaneous Magic(docs/58)──
    {0x3A, S::Misc, "Zak's Speed", E::BuffDex, T::AllAllies, 10, false, 15, 15, 0, 0, false, K::None,
     "58 Zak's Speed 整場 +15 DEX/party/POW 10/Buff"},
    {0x3B, S::Misc, "Kill Ray", E::Damage, T::OneEnemy, 15, false, 10, 80, 0, 0, true, K::None,
     "58 Kill Ray 10–80 hp/50'/1 enemy/POW 15/Zap — 平坦區間(非標準骰式)"},
    {0x3C, S::Misc, "Prison", E::Control, T::GroupEnemy, 8, false, 0, 0, 0, 0, false, K::Daze,
     "58 Prison 整場阻止目標前進/60'/group/POW 8/Buff-Debuff — 控制:Daze"},
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

ZapHit zap_attack_roll(int caster_int, int magic_ranks, const Combatant& target,
                       CombatRng& rng) {
  // docs/58:1d16+2 roll-under。命中 ⟺ roll ≤ 12 + ranks + INT − DV。
  //   用 rng.below(16)∈[0,16) +2 → roll∈[2,17](對齊「1d16+2」,1d16∈[1,16] 近似)。
  //   AC 不參與(與近戰 13+AV−(DV+AC) 區別)。dodge_dv 計入目標當前 DV(Combatant.dv 已含)。
  ZapHit h;
  h.roll = 2 + static_cast<int>(rng.below(16));
  h.need = 12 + magic_ranks + caster_int - (target.dv + target.dodge_dv);
  h.hit = (h.roll <= h.need);
  return h;
}

// 內部:擲一條傷害骰式 count d sides;count<=0 退回 [amount_min,amount_max] 平坦區間
//   (供 Kill Ray 10–80 這類非標準骰式)。
static int roll_damage_dice(const SpellDef& sp, CombatRng& rng) {
  if (sp.dice_count > 0 && sp.dice_sides > 0)
    return rng.roll(sp.dice_count, sp.dice_sides);
  int span = sp.amount_max - sp.amount_min;
  return sp.amount_min + (span > 0 ? static_cast<int>(rng.below(
                                         static_cast<std::uint16_t>(span + 1)))
                                   : 0);
}

// 內部:把已擲出的傷害經 Zap 攻擊判定套到 target(命中全傷、miss 半傷,docs/58)。
static void apply_zap_damage(const SpellDef& sp, int caster_int, int magic_ranks,
                             int raw_dmg, Combatant& target, CombatRng& rng,
                             CastResult& r) {
  r.is_zap = true;
  ZapHit h = zap_attack_roll(caster_int, magic_ranks, target, rng);
  r.zap_hit = h.hit;
  r.zap_roll = h.roll;
  r.zap_need = h.need;
  int dmg = h.hit ? raw_dmg : raw_dmg / 2;  // docs/58:miss 仍吃半傷(向下取整)
  r.amount = dmg;
  target.hp -= dmg;
  if (target.hp <= 0) {
    target.hp = 0;
    target.status |= 0x01;
    r.target_died = true;
  }
  r.handled = true;
}

CastResult cast_spell(std::uint8_t spell_id, int caster_power, int caster_str,
                      Combatant& target, CombatRng& rng, int caster_int,
                      int magic_ranks, int power_points) {
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
  // 扣 Power:固定法術 = power_cost;variable_power = 實際投入點數(>=1,上限由呼叫端依
  //   2×ranks 夾;此處再保守夾 [1, caster_power])。
  int pts = power_points < 1 ? 1 : power_points;
  if (sp->variable_power) {
    if (pts > caster_power) pts = caster_power;
    r.power_spent = pts;
  } else {
    r.power_spent = sp->power_cost;
  }

  switch (sp->effect) {
    case E::Damage: {
      // 固定骰式傷害(docs/58 NdM,Kill Ray 為平坦區間),走 Zap 攻擊判定(miss 半傷)。
      int raw = roll_damage_dice(*sp, rng);
      apply_zap_damage(*sp, caster_int, magic_ranks, raw, target, rng, r);
      break;
    }
    case E::PowerScaled: {
      // docs/58「NdM hp/pt」:每投入 1 點 Power 擲一份 dice_count d dice_sides。
      //   投入點數 = pts(已夾 >=1);上限 = 2×ranks(呼叫端先夾,此處再保守夾)。
      //   先擲全部點數的傷害(確定性),再一次 Zap 攻擊判定(miss 半傷)。
      int cap = magic_ranks > 0 ? 2 * magic_ranks : pts;  // ranks=0(舊呼叫端)不額外夾
      int use = pts > cap ? cap : pts;
      if (use < 1) use = 1;
      r.power_spent = sp->variable_power ? use : sp->power_cost;
      int raw = 0;
      for (int i = 0; i < use; ++i) raw += rng.roll(sp->dice_count, sp->dice_sides);
      apply_zap_damage(*sp, caster_int, magic_ranks, raw, target, rng, r);
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
      // Charm(0x02)docs/58:除 +1 AV 外另治 1d4 hp。以 dice_count/dice_sides 表治療骰。
      if (sp->dice_count > 0 && sp->dice_sides > 0) {
        int heal = rng.roll(sp->dice_count, sp->dice_sides);
        target.hp += heal;
        if (target.hp > target.max_hp && target.max_hp > 0) target.hp = target.max_hp;
        r.amount = heal;  // 回報治療量(AV 加值固定為法術定義 +1)
      }
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
    case E::DebuffAvDv: {
      // 蟲災(Insect Plague,docs/58):整場 −2 AV、−2 DV;Debuff 類走 Zap 攻擊判定。
      //   命中 → 套滿 −2/−2;miss → 半效(−1/−1,向下取整;docs/58「miss 吃半傷」推及 debuff)。
      r.is_zap = true;
      ZapHit h = zap_attack_roll(caster_int, magic_ranks, target, rng);
      r.zap_hit = h.hit; r.zap_roll = h.roll; r.zap_need = h.need;
      int amt = h.hit ? sp->amount_min : sp->amount_min / 2;
      r.amount = amt;
      target.av -= amt;
      target.dv -= amt;
      r.handled = true;
      break;
    }
    case E::Control: {
      // 控制類(grounded 手冊「效果」欄;持續回合為 remake 設計,見 kControlDazzleTurns)。
      //   作用於 target 的控制狀態(dazzle_turns/fled/dmg_*),不改 STUN。
      //   group/all 目標由呼叫端對每隻怪各呼叫一次(同傷害類慣例)。
      r.control = sp->control;
      switch (sp->control) {
        case ControlKind::Daze:
          // 眩目/閃光/火燄柱/圍困/龍捲風/荊棘:目標 N 回合無法行動(迷失/停止前進/推開/障礙)。
          target.dazzle_turns += kControlDazzleTurns;
          r.dazzle_turns = kControlDazzleTurns;
          r.note = "control: daze";
          r.handled = true;
          break;
        case ControlKind::Flee:
          // 膽怯術:使敵人逃跑 → 逐出戰鬥(不再參戰)。逃走怪不計入我方擊殺 XP(見 combat_loop)。
          target.fled = true;
          r.target_fled = true;
          r.note = "control: flee";
          r.handled = true;
          break;
        case ControlKind::Disarm:
          // 解除武裝:手冊未給數值 → remake 設計:目標傷害降為徒手骰(kUnarmed*),修正歸零。
          target.dmg_dice = kUnarmedDice;
          target.dmg_sides = kUnarmedSides;
          target.dmg_bonus = 0;
          r.note = "control: disarm";
          r.handled = true;
          break;
        case ControlKind::Dispel:
          // 幻影現形:驅散幻影,無戰鬥數值意義(N/A)。視為已結算的 no-op。
          r.note = "control: dispel (N/A in combat)";
          r.handled = true;
          break;
        case ControlKind::None:
          r.note = "control: unspecified (TODO)";
          r.handled = false;
          break;
      }
      break;
    }
    case E::Utility: {
      // 工具類分流(grounded 手冊 docs/33 效果欄):
      //   • 召喚類(0x15-0x18/0x23/0x24/0x25/0x39):回填 r.summon,由 combat_loop 加臨時
      //     友方;cast_spell 本身不改 target(召喚不作用於敵人)→ handled=true。
      //   • 地形/光源/補給(Mage Light/Sense/Disarm Trap/Soften Stone/Create Wall/Charger…):
      //     戰鬥內無數值意義或於探索層結算 → 仍 handled=false(只扣 Power),由探索層處理。
      SummonKind sk = summon_kind_of(spell_id);
      if (sk != SummonKind::None) {
        r.summon = sk;
        r.note = "summon: ally created";
        r.handled = true;  // 召喚已「成功施放」(實際加入戰鬥由 combat_loop::cast 處理)
      } else {
        r.note = "utility: explore/terrain (light/sense/charge/wall) — handled outside combat";
        r.handled = false;
      }
      break;
    }
  }
  r.target_hp_after = target.hp;
  return r;
}

SummonKind summon_kind_of(std::uint8_t spell_id) {
  const SpellDef* sp = find_spell(spell_id);
  return sp ? sp->summon : SummonKind::None;
}

Combatant make_summon(SummonKind kind, int caster_str) {
  // 召喚物屬性 = **docs/58 召喚表精確值**(HP / Dex / Armor / 武器傷害骰)。
  //   AV/DV = Dex/4(docs/58「AV/DV 由 Dex 推,同怪物 DEX/4 慣例」);AC = Armor;
  //   武器骰直接照表(Air 1d10、Earth 1d20、Fire 2d20…)。真值層級 = remake 設計
  //   (grounded docs/58 攻略,非 bytecode oracle)。確定性(不耗 RNG)。
  // caster_str:傷害修正(STR/5,對齊近戰 str_damage_bonus);呼應攻略「力量影響效果」,
  //   不改攻略表本身的骰式/HP/Armor。
  // HP / Dex / Armor / 骰 → {hp, dex, armor, dice, sides}。
  struct SummonStats { const char* name; int hp; int dex; int armor; int dice; int sides; };
  SummonStats st;
  switch (kind) {
    case SummonKind::AirElemental:  st = {"Air Elemental",  12, 12,  8, 1, 10}; break;
    case SummonKind::EarthElemental:st = {"Earth Elemental",15, 14, 10, 1, 20}; break;
    case SummonKind::WaterElemental:st = {"Water Elemental",25, 15, 11, 1, 20}; break;
    case SummonKind::FireElemental: st = {"Fire Elemental", 35, 18, 15, 2, 20}; break;
    case SummonKind::Beast:         st = {"Summoned Beast", 13, 16,  7, 1, 12}; break;
    case SummonKind::WoodSpirit:    st = {"Wood Spirit",    19, 16,  9, 1, 12}; break;
    case SummonKind::Spirit:        st = {"Invoked Spirit", 13, 18, 12, 3, 10}; break;
    case SummonKind::Salamander:
      // docs/58:很危險的咒語(可能反噬施法者);反噬機率/數值攻略未量化 → 不臆造,
      //   只給攻略表的召喚物屬性。
      st = {"Salamander", 23, 24, 10, 1, 10}; break;
    case SummonKind::None:
    default:                        st = {"Summon", 12, 12, 0, 1, 6}; break;
  }
  Combatant u;
  u.is_player = true;   // 我方陣營
  u.summoned = true;    // 戰鬥結束消失
  u.name = st.name;
  u.hp = u.max_hp = st.hp;
  u.av = st.dex / 4;    // docs/58:AV/DV = Dex/4(同怪物慣例)
  u.dv = st.dex / 4;
  u.ac = st.armor;
  u.dmg_dice = st.dice;
  u.dmg_sides = st.sides;
  u.dmg_bonus = str_damage_bonus(caster_str);  // STR 修正(對齊近戰)
  return u;
}

}  // namespace dw::game
