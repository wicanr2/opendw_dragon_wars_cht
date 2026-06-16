// verify_spells — 法術系統的確定性 PASS/FAIL 驗證(ctest)。
//
// 數值權威依據 = **docs/58_MAGIC_REFERENCE.md(fraterrisus 攻略 v3.0)**;真值層級
//   = remake 設計(grounded 攻略),非 opendw bytecode oracle(見 spells.hpp 檔頭)。
//
// 對拍:
//  A.  法術表規模/覆蓋:筆數 == 預期、id 連續無洞、五大 school 皆有。
//  B.  抽樣效果值對拍 docs/58:Mage Fire 1d8、Lesser Heal 1d4、Fire Light 1d6/pt(PerPoint)、
//        Healing 1d6、Insect Plague −2 AV/DV、Sun Stroke 1d8/pt、Battle Power +10 STR、
//        Kill Ray 10–80、Zak's Speed +15 DEX、Fire Storm 6d6、Inferno 1d4/pt、
//        Sala's Swift POW 8、Heal(Sun) 1d8、Armor of Light = +2 DV(非 AC)。
//  C.  bitfield 走訪:character_knows_spell 對拍人工設定 bit。
//  D.  施法結算:扣 Power 正確、傷害(命中)落範圍、致死、buff/debuff 方向、控制、工具。
//  E.  Zap 攻擊判定(docs/58):命中吃全傷、miss 吃半傷;高 INT/ranks → 必中,負門檻 → 必失。
//  F.  PerPoint 每點骰式 + 2×ranks 上限。
//  G.  確定性:固定 seed 兩次施法序列逐筆一致。
//
// 用法:verify_spells <bundle_dir>(bundle 未用,保留介面一致)。退出碼:0=PASS。
#include "game/spells.hpp"
#include "game/combat.hpp"
#include "game/party.hpp"

#include <cstdio>
#include <string>
#include <vector>

using namespace dw::game;

namespace {
int g_fail = 0;
void check(bool cond, const char* what) {
  std::printf("  [%s] %s\n", cond ? "PASS" : "FAIL", what);
  if (!cond) g_fail++;
}
const SpellDef* sp(std::uint8_t id) { return find_spell(id); }

// 用高 INT/ranks(必中)對某傷害法術連擲 N 次,回傳是否全落在 [lo,hi](命中=全傷)。
bool hit_range_within(std::uint8_t id, int lo, int hi, int trials) {
  CombatRng rng(0x1234, 0);
  bool ok = true;
  for (int i = 0; i < trials; ++i) {
    Combatant t;
    t.is_player = false;
    t.hp = t.max_hp = 100000;  // 大 HP 避免截斷
    t.dv = 0;
    // caster_int=99, ranks=99 → 門檻極高,roll∈[2,17] 必 ≤ 門檻 → 必中(全傷)。
    CastResult r = cast_spell(id, 999, 20, t, rng, /*int=*/99, /*ranks=*/99, /*pts=*/1);
    if (!r.ok || !r.handled || !r.zap_hit) { ok = false; break; }
    if (r.amount < lo || r.amount > hi) ok = false;
  }
  return ok;
}
}  // namespace

int main(int argc, char** argv) {
  (void)argc; (void)argv;

  // ── A. 法術表規模 / 覆蓋 ──────────────────────────────────────────────
  std::printf("== A. spell table size / coverage ==\n");
  const auto& tbl = spell_table();
  std::printf("  spell count = %zu\n", tbl.size());
  check(tbl.size() == 61, "spell count == 61 (id 0x00..0x3C)");
  {
    bool contiguous = true;
    for (std::size_t i = 0; i < tbl.size(); ++i)
      if (tbl[i].id != static_cast<std::uint8_t>(i)) contiguous = false;
    check(contiguous, "ids contiguous 0x00..0x3C, table-order == id");
  }
  {
    bool L=false,H=false,D=false,Sn=false,M=false;
    for (const auto& s : tbl) {
      if (s.school==SpellSchool::Low) L=true;
      if (s.school==SpellSchool::High) H=true;
      if (s.school==SpellSchool::Druid) D=true;
      if (s.school==SpellSchool::Sun) Sn=true;
      if (s.school==SpellSchool::Misc) M=true;
    }
    check(L&&H&&D&&Sn&&M, "all 5 schools present (L/H/D/S/M)");
  }

  // ── B. 抽樣效果值對拍 docs/58 ─────────────────────────────────────────
  std::printf("== B. sampled effect values vs docs/58 ==\n");
  if (auto* s = sp(0x00))
    check(s->dice_count==1 && s->dice_sides==8 && s->power_cost==2 && s->zap &&
              s->effect==SpellEffect::Damage && s->target==SpellTarget::OneEnemy,
          "0x00 Mage Fire = 1d8 / POW 2 / OneEnemy / Damage / Zap");
  else check(false, "0x00 Mage Fire present");
  if (auto* s = sp(0x04))
    check(s->dice_count==1 && s->dice_sides==4 && s->power_cost==2 &&
              s->effect==SpellEffect::Heal && !s->zap,
          "0x04 Lesser Heal = 1d4 / POW 2 / Heal");
  else check(false, "0x04 Lesser Heal present");
  if (auto* s = sp(0x06))
    check(s->effect==SpellEffect::PowerScaled && s->dice_count==1 && s->dice_sides==6 &&
              s->variable_power && s->zap && s->target==SpellTarget::OneEnemy,
          "0x06 Fire Light = 1d6 hp/pt / var. / OneEnemy / Zap");
  else check(false, "0x06 Fire Light present");
  if (auto* s = sp(0x11))
    check(s->dice_count==1 && s->dice_sides==6 && s->power_cost==3 &&
              s->effect==SpellEffect::Heal,
          "0x11 Healing = 1d6 / POW 3 / Heal");
  else check(false, "0x11 Healing present");
  if (auto* s = sp(0x1B))
    check(s->effect==SpellEffect::DebuffAvDv && s->amount_min==2 && s->power_cost==4 && s->zap,
          "0x1B Insect Plague = -2 AV/DV / POW 4 / Debuff(Zap)");
  else check(false, "0x1B Insect Plague present");
  if (auto* s = sp(0x26))
    check(s->effect==SpellEffect::PowerScaled && s->dice_count==1 && s->dice_sides==8 && s->zap,
          "0x26 Sun Stroke = 1d8 hp/pt / Zap");
  else check(false, "0x26 Sun Stroke present");
  if (auto* s = sp(0x2A))
    check(s->dice_count==6 && s->dice_sides==6 && s->power_cost==20 &&
              s->effect==SpellEffect::Damage && s->target==SpellTarget::AllEnemy,
          "0x2A Fire Storm = 6d6 / POW 20 / AllEnemy");
  else check(false, "0x2A Fire Storm present");
  if (auto* s = sp(0x2B))
    check(s->effect==SpellEffect::PowerScaled && s->dice_count==1 && s->dice_sides==4 &&
              s->target==SpellTarget::AllEnemy,
          "0x2B Inferno = 1d4 hp/pt / AllEnemy");
  else check(false, "0x2B Inferno present");
  if (auto* s = sp(0x2D))
    check(s->effect==SpellEffect::BuffStr && s->amount_min==10 && s->power_cost==8 &&
              s->target==SpellTarget::AllAllies,
          "0x2D Battle Power = +10 STR / POW 8 / AllAllies");
  else check(false, "0x2D Battle Power present");
  if (auto* s = sp(0x31))
    check(s->effect==SpellEffect::BuffDv && s->amount_min==2 && s->power_cost==6,
          "0x31 Armor of Light = +2 DV (NOT AC) / POW 6");
  else check(false, "0x31 Armor of Light present");
  if (auto* s = sp(0x33))
    check(s->dice_count==1 && s->dice_sides==8 && s->power_cost==4 &&
              s->effect==SpellEffect::Heal,
          "0x33 Heal(Sun) = 1d8 / POW 4");
  else check(false, "0x33 Heal(Sun) present");
  if (auto* s = sp(0x0E))
    check(s->effect==SpellEffect::BuffDex && s->amount_min==8 && s->power_cost==8,
          "0x0E Sala's Swift = +8 DEX / POW 8");
  else check(false, "0x0E Sala's Swift present");
  if (auto* s = sp(0x3B))
    check(s->amount_min==10 && s->amount_max==80 && s->power_cost==15 &&
              s->effect==SpellEffect::Damage && s->zap,
          "0x3B Kill Ray = 10-80 / POW 15 / Zap");
  else check(false, "0x3B Kill Ray present");
  if (auto* s = sp(0x3A))
    check(s->effect==SpellEffect::BuffDex && s->amount_min==15 && s->power_cost==10,
          "0x3A Zak's Speed = +15 DEX / POW 10");
  else check(false, "0x3A Zak's Speed present");

  // ── C. bitfield 走訪 ─────────────────────────────────────────────────
  std::printf("== C. spells[60-67] bitfield traversal ==\n");
  {
    CharacterRecord c;
    c.spells[0] |= (1u << 0);  // 0x00
    c.spells[1] |= (1u << 2);  // 0x0A
    c.spells[4] |= (1u << 6);  // 0x26
    check(character_knows_spell(c, 0x00), "knows 0x00 (byte0 bit0)");
    check(character_knows_spell(c, 0x0A), "knows 0x0A (byte1 bit2)");
    check(character_knows_spell(c, 0x26), "knows 0x26 (byte4 bit6)");
    check(!character_knows_spell(c, 0x01), "does NOT know 0x01");
    check(!character_knows_spell(c, 0x3C), "does NOT know 0x3C");
    auto cs1 = castable_spells(c, 1);    // 只夠日灼(可變,>=1)
    auto cs15 = castable_spells(c, 15);  // 全部 3 條
    check(cs1.size()==1 && cs1[0]==0x26, "at Power 1: only Sun Stroke castable (var., cost 1)");
    check(cs15.size()==3, "at Power 15: all 3 known castable");
  }

  // ── D. 施法結算:扣 Power / 範圍 / buff / 控制 / 工具 ─────────────────
  std::printf("== D. cast resolution ==\n");
  {
    // 魔火扣 POW 2,命中傷害落 [1,8]。caster_int/ranks 高 → 必中。
    CombatRng rng(0x1234, 0);
    Combatant t; t.is_player=false; t.hp=t.max_hp=50; t.dv=0;
    CastResult r = cast_spell(0x00, 10, 20, t, rng, 99, 99, 1);
    check(r.ok && r.power_spent==2, "Mage Fire spends POW 2");
    check(r.is_zap && r.zap_hit && r.amount>=1 && r.amount<=8, "Mage Fire hit damage in [1,8]");
    check(t.hp == 50 - r.amount, "target STUN reduced by damage");
  }
  {
    // POW 不足 → ok=false。
    CombatRng rng(0x1234, 0);
    Combatant t; t.hp=t.max_hp=50;
    CastResult r = cast_spell(0x3B, 5, 20, t, rng);  // 死光需 15
    check(!r.ok && r.power_spent==0, "Kill Ray rejected when Power < 15");
  }
  {
    // 治療回復 STUN 不超 max。
    CombatRng rng(0x1234, 0);
    Combatant t; t.is_player=true; t.hp=3; t.max_hp=6;
    CastResult r = cast_spell(0x11, 10, 20, t, rng);
    check(r.ok && r.amount>=1 && r.amount<=6, "Healing amount in [1,6]");
    check(t.hp<=t.max_hp && t.hp>3, "STUN restored, clamped to max");
  }
  {
    // 致死:小 HP 怪挨大寒冰(4d6,必中)→ 死亡且 hp 夾 0。
    CombatRng rng(0x9999, 0);
    Combatant t; t.hp=t.max_hp=3; t.dv=0;
    CastResult r = cast_spell(0x0A, 20, 20, t, rng, 99, 99, 1);
    check(r.ok && r.zap_hit && r.target_died && t.hp==0 && (t.status&0x01),
          "Big Chill kills 3-HP target (hit), hp clamped 0, dead bit set");
  }
  {
    // 命中傷害範圍掃描(必中):Mage Fire 1d8、Elvar's Fire 2d6、Fire Storm 6d6、Kill Ray 10-80。
    check(hit_range_within(0x00, 1, 8, 200), "Mage Fire 200x hit within [1,8]");
    check(hit_range_within(0x07, 2, 12, 200), "Elvar's Fire 200x hit within [2,12]");
    check(hit_range_within(0x2A, 6, 36, 200), "Fire Storm 200x hit within [6,36]");
    check(hit_range_within(0x3B, 10, 80, 200), "Kill Ray 200x hit within [10,80]");
  }
  {
    // 治療範圍掃描(Heal 不走判定)。
    CombatRng rng(0x1234, 0);
    bool ok = true;
    for (int i = 0; i < 200; ++i) {
      Combatant t; t.is_player=true; t.hp=1; t.max_hp=100000;
      CastResult r = cast_spell(0x04, 999, 20, t, rng);  // Lesser Heal 1d4
      if (!r.ok || r.amount < 1 || r.amount > 4) ok = false;
    }
    check(ok, "Lesser Heal 200x within [1,4]");
  }
  {
    // buff/debuff 套用方向。
    CombatRng rng(0x1234, 0);
    Combatant ally; ally.is_player=true; ally.av=5; ally.dv=5; ally.ac=0;
    cast_spell(0x03, 10, 20, ally, rng);   // 祈福 +2 DV
    check(ally.dv==7, "Luck +2 DV applied");
    cast_spell(0x2C, 10, 20, ally, rng);   // 聖戰光輝 +2 AV
    check(ally.av==7, "Holy Aim +2 AV applied");
    Combatant ally2; ally2.is_player=true; ally2.dv=5;
    cast_spell(0x31, 10, 20, ally2, rng);  // 光的武裝 +2 DV(docs/58:非 AC)
    check(ally2.dv==7 && ally2.ac==0, "Armor of Light +2 DV (not AC)");
    // 蟲害 -2 AV/DV(必中)。
    Combatant enemy; enemy.av=5; enemy.dv=5;
    cast_spell(0x1B, 10, 20, enemy, rng, 99, 99, 1);
    check(enemy.av==3 && enemy.dv==3, "Insect Plague -2 AV/DV applied (hit)");
  }
  {
    // 解除武裝 0x01:扣 POW 4、handled、control=Disarm、傷害降為徒手骰、HP 不變。
    CombatRng rng(0x1234, 0);
    Combatant t; t.hp=t.max_hp=50; t.dmg_dice=3; t.dmg_sides=6; t.dmg_bonus=5;
    CastResult r = cast_spell(0x01, 10, 20, t, rng);
    check(r.ok && r.power_spent==4 && r.handled && r.control==ControlKind::Disarm &&
              t.hp==50 && t.dmg_dice==kUnarmedDice && t.dmg_sides==kUnarmedSides && t.dmg_bonus==0,
          "Disarm: POW 4, weapon->unarmed dice, no HP change");
  }
  {
    // 工具:法師魔光(handled=false,只扣 Power);var. → 投入 1 點。
    CombatRng rng(0x1234, 0);
    Combatant t; t.hp=t.max_hp=50;
    CastResult r = cast_spell(0x05, 10, 20, t, rng);
    check(r.ok && r.power_spent==1 && !r.handled && t.hp==50,
          "Mage Light (utility) spends Power 1, not resolved, no HP change");
  }
  {
    // 眩目 / 膽怯 / 幻影現形(控制三態,沿用既有語意)。
    CombatRng rng(0x1234, 0);
    Combatant t1; t1.hp=t1.max_hp=50;
    CastResult r1 = cast_spell(0x0B, 10, 20, t1, rng);
    check(r1.ok && r1.handled && r1.control==ControlKind::Daze &&
              t1.dazzled() && t1.hp==50, "Dazzle applies daze, no HP change");
    Combatant t2; t2.is_player=false; t2.hp=t2.max_hp=50;
    CastResult r2 = cast_spell(0x10, 20, 20, t2, rng);
    check(r2.ok && r2.handled && r2.control==ControlKind::Flee && t2.fled &&
              !t2.in_combat() && t2.alive() && t2.hp==50, "Cowardice makes target flee");
    Combatant t3; t3.hp=t3.max_hp=50; t3.av=5;
    CastResult r3 = cast_spell(0x0D, 10, 20, t3, rng);
    check(r3.ok && r3.handled && r3.control==ControlKind::Dispel && t3.hp==50 && t3.av==5,
          "Reveal Glamour = Dispel (N/A in combat)");
  }

  // ── E. Zap 攻擊判定(docs/58:1d16+2 roll-under,門檻 12+ranks+INT−DV,miss 半傷)──
  std::printf("== E. zap attack judgement ==\n");
  {
    // 高門檻(INT 99/ranks 99)→ 必中,全傷。
    CombatRng rng(0x55, 0);
    Combatant t; t.hp=t.max_hp=100000; t.dv=0;
    CastResult r = cast_spell(0x00, 10, 20, t, rng, 99, 99, 1);
    check(r.is_zap && r.zap_hit && r.amount>=1 && r.amount<=8, "high INT/ranks always hits (full dmg)");
  }
  {
    // 負門檻(INT 0/ranks 0,目標 DV 99)→ roll∈[2,17] 恆 > 門檻 → 必失,半傷。
    CombatRng rng(0x55, 0);
    Combatant t; t.hp=t.max_hp=100000; t.dv=99;
    CastResult r = cast_spell(0x00, 10, 20, t, rng, 0, 0, 1);
    check(r.is_zap && !r.zap_hit, "very high DV target -> miss");
    // miss 半傷:1d8 命中 [1,8],半傷 = floor/2 ∈ [0,4]。
    check(r.amount>=0 && r.amount<=4, "miss deals half damage (floor) in [0,4]");
  }
  {
    // miss 半傷 < 命中全傷(同骰須一致;以多次抽樣統計平均 miss < 平均 hit)。
    long hit_sum=0, miss_sum=0; int n=300;
    CombatRng rh(0x77,0), rm(0x77,0);
    for (int i=0;i<n;i++){ Combatant t; t.hp=t.max_hp=100000; t.dv=0;
      CastResult r=cast_spell(0x2A,999,20,t,rh,99,99,1); hit_sum+=r.amount; }
    for (int i=0;i<n;i++){ Combatant t; t.hp=t.max_hp=100000; t.dv=99;
      CastResult r=cast_spell(0x2A,999,20,t,rm,0,0,1); miss_sum+=r.amount; }
    check(miss_sum*2 <= hit_sum + n,  // miss≈hit/2 → miss*2 ≲ hit(+取整誤差)
          "Fire Storm: average miss damage ~= half of hit damage");
  }

  // ── F. PerPoint:每點骰式 + 2×ranks 上限 ──────────────────────────────
  std::printf("== F. per-point variable damage + 2x ranks cap ==\n");
  {
    // Inferno 1d4/pt,ranks=3 → 上限 6 點;投入 6 點(必中)→ 6 份 1d4 = [6,24]。
    CombatRng rng(0x123,0);
    bool ok=true; int seen_max=0;
    for (int i=0;i<200;i++){ Combatant t; t.hp=t.max_hp=100000; t.dv=0;
      CastResult r=cast_spell(0x2B, 999, 20, t, rng, 99, 3, /*pts=*/6);
      if(!r.zap_hit){ok=false;break;} if(r.amount<6||r.amount>24)ok=false;
      if(r.amount>seen_max)seen_max=r.amount; }
    check(ok, "Inferno 6 pts (ranks3) hit within [6,24] = 6x 1d4");
  }
  {
    // 上限夾:ranks=2 → 上限 4 點;要求投入 10 點 → 實扣 4。
    CombatRng rng(0x123,0);
    Combatant t; t.hp=t.max_hp=100000; t.dv=0;
    CastResult r=cast_spell(0x2B, 999, 20, t, rng, 99, /*ranks=*/2, /*pts=*/10);
    check(r.power_spent==4, "Inferno caps power_spent at 2x ranks (=4)");
  }
  {
    // 1 點(預設)→ 1 份骰式 [1,4]。
    CombatRng rng(0x123,0);
    bool ok=true;
    for (int i=0;i<200;i++){ Combatant t; t.hp=t.max_hp=100000; t.dv=0;
      CastResult r=cast_spell(0x2B, 999, 20, t, rng, 99, 99, 1);
      if(!r.zap_hit){ok=false;break;} if(r.amount<1||r.amount>4)ok=false; }
    check(ok, "Inferno 1 pt hit within [1,4]");
  }

  // ── H. 召喚屬性對拍 docs/58 召喚表 ───────────────────────────────────
  std::printf("== H. summon stats vs docs/58 ==\n");
  {
    // AV/DV = Dex/4;HP/AC/骰照表。caster_str=0 → dmg_bonus = str_damage_bonus(0)。
    Combatant air = make_summon(SummonKind::AirElemental, 0);
    check(air.hp==12 && air.max_hp==12 && air.av==12/4 && air.dv==12/4 && air.ac==8 &&
              air.dmg_dice==1 && air.dmg_sides==10 && air.is_player && air.summoned,
          "Air Element HP12/Dex12->AV/DV3/Armor8/1d10");
    Combatant fire = make_summon(SummonKind::FireElemental, 0);
    check(fire.hp==35 && fire.av==18/4 && fire.ac==15 && fire.dmg_dice==2 && fire.dmg_sides==20,
          "Fire Element HP35/Dex18->AV/DV4/Armor15/2d20");
    Combatant spirit = make_summon(SummonKind::Spirit, 0);
    check(spirit.hp==13 && spirit.av==18/4 && spirit.ac==12 && spirit.dmg_dice==3 && spirit.dmg_sides==10,
          "Spirit HP13/Dex18/Aura12/3d10");
    Combatant sala = make_summon(SummonKind::Salamander, 0);
    check(sala.hp==23 && sala.av==24/4 && sala.ac==10 && sala.dmg_dice==1 && sala.dmg_sides==10,
          "Salamander HP23/Dex24/Scales10/1d10");
    // summon_kind_of:0x15→Air、0x39→Salamander、0x00→None。
    check(summon_kind_of(0x15)==SummonKind::AirElemental &&
              summon_kind_of(0x39)==SummonKind::Salamander &&
              summon_kind_of(0x00)==SummonKind::None,
          "summon_kind_of maps spell id -> kind");
  }

  // ── G. 確定性 ─────────────────────────────────────────────────────────
  std::printf("== G. determinism ==\n");
  {
    auto run = [](std::vector<int>& out) {
      CombatRng rng(0x4D4D, 7);
      const std::uint8_t seq[] = {0x00, 0x07, 0x3B, 0x06, 0x0A};
      for (std::uint8_t id : seq) {
        Combatant t; t.is_player=false; t.hp=t.max_hp=100000; t.dv=0;
        CastResult r = cast_spell(id, 999, 17, t, rng, 50, 10, 1);
        out.push_back(r.amount);
        out.push_back(r.zap_hit ? 1 : 0);
      }
    };
    std::vector<int> a, b; run(a); run(b);
    check(a==b && a.size()==10, "same seed -> identical cast amount/hit sequence");
  }

  std::printf("\n%s (failures=%d)\n", g_fail==0 ? "ALL PASS" : "FAIL", g_fail);
  return g_fail==0 ? 0 : 1;
}
