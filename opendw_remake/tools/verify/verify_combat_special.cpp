// verify_combat_special — 4 種特殊攻擊 + Dodge 的 PASS/FAIL(ctest)。
//
// 真值層級(務必誠實):特殊攻擊的命中/傷害/卸武裝/DV 修正係數 = **remake 設計
//   grounded 手冊**(opendw C 未實作戰鬥結算、res3 戰鬥 bytecode 全閉環受阻,
//   見 combat.hpp/combat_loop.hpp 檔頭)。底層單次攻擊仍走 resolve_attack(命中/傷害
//   公式 = bytecode 真值),特殊攻擊只在其上套 AV/傷害/DV 修正,**不重寫公式**。
//
// 對拍(均 deterministic):
//  A. 強力一擊(MightyBlow):命中門檻較低(AV penalty)→ 給高 AV 攻擊者確保命中時,
//     傷害比一般攻擊**多 kMightyBlowDmgBonus**(固定 seed 比對同一擲)。
//  B. 卸武裝(Disarm):命中 → target.disarmed=true 且傷害骰回退徒手(1d4),本次 damage==0
//     (打武器非身體;target.hp 不變)。
//  C. 前進(Advance):命中門檻較高(AV bonus)→ 命中率不低於一般攻擊。
//  D. 快速戰鬥(QuickFight):結算等同一般攻擊(同 seed → 同命中/傷害)。
//  E. 閃避(Dodge):actor.dodge_dv 升至 kDodgeDvBonus;敵人下一輪攻擊命中率下降
//     (被命中門檻 = 13+AV−(DV+AC+dodge_dv),dodge 後門檻更低)。
//  F. 確定性:同 seed 兩次「特殊攻擊一回合」逐結果 byte-identical。
//
// 用法:verify_combat_special <bundle_dir>;退出碼 0=PASS。
#include "game/combat.hpp"
#include "game/combat_loop.hpp"
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

// 造一個確定命中的攻擊者(高 AV)與固定靶(高 HP,不會死,純看傷害/狀態)。
Combatant make_attacker(int av, int dmg_dice, int dmg_sides, int dmg_bonus) {
  Combatant u;
  u.name = "Hero";
  u.is_player = true;
  u.hp = u.max_hp = 100;
  u.av = av; u.dv = 4; u.ac = 0;
  u.dmg_dice = dmg_dice; u.dmg_sides = dmg_sides; u.dmg_bonus = dmg_bonus;
  return u;
}
Combatant make_dummy(int hp, int dv, int ac) {
  Combatant u;
  u.name = "Robber";
  u.is_player = false;
  u.hp = u.max_hp = hp;
  u.av = 2; u.dv = dv; u.ac = ac;
  u.dmg_dice = 1; u.dmg_sides = 8; u.dmg_bonus = 2;  // 有武器(8 面骰)→ 觀察卸武裝降為 1d4
  return u;
}
}  // namespace

int main(int argc, char** argv) {
  (void)argc; (void)argv;

  std::printf("== A. 強力一擊:命中時傷害比一般攻擊多 kMightyBlowDmgBonus ==\n");
  {
    // 同 seed、同攻擊者/目標,分別跑一般 vs 強力一擊;高 AV 確保兩者皆命中。
    auto run = [](bool mighty) -> AttackResult {
      Combatant atk = make_attacker(/*av=*/30, 1, 6, 0);  // av 高 → roll-under 門檻必命中
      Combatant tgt = make_dummy(/*hp=*/200, /*dv=*/0, /*ac=*/0);
      CombatRng rng(0x1234, 0);
      return mighty ? resolve_special_attack(atk, tgt, SpecialAttack::MightyBlow, rng)
                    : resolve_attack(atk, tgt, rng);
    };
    AttackResult normal = run(false);
    AttackResult mighty = run(true);
    check(normal.hit && mighty.hit, "高 AV 下一般攻擊與強力一擊皆命中");
    check(mighty.damage == normal.damage + kMightyBlowDmgBonus,
          "強力一擊傷害 = 一般攻擊 + kMightyBlowDmgBonus(同 seed 同骰)");
  }

  std::printf("== B. 卸武裝:命中 → disarmed、傷害骰降為徒手、本次 damage==0 ==\n");
  {
    Combatant atk = make_attacker(/*av=*/30, 1, 6, 0);
    Combatant tgt = make_dummy(/*hp=*/50, /*dv=*/0, /*ac=*/0);
    int hp_before = tgt.hp;
    int sides_before = tgt.dmg_sides;
    CombatRng rng(0x1234, 0);
    AttackResult r = resolve_special_attack(atk, tgt, SpecialAttack::Disarm, rng);
    check(r.hit, "卸武裝命中(高 AV)");
    check(tgt.disarmed, "命中 → target.disarmed=true");
    check(tgt.dmg_sides == kUnarmedSides && tgt.dmg_dice == kUnarmedDice,
          "傷害骰回退徒手(1d4)");
    check(r.damage == 0, "卸武裝本次不造成身體傷害(damage==0)");
    check(tgt.hp == hp_before, "target.hp 不變(打武器非身體)");
    check(sides_before != kUnarmedSides, "前提:原目標有武器(非 1d4)");
  }

  std::printf("== C. 前進:命中門檻較高(AV +bonus)→ 邊際情形比一般攻擊更易命中 ==\n");
  {
    // 設計一個「一般攻擊剛好打不到、前進剛好打得到」的邊際命中情形以驗 AV bonus 生效。
    //   roll-under:HIT ⟺ roll ≤ 13+AV−(DV+AC)。固定 seed → 固定 roll;掃 AV 找邊際。
    //   這裡用統計:對多 seed 比較命中數,前進命中數 ≥ 一般。
    int adv_hits = 0, norm_hits = 0;
    for (std::uint16_t s = 1; s <= 200; ++s) {
      {
        Combatant atk = make_attacker(/*av=*/5, 1, 6, 0);
        Combatant tgt = make_dummy(200, /*dv=*/6, /*ac=*/0);
        CombatRng rng(s, 0);
        if (resolve_attack(atk, tgt, rng).hit) norm_hits++;
      }
      {
        Combatant atk = make_attacker(/*av=*/5, 1, 6, 0);
        Combatant tgt = make_dummy(200, /*dv=*/6, /*ac=*/0);
        CombatRng rng(s, 0);
        if (resolve_special_attack(atk, tgt, SpecialAttack::Advance, rng).hit) adv_hits++;
      }
    }
    std::printf("    advance_hits=%d normal_hits=%d (kAdvanceAvBonus=%d)\n",
                adv_hits, norm_hits, kAdvanceAvBonus);
    check(adv_hits >= norm_hits, "前進命中次數 ≥ 一般攻擊(AV bonus 生效)");
    check(adv_hits > norm_hits, "前進在邊際命中情形確實多命中(AV bonus 有意義)");
  }

  std::printf("== D. 快速戰鬥:結算等同一般攻擊(同 seed → 同命中/傷害)==\n");
  {
    auto run = [](bool quick) -> AttackResult {
      Combatant atk = make_attacker(/*av=*/8, 2, 6, 1);
      Combatant tgt = make_dummy(200, 4, 0);
      CombatRng rng(0x4D4D, 0);
      return quick ? resolve_special_attack(atk, tgt, SpecialAttack::QuickFight, rng)
                   : resolve_attack(atk, tgt, rng);
    };
    AttackResult q = run(true), n = run(false);
    check(q.hit == n.hit && q.damage == n.damage,
          "快速戰鬥 = 一般攻擊(命中/傷害一致)");
  }

  std::printf("== E. 閃避:dodge_dv 升、敵人命中率下降 ==\n");
  {
    // 透過 CombatLoop 驗:隊伍第 0 名 Dodge → dodge_dv=kDodgeDvBonus;
    //   接著怪攻擊時命中率比未閃避時低(被命中門檻下降)。
    std::vector<Combatant> grp(1, make_attacker(/*av=*/8, 1, 4, 0));
    grp[0].is_player = false; grp[0].name = "Robber";  // 當成怪(攻擊隊伍)
    // 統計:有 Dodge vs 無 Dodge,怪在 N seed 下命中隊員的次數。
    int hits_no_dodge = 0, hits_dodge = 0;
    for (std::uint16_t s = 1; s <= 200; ++s) {
      // 無 Dodge:怪直接攻擊。
      {
        Combatant hero; hero.name = "P"; hero.is_player = true;
        hero.hp = hero.max_hp = 999; hero.av = 4; hero.dv = 4; hero.ac = 0;
        std::vector<Combatant> party{hero};
        std::vector<Combatant> mons(1, grp[0]);
        CombatLoop loop(party, mons, CombatRng(s, 0));
        loop.advance_round();
        for (const auto& e : loop.events())
          if (!e.attacker_is_player && e.hit) hits_no_dodge++;
      }
      // 有 Dodge:隊員先 Dodge(提高 DV),再讓怪攻擊一回合。
      {
        Combatant hero; hero.name = "P"; hero.is_player = true;
        hero.hp = hero.max_hp = 999; hero.av = 4; hero.dv = 4; hero.ac = 0;
        std::vector<Combatant> party{hero};
        std::vector<Combatant> mons(1, grp[0]);
        CombatLoop loop(party, mons, CombatRng(s, 0));
        AttackResult d = loop.special_attack(SpecialAttack::Dodge, true, 0, -1);
        (void)d;
        // Dodge 後隊員 dodge_dv 應為 kDodgeDvBonus。
        if (s == 1)
          check(loop.party().at(0).dodge_dv == kDodgeDvBonus,
                "Dodge → 隊員 dodge_dv = kDodgeDvBonus");
        loop.advance_round();
        for (const auto& e : loop.events())
          if (!e.attacker_is_player && e.hit) hits_dodge++;
      }
    }
    std::printf("    hits_no_dodge=%d hits_dodge=%d (kDodgeDvBonus=%d)\n",
                hits_no_dodge, hits_dodge, kDodgeDvBonus);
    check(hits_dodge < hits_no_dodge, "閃避後敵人命中次數下降");
  }

  std::printf("== F. 確定性:同 seed 兩次特殊攻擊一回合 byte-identical ==\n");
  {
    auto run = [](SpecialAttack sa) -> std::string {
      Combatant hero = make_attacker(/*av=*/8, 2, 6, 1);
      std::vector<Combatant> party{hero};
      std::vector<Combatant> mons(3, make_dummy(20, 4, 0));
      CombatLoop loop(party, mons, CombatRng(0x1234, 0));
      loop.special_attack(sa, true, 0, -1);
      loop.advance_round();
      std::string s;
      char b[160];
      for (const auto& e : loop.events()) {
        std::snprintf(b, sizeof b,
                      "E %s>%s pl=%d hit=%d dmg=%d disarm=%d dodge=%d died=%d\n",
                      e.attacker.c_str(), e.target.c_str(), (int)e.attacker_is_player,
                      (int)e.hit, e.damage, (int)e.special_disarm,
                      (int)e.special_dodge, (int)e.target_died);
        s += b;
      }
      return s;
    };
    bool all_det = true;
    for (SpecialAttack sa : {SpecialAttack::MightyBlow, SpecialAttack::Disarm,
                             SpecialAttack::Advance, SpecialAttack::QuickFight,
                             SpecialAttack::Dodge}) {
      if (run(sa) != run(sa)) all_det = false;
    }
    check(all_det, "5 種特殊攻擊各自同 seed 兩次序列一致");
  }

  std::printf("\n%s\n",
              g_fail == 0 ? "verify_combat_special: ALL PASS"
                          : "verify_combat_special: FAIL");
  return g_fail == 0 ? 0 : 1;
}
