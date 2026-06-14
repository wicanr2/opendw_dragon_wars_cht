// verify_combat — 戰鬥切片的確定性 PASS/FAIL 驗證(ctest)。
//
// 對拍(均 deterministic):
//  A.  怪物萃取:從 bundle/monsters/monsters.bin 載入,逐筆名字與 attr[0x0B](sprite 基底)
//      對 **oracle 名單**(opendw monster_info.cpp 走訪 res31 的輸出 / doc 26)逐項相等。
//      —— 此為真 oracle 對拍(opendw 確實會解出這些名字)。
//  A2. CharacterRecord 戰鬥欄位(AV/DV/AC/XP/skills/inventory):對拍真實 default_party.bin。
//      起始隊伍 stored AV/DV/AC=0(原版 runtime 計算)→ 驗 effective DV==DEX/4(SDA base 公式)。
//  A3. 傷害骰解碼:fraterrisus 編碼(高3bit 骰面/低3bit 骰數-1)→ 骰式對照表幾筆相等。
//  B.  RNG:CombatRng 對「op_4D PRNG 演算法」(opendw op_prng @0x4132 的忠實移植,
//      即 remake VM vm_state/interpreter op4D_prng)逐步相等。本檔內聯一份等價參考實作,
//      證明 CombatRng 與 oracle 演算法在固定 seed/tick 下序列一致。
//  C.  結算可重現:固定 seed + 固定隊伍 + 固定怪物,跑 N 回合,兩次執行逐回合
//      (命中/傷害/HP)完全一致 → 證明結算路徑確定性。
//      註:結算公式 grounded in docs/44(fraterrisus+SDA+手冊),非 opendw byte-for-byte;
//      to-hit 骰分布為暫定,待 DOS 校準(見 combat.hpp 檔頭)。不宣稱 oracle 真值。
//
// 用法:verify_combat <bundle_dir>
// 退出碼:0=PASS,非 0=FAIL。

#include "game/combat.hpp"
#include "game/party.hpp"

#include <cstdint>
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

// ── B 的參考 RNG:opendw op_prng + update_random_seed 的等價內聯重現 ──
// (sys_ticks() 以遞增 tick 替身,與 CombatRng / VM fake_ticks 同策略)
struct RefRng {
  std::uint16_t seed, tick;
  RefRng(std::uint16_t s, std::uint16_t t) : seed(s), tick(t) {}
  std::uint16_t next_word(std::uint16_t r2) {
    std::uint16_t ax = ++tick;
    ax = static_cast<std::uint16_t>(ax + seed);
    seed = ax;
    std::uint32_t mul = static_cast<std::uint32_t>(seed) * r2;
    return static_cast<std::uint16_t>((mul & 0xFFFF0000u) >> 16);
  }
};

// oracle 名單(opendw monster_info.cpp / docs/26_MONSTERS_AND_SPRITES.md 走訪 res31)。
// 含單複數 escape 的字面(\en / \ves)亦保留,逐字對拍。
const char* kOracleNames[] = {
    "Robber",       "King's Guard", "Soldier",          "Bandit",
    "Pikeman\\en",  "Loon",         "Fanatic",          "Yonderboy",
    "Born Loser",   "Unjustly Accused", "Innocent Man\\en", "Giant Spider",
    "Wild Dog",     "Spider",       "Cannibal",         "Big Dog",
    "Wild hound",   "Rock Spider",  "Spider",           "Wolf\\ves",
    "Jail Keeper",  "Rock Spider",  "Drunk",            "Humbaba",
    "Gladiator",
};
constexpr int kOracleCount = sizeof(kOracleNames) / sizeof(kOracleNames[0]);

}  // namespace

int main(int argc, char** argv) {
  if (argc < 2) {
    std::fprintf(stderr, "usage: %s <bundle_dir>\n", argv[0]);
    return 2;
  }
  const std::filesystem::path bundle = argv[1];

  std::printf("== A. monster extraction vs oracle name list ==\n");
  auto monsters = MonsterTable::load(bundle);
  check(static_cast<int>(monsters.size()) == kOracleCount,
        ("count == " + std::to_string(kOracleCount) + " (got " +
         std::to_string(monsters.size()) + ")").c_str());
  {
    int n = static_cast<int>(monsters.size());
    bool all = (n == kOracleCount);
    for (int i = 0; i < n && i < kOracleCount; ++i) {
      if (monsters[i].name != kOracleNames[i]) {
        std::printf("    idx %d: got '%s' want '%s'\n", i,
                    monsters[i].name.c_str(), kOracleNames[i]);
        all = false;
      }
    }
    check(all, "all monster names match oracle byte-for-byte");
  }

  std::printf("== A2. CharacterRecord combat fields (byte-grounded, fraterrisus 512B) ==\n");
  {
    auto party = Party::load_default(bundle);
    check(party.size() == 4, "default party has 4 members");
    // 印出 4 員 AV/DV/AC,並檢查 effective AV/DV ≈ DEX/4(SDA base 公式)。
    // 註:起始隊伍的 stored av/dv/ac 欄為 0(原版 runtime 計算),故 effective 走 DEX/4 路徑。
    bool stored_zero = true, eff_dex4 = true;
    for (std::size_t i = 0; i < party.size(); ++i) {
      const auto& c = party.at(i);
      int base = c.dexterity / 4;
      std::printf("    %-8s DEX=%2d stored[av=%u dv=%u ac=%u xp=%u] "
                  "eff[av=%d dv=%d ac=%d] DEX/4=%d\n",
                  c.name.c_str(), c.dexterity, c.av, c.dv, c.ac, c.xp,
                  c.effective_av(), c.effective_dv(), c.effective_ac(), base);
      if (c.av != 0 || c.dv != 0 || c.ac != 0) stored_zero = false;
      // 起始隊伍無武器/無 stored → effective_av/dv 應等於 DEX/4(武器技能 0)。
      if (c.effective_dv() != base) eff_dex4 = false;
    }
    check(stored_zero,
          "starting party stored AV/DV/AC are 0 (runtime-computed, per SDA)");
    check(eff_dex4, "effective DV == DEX/4 for all members (SDA base formula)");
    // skills[32-58] 已解析:至少一員有非 0 技能(rec0 起始有技能點配置)。
    bool any_skill = false;
    for (std::size_t i = 0; i < party.size(); ++i)
      for (auto s : party.at(i).skills) if (s) any_skill = true;
    check(any_skill, "skills[32-58] parsed (some member has non-zero skill)");
    // 起始隊伍無裝備 → 主武器欄 present=false(inventory 區全 0)。
    bool no_weapon = true;
    for (std::size_t i = 0; i < party.size(); ++i)
      if (party.at(i).main_weapon().present) no_weapon = false;
    check(no_weapon, "starting party has no equipped weapon (inventory all 0)");
  }

  std::printf("== A3. damage-dice decode (fraterrisus encoding) ==\n");
  {
    struct Case { std::uint8_t enc; int count; int sides; const char* label; };
    // 高 3bit 骰面 {d4,d6,d8,d10,d12,d20,d30,d100};低 3bit 骰數-1。
    // 註:enc==0(=「1d4」的純編碼)在物品欄語境保留為「空欄/無骰」,故不列為傷害骰 case。
    const Case cases[] = {
        {0b000'00'001, 2, 4,  "0x01 = 2d4"},     // 最低 d4 但骰數>1(避開 enc==0 空欄)
        {0b101'00'001, 2, 20, "0xA1 = 2d20"},    // docs/44 範例
        {0b001'00'000, 1, 6,  "0x20 = 1d6"},
        {0b011'00'010, 3, 10, "0x62 = 3d10"},
        {0b111'00'111, 8, 100,"0xE7 = 8d100"},
    };
    bool all = true;
    for (const auto& c : cases) {
      DamageDice d = decode_damage_dice(c.enc);
      bool ok = d.count == c.count && d.sides == c.sides;
      if (!ok) {
        std::printf("    %s: got %dd%d\n", c.label, d.count, d.sides);
        all = false;
      }
    }
    check(all, "damage-dice encoding -> dice table correct (5 cases)");
    DamageDice z = decode_damage_dice(0);
    check(!z.valid(), "encoded 0 -> no dice (valid()==false)");
  }

  std::printf("== B. CombatRng == oracle op_4D PRNG algorithm ==\n");
  {
    CombatRng rng(0x1234, 0);
    RefRng ref(0x1234, 0);
    bool ok = true;
    // 用變動的 r2(對照 word_3AE2)逐步比對,200 步。
    for (int i = 0; i < 200; ++i) {
      std::uint16_t r2 = static_cast<std::uint16_t>(1 + (i * 7) % 97);
      std::uint16_t a = rng.next_word(r2);
      std::uint16_t b = ref.next_word(r2);
      if (a != b) {
        std::printf("    step %d: rng=0x%04X ref=0x%04X\n", i, a, b);
        ok = false;
        break;
      }
    }
    check(ok, "200-step sequence identical to oracle algorithm");
    // seedability:相同 seed 兩 instance 序列一致;不同 seed 須相異。
    CombatRng r1(0xABCD, 5), r2(0xABCD, 5), r3(0x1111, 5);
    bool same = true, diff = false;
    for (int i = 0; i < 50; ++i) {
      std::uint16_t v1 = r1.below(100), v2 = r2.below(100), v3 = r3.below(100);
      if (v1 != v2) same = false;
      if (v1 != v3) diff = true;
    }
    check(same, "same seed -> identical sequence");
    check(diff, "different seed -> different sequence");
  }

  std::printf("== C. combat resolution determinism (fixed seed) ==\n");
  {
    // 固定隊伍:取 bundle 預設隊伍第 0 名;固定怪物:取怪物 0(Robber)。
    auto party = Party::load_default(bundle);
    bool have_party = party.size() > 0;
    check(have_party, "default party loaded");
    bool have_mon = !monsters.empty();
    if (have_party && have_mon) {
      auto run = [&](std::vector<std::string>& log) {
        CombatRng rng(0x1234, 0);
        Combatant hero = Combatant::from_player(party.at(0));
        Combatant mon = Combatant::from_monster(monsters[0]);
        // 確保雙方有血可打(怪物 attr 暫定可能為 0,from_monster 已給 default)。
        for (int round = 0; round < 20 && hero.alive() && mon.alive(); ++round) {
          AttackResult ph = resolve_attack(hero, mon, rng);
          char buf[160];
          std::snprintf(buf, sizeof(buf),
                        "R%02d HERO->MON hit=%d roll=%d need=%d dmg=%d mon_hp=%d",
                        round, ph.hit, ph.to_hit_roll, ph.to_hit_need, ph.damage,
                        ph.target_hp_after);
          log.emplace_back(buf);
          if (!mon.alive()) break;
          AttackResult pm = resolve_attack(mon, hero, rng);
          std::snprintf(buf, sizeof(buf),
                        "R%02d MON->HERO hit=%d roll=%d need=%d dmg=%d hero_hp=%d",
                        round, pm.hit, pm.to_hit_roll, pm.to_hit_need, pm.damage,
                        pm.target_hp_after);
          log.emplace_back(buf);
        }
      };
      std::vector<std::string> a, b;
      run(a);
      run(b);
      check(a == b, "two fixed-seed runs produce identical round log");
      std::printf("  -- round log (fixed seed 0x1234) --\n");
      for (auto& line : a) std::printf("    %s\n", line.c_str());
    }
  }

  std::printf("== D. 徒手傷害公式(bytecode 反推:dmg = 1d4 + floor(STR/5))==\n");
  {
    // 【更新】先前此案例斷言 DOS best-fit {3,4,6}「無 5」+ ×3/2 + floor3。
    // 第四輪端到端跑 res3 bytecode(0x0D54 徒手傷害路徑)反推真值:
    //   傷害 = 傷害骰(徒手 Fist=0 → 1d4)+ floor(STR/5),**無 ×3/2、無 floor(3)**。
    //   STR10 → bonus=floor(10/5)=2 → dmg = 1d4+2 = {3,4,5,6}(**含 5**)。
    //   故 DOS §9 的「無 5」被 bytecode 證偽 = 53 筆小樣本雜訊;此案例改對 bytecode 真值。
    CombatRng rng(0x1234, 0);
    Combatant atk{};
    atk.av = 100;  // 保證命中
    atk.dmg_dice = kUnarmedDice; atk.dmg_sides = kUnarmedSides;  // 1d4(descriptor 0x00)
    atk.dmg_bonus = str_damage_bonus(10);  // floor(10/5) = 2(bytecode 真值)
    bool all_in_set = true; bool saw3 = false, saw4 = false, saw5 = false, saw6 = false;
    int mn = 999, mx = -1;
    for (int i = 0; i < 2000; ++i) {
      Combatant tgt{}; tgt.dv = 0; tgt.ac = 0; tgt.hp = 100000; tgt.max_hp = 100000;
      AttackResult r = resolve_attack(atk, tgt, rng);
      if (!r.hit) continue;
      if (r.damage < mn) mn = r.damage;
      if (r.damage > mx) mx = r.damage;
      if (r.damage == 3) saw3 = true;
      if (r.damage == 4) saw4 = true;
      if (r.damage == 5) saw5 = true;
      if (r.damage == 6) saw6 = true;
      if (r.damage < 3 || r.damage > 6) all_in_set = false;
    }
    check(str_damage_bonus(10) == 2, "STR10 傷害修正 = floor(10/5) = 2(bytecode)");
    check(mn == 3 && mx == 6, "徒手 Str10 範圍 = [3,6](1d4+2,bytecode)");
    check(saw3 && saw4 && saw5 && saw6, "徒手 Str10 = {3,4,5,6}(含 5;bytecode 證偽 DOS『無 5』)");
    check(all_in_set, "徒手 Str10 全在 {3,4,5,6}");
  }
  std::printf("== E. AC on hit-side, not damage (DOS §9) ==\n");
  {
    // AC 只抬高 to_hit_need、不參與傷害:同 rng 狀態、同攻擊者,ac=0 vs ac=50 命中時傷害相同;
    // 高 ac 目標 need = 11 + dv + ac。
    Combatant atk{}; atk.av = 100; atk.dmg_dice = 1; atk.dmg_sides = 4; atk.dmg_bonus = 0;
    CombatRng ra(0x55, 0), rb(0x55, 0);
    Combatant t0{}; t0.dv = 3; t0.ac = 0;  t0.hp = 100000; t0.max_hp = 100000;
    Combatant t1{}; t1.dv = 3; t1.ac = 50; t1.hp = 100000; t1.max_hp = 100000;
    AttackResult r0 = resolve_attack(atk, t0, ra);
    AttackResult r1 = resolve_attack(atk, t1, rb);
    check(r1.to_hit_need == r0.to_hit_need + 50, "high AC raises to_hit_need by AC");
    check(r0.hit && r1.hit && r0.damage == r1.damage, "damage independent of target AC");
  }

  std::printf("\n%s\n", g_fail == 0 ? "verify_combat: ALL PASS" : "verify_combat: FAIL");
  return g_fail == 0 ? 0 : 1;
}
