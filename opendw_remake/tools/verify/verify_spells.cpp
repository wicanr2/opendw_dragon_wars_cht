// verify_spells — 法術系統的確定性 PASS/FAIL 驗證(ctest)。
//
// 對拍(均依手冊 docs/33 grounded;非 opendw byte-for-byte oracle,見 spells.hpp 檔頭):
//  A.  法術表規模/覆蓋:筆數 == 預期、id 連續無洞、五大 school(L/H/D/S/M)皆有。
//  B.  抽樣效果值對拍手冊:
//        魔火 1-8/Power 2、輕微醫療 1-4/Power 2、治療(H) 1-6/Power 3、
//        火燄之光 Str×1-6/可變 Power、日灼術 Str×1-8、死光 10-80/Power 15、
//        蟲害 -2 AV/DV、戰鬥力 +10 STR、迅捷術 +8 DEX。
//  C.  bitfield 走訪:character_knows_spell 對拍人工設定的 spells[60-67] bit。
//  D.  施法扣 Power 正確、傷害/治療落在手冊範圍內、致死標記正確。
//  E.  確定性:固定 seed 兩次施法序列逐筆一致。
//
// 用法:verify_spells <bundle_dir>(bundle 目前未用,保留與其他 verify 一致的介面)。
// 退出碼:0=PASS,非 0=FAIL。
#include "game/spells.hpp"
#include "game/combat.hpp"
#include "game/party.hpp"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

using namespace dw::game;

namespace {
int g_fail = 0;
void check(bool cond, const char* what) {
  std::printf("  [%s] %s\n", cond ? "PASS" : "FAIL", what);
  if (!cond) g_fail++;
}

// 取一條法術定義(找不到回 nullptr 並讓 caller check 失敗)。
const SpellDef* sp(std::uint8_t id) { return find_spell(id); }

// 用固定 seed 對某傷害/治療法術連擲 N 次,回傳 [min,max] 觀察區間與是否全落在 [lo,hi]。
bool range_within(std::uint8_t id, int lo, int hi, int caster_str, int trials,
                  int& obs_min, int& obs_max) {
  CombatRng rng(0x1234, 0);
  obs_min = 1 << 30;
  obs_max = -(1 << 30);
  bool ok = true;
  for (int i = 0; i < trials; ++i) {
    Combatant t;
    t.is_player = false;
    t.hp = t.max_hp = 100000;  // 大 HP 避免致死截斷,純看擲出量
    CastResult r = cast_spell(id, 999, caster_str, t, rng);
    if (!r.ok || !r.handled) { ok = false; break; }
    if (r.amount < obs_min) obs_min = r.amount;
    if (r.amount > obs_max) obs_max = r.amount;
    if (r.amount < lo || r.amount > hi) ok = false;
  }
  return ok;
}
}  // namespace

int main(int argc, char** argv) {
  (void)argc;
  (void)argv;

  // ── A. 法術表規模 / 覆蓋 ──────────────────────────────────────────────
  std::printf("== A. spell table size / coverage ==\n");
  const auto& tbl = spell_table();
  std::printf("  spell count = %zu\n", tbl.size());
  check(tbl.size() == 61, "spell count == 61 (id 0x00..0x3C)");
  // id 連續 0x00..0x3C 無洞。
  {
    bool contiguous = true;
    for (std::size_t i = 0; i < tbl.size(); ++i)
      if (tbl[i].id != static_cast<std::uint8_t>(i)) contiguous = false;
    check(contiguous, "ids contiguous 0x00..0x3C, table-order == id");
  }
  // 五大 school 皆有。
  {
    bool L = false, H = false, D = false, Sn = false, M = false;
    for (const auto& s : tbl) {
      if (s.school == SpellSchool::Low) L = true;
      if (s.school == SpellSchool::High) H = true;
      if (s.school == SpellSchool::Druid) D = true;
      if (s.school == SpellSchool::Sun) Sn = true;
      if (s.school == SpellSchool::Misc) M = true;
    }
    check(L && H && D && Sn && M, "all 5 schools present (L/H/D/S/M)");
  }

  // ── B. 抽樣效果值對拍手冊 ─────────────────────────────────────────────
  std::printf("== B. sampled effect values vs manual (docs/33) ==\n");
  // 魔火 0x00:1-8 點 / Power 2 / 一個敵人 / 傷害。
  if (auto* s = sp(0x00))
    check(s->amount_min == 1 && s->amount_max == 8 && s->power_cost == 2 &&
              s->effect == SpellEffect::Damage && s->target == SpellTarget::OneEnemy,
          "0x00 Mage Fire = 1-8 / Power 2 / OneEnemy / Damage");
  else check(false, "0x00 Mage Fire present");
  // 輕微醫療 0x04:1-4 點 / Power 2 / 一個人物 / 治療。
  if (auto* s = sp(0x04))
    check(s->amount_min == 1 && s->amount_max == 4 && s->power_cost == 2 &&
              s->effect == SpellEffect::Heal && s->target == SpellTarget::OneAlly,
          "0x04 Lesser Heal = 1-4 / Power 2 / OneAlly / Heal");
  else check(false, "0x04 Lesser Heal present");
  // 火燄之光 0x06:使用力量 1-6 倍 / 可變 Power / 一個敵人。
  if (auto* s = sp(0x06))
    check(s->effect == SpellEffect::PowerScaled && s->power_mult == 6 &&
              s->variable_power && s->target == SpellTarget::OneEnemy,
          "0x06 Fire Light = PowerScaled x1-6 / variable Power / OneEnemy");
  else check(false, "0x06 Fire Light present");
  // 高級治療 0x11:1-6 點 / Power 3。
  if (auto* s = sp(0x11))
    check(s->amount_min == 1 && s->amount_max == 6 && s->power_cost == 3 &&
              s->effect == SpellEffect::Heal,
          "0x11 Healing = 1-6 / Power 3 / Heal");
  else check(false, "0x11 Healing present");
  // 蟲害 0x1B:-2 AV/DV / Power 4。
  if (auto* s = sp(0x1B))
    check(s->effect == SpellEffect::DebuffAvDv && s->amount_min == 2 &&
              s->power_cost == 4,
          "0x1B Insect Plague = -2 AV/DV / Power 4");
  else check(false, "0x1B Insect Plague present");
  // 日灼術 0x26:使用力量 1-8 倍。
  if (auto* s = sp(0x26))
    check(s->effect == SpellEffect::PowerScaled && s->power_mult == 8,
          "0x26 Sun Stroke = PowerScaled x1-8");
  else check(false, "0x26 Sun Stroke present");
  // 戰鬥力 0x2D:+10 STR / Power 8 / 整個小組。
  if (auto* s = sp(0x2D))
    check(s->effect == SpellEffect::BuffStr && s->amount_min == 10 &&
              s->power_cost == 8 && s->target == SpellTarget::AllAllies,
          "0x2D Battle Power = +10 STR / Power 8 / AllAllies");
  else check(false, "0x2D Battle Power present");
  // 死光 0x3B:10-80 點 / Power 15。
  if (auto* s = sp(0x3B))
    check(s->amount_min == 10 && s->amount_max == 80 && s->power_cost == 15 &&
              s->effect == SpellEffect::Damage,
          "0x3B Kill Ray = 10-80 / Power 15 / Damage");
  else check(false, "0x3B Kill Ray present");
  // 增加敏捷 0x3A:+15 DEX / Power 10。
  if (auto* s = sp(0x3A))
    check(s->effect == SpellEffect::BuffDex && s->amount_min == 15 &&
              s->power_cost == 10,
          "0x3A Zak's Speed = +15 DEX / Power 10");
  else check(false, "0x3A Zak's Speed present");

  // ── C. bitfield 走訪 ─────────────────────────────────────────────────
  std::printf("== C. spells[60-67] bitfield traversal ==\n");
  {
    CharacterRecord c;
    // 習得 0x00(魔火;byte0 bit0)、0x0A(大寒冰;byte1 bit2)、0x26(日灼;byte4 bit6)。
    c.spells[0] |= (1u << 0);
    c.spells[1] |= (1u << 2);
    c.spells[4] |= (1u << 6);
    check(character_knows_spell(c, 0x00), "knows 0x00 (byte0 bit0)");
    check(character_knows_spell(c, 0x0A), "knows 0x0A (byte1 bit2)");
    check(character_knows_spell(c, 0x26), "knows 0x26 (byte4 bit6)");
    check(!character_knows_spell(c, 0x01), "does NOT know 0x01");
    check(!character_knows_spell(c, 0x3C), "does NOT know 0x3C");
    // castable:Power 足夠才入列。魔火 Power2、大寒冰 Power15、日灼可變(>=1)。
    auto cs1 = castable_spells(c, 1);    // 只夠魔火(2)?不,1<2 → 只夠日灼(可變,>=1)
    auto cs15 = castable_spells(c, 15);  // 全部
    bool only_sun_at1 = (cs1.size() == 1 && cs1[0] == 0x26);
    check(only_sun_at1, "at Power 1: only Sun Stroke castable (variable, cost 1)");
    check(cs15.size() == 3, "at Power 15: all 3 known castable");
  }

  // ── D. 施法結算:扣 Power / 範圍 / 致死 ──────────────────────────────
  std::printf("== D. cast resolution: power / range / lethality ==\n");
  {
    // 魔火扣 Power 2,傷害落在 1-8。
    CombatRng rng(0x1234, 0);
    Combatant t;
    t.is_player = false; t.hp = t.max_hp = 50;
    CastResult r = cast_spell(0x00, 10, 20, t, rng);
    check(r.ok && r.power_spent == 2, "Mage Fire spends Power 2");
    check(r.amount >= 1 && r.amount <= 8, "Mage Fire damage in [1,8]");
    check(t.hp == 50 - r.amount, "target STUN reduced by damage");
  }
  {
    // Power 不足 → ok=false,不結算。
    CombatRng rng(0x1234, 0);
    Combatant t; t.hp = t.max_hp = 50;
    CastResult r = cast_spell(0x3B, 5, 20, t, rng);  // 死光需 15
    check(!r.ok && r.power_spent == 0, "Kill Ray rejected when Power < 15");
  }
  {
    // 治療回復 STUN,且不超過 max。
    CombatRng rng(0x1234, 0);
    Combatant t; t.is_player = true; t.hp = 3; t.max_hp = 6;
    CastResult r = cast_spell(0x11, 10, 20, t, rng);  // 治療 1-6
    check(r.ok && r.amount >= 1 && r.amount <= 6, "Healing amount in [1,6]");
    check(t.hp <= t.max_hp && t.hp > 3, "STUN restored, clamped to max");
  }
  {
    // 致死:小 HP 怪挨大寒冰(4-24)→ 死亡且 hp 夾 0。
    CombatRng rng(0x9999, 0);
    Combatant t; t.hp = t.max_hp = 3;
    CastResult r = cast_spell(0x0A, 20, 20, t, rng);
    check(r.ok && r.target_died && t.hp == 0 && (t.status & 0x01),
          "Big Chill kills 3-HP target, hp clamped 0, dead bit set");
  }
  {
    // 範圍掃描:抽樣傷害/治療法術全程落在手冊範圍。
    int lo, hi;
    check(range_within(0x00, 1, 8, 20, 200, lo, hi), "Mage Fire 200x within [1,8]");
    check(range_within(0x07, 2, 12, 20, 200, lo, hi), "Elvar's Fire 200x within [2,12]");
    check(range_within(0x3B, 10, 80, 20, 200, lo, hi), "Kill Ray 200x within [10,80]");
    check(range_within(0x04, 1, 4, 20, 200, lo, hi), "Lesser Heal 200x within [1,4]");
    // 力量倍率:STR=10,火燄之光 ×1-6 → [10,60]。
    check(range_within(0x06, 10, 60, 10, 200, lo, hi),
          "Fire Light (STR10) 200x within [10,60] = STRx1-6");
  }
  {
    // buff/debuff 套用方向正確。
    CombatRng rng(0x1234, 0);
    Combatant ally; ally.is_player = true; ally.av = 5; ally.dv = 5; ally.ac = 0;
    cast_spell(0x03, 10, 20, ally, rng);  // 祈福 +2 DV
    check(ally.dv == 7, "Luck +2 DV applied");
    cast_spell(0x2C, 10, 20, ally, rng);  // 聖戰光輝 +2 AV
    check(ally.av == 7, "Holy Aim +2 AV applied");
    Combatant enemy; enemy.av = 5; enemy.dv = 5;
    cast_spell(0x1B, 10, 20, enemy, rng);  // 蟲害 -2 AV/DV
    check(enemy.av == 3 && enemy.dv == 3, "Insect Plague -2 AV/DV applied");
  }
  {
    // 控制/工具類:只扣 Power,handled=false(TODO 誠實標示)。
    CombatRng rng(0x1234, 0);
    Combatant t; t.hp = t.max_hp = 50;
    CastResult r = cast_spell(0x01, 10, 20, t, rng);  // 解除武裝(控制)
    check(r.ok && r.power_spent == 4 && !r.handled && t.hp == 50,
          "Disarm (control) spends Power, not resolved (TODO), no HP change");
  }

  // ── E. 確定性 ─────────────────────────────────────────────────────────
  std::printf("== E. determinism ==\n");
  {
    auto run = [](std::vector<int>& out) {
      CombatRng rng(0x4D4D, 7);
      const std::uint8_t seq[] = {0x00, 0x07, 0x3B, 0x06, 0x0A};
      for (std::uint8_t id : seq) {
        Combatant t; t.is_player = false; t.hp = t.max_hp = 100000;
        CastResult r = cast_spell(id, 999, 17, t, rng);
        out.push_back(r.amount);
      }
    };
    std::vector<int> a, b;
    run(a);
    run(b);
    check(a == b && a.size() == 5, "same seed -> identical cast amount sequence");
  }

  std::printf("\n%s (failures=%d)\n", g_fail == 0 ? "ALL PASS" : "FAIL", g_fail);
  return g_fail == 0 ? 0 : 1;
}
