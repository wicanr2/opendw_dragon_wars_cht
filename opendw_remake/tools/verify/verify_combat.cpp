// verify_combat — 戰鬥切片的確定性 PASS/FAIL 驗證(ctest)。
//
// 三組對拍(均 deterministic):
//  A. 怪物萃取:從 bundle/monsters/monsters.bin 載入,逐筆名字與 attr[0x0B](sprite 基底)
//     對 **oracle 名單**(opendw monster_info.cpp 走訪 res31 的輸出 / doc 26)逐項相等。
//     —— 此為真 oracle 對拍(opendw 確實會解出這些名字)。
//  B. RNG:CombatRng 對「op_4D PRNG 演算法」(opendw op_prng @0x4132 的忠實移植,
//     即 remake VM vm_state/interpreter op4D_prng)逐步相等。本檔內聯一份等價參考實作,
//     證明 CombatRng 與 oracle 演算法在固定 seed/tick 下序列一致。
//  C. 結算可重現:固定 seed + 固定隊伍 + 固定怪物,跑 N 回合,兩次執行逐回合
//     (命中/傷害/HP)完全一致 → 證明結算路徑確定性(數值真值待 bytecode 逆出,見 combat.hpp)。
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

  std::printf("\n%s\n", g_fail == 0 ? "verify_combat: ALL PASS" : "verify_combat: FAIL");
  return g_fail == 0 ? 0 : 1;
}
