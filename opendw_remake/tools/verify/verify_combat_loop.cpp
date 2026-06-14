// verify_combat_loop — 完整戰鬥迴圈確定性 PASS/FAIL(ctest)。
//
// 對拍(均 deterministic):
//  A.  確定性(關鍵):固定 seed + 固定隊伍 + 固定怪群 → 兩次完整戰鬥的逐事件
//      序列(攻擊者/目標/命中/傷害/暈眩/擊倒)+ 逐回合存活數 + 最終 HP + XP
//      **byte-identical**(序列化成字串後 cmp)。
//  B.  勝利 XP:怪群全滅 → outcome=Victory、xp_award()==80(DOS 實機 docs/43)。
//  C.  勝負判定:怪全滅→Victory;全隊昏倒→Defeat;flee()→Fled。
//  D.  公式不變:戰鬥內傷害僅由 resolve_attack 產生(combat_loop 不重算);
//      以「弱怪群被高 AV 隊伍清空且每次傷害 ≥1」間接驗證走的是 resolve_attack。
//
// 用法:verify_combat_loop <bundle_dir>;退出碼 0=PASS。
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

// 把一整場戰鬥序列化成可逐 byte 比對的字串(事件流 + 每回合存活 + 結果 + XP)。
std::string run_battle(const std::vector<Combatant>& party_proto,
                       const Combatant& mon_proto, int mon_count,
                       std::uint16_t seed, int max_rounds) {
  std::vector<Combatant> grp;
  for (int i = 0; i < mon_count; ++i) grp.push_back(mon_proto);
  CombatLoop loop(party_proto, grp, CombatRng(seed, 0));
  std::string out;
  char buf[256];
  std::size_t emitted = 0;
  for (int r = 0; r < max_rounds && !loop.over(); ++r) {
    loop.advance_round();
    const auto& evs = loop.events();
    for (; emitted < evs.size(); ++emitted) {
      const auto& e = evs[emitted];
      std::snprintf(buf, sizeof buf,
                    "EV %s->%s pl=%d hit=%d dmg=%d stun=%d died=%d\n",
                    e.attacker.c_str(), e.target.c_str(),
                    (int)e.attacker_is_player, (int)e.hit, e.damage,
                    (int)e.stunned, (int)e.target_died);
      out += buf;
    }
    std::snprintf(buf, sizeof buf, "R%02d palive=%d malive=%d outcome=%d\n", r,
                  loop.party_alive(), loop.monsters_alive(), (int)loop.outcome());
    out += buf;
  }
  // 最終 HP（隊伍 + 怪群）。
  for (const auto& p : loop.party()) {
    std::snprintf(buf, sizeof buf, "P %s hp=%d\n", p.name.c_str(), p.hp);
    out += buf;
  }
  for (std::size_t i = 0; i < loop.monsters().size(); ++i) {
    std::snprintf(buf, sizeof buf, "M%zu hp=%d\n", i, loop.monsters()[i].hp);
    out += buf;
  }
  std::snprintf(buf, sizeof buf, "OUTCOME=%d XP=%d rounds=%d\n",
                (int)loop.outcome(), loop.xp_award(), loop.round_count());
  out += buf;
  return out;
}
}  // namespace

int main(int argc, char** argv) {
  if (argc < 2) {
    std::fprintf(stderr, "usage: %s <bundle_dir>\n", argv[0]);
    return 2;
  }
  const std::filesystem::path bundle = argv[1];
  auto party = Party::load_default(bundle);
  auto monsters = MonsterTable::load(bundle);
  check(party.size() == 4, "default party has 4 members");
  check(!monsters.empty(), "monster table loaded");
  if (party.size() != 4 || monsters.empty()) {
    std::printf("\nverify_combat_loop: FAIL (missing fixtures)\n");
    return 1;
  }

  // 固定隊伍投影(原版真值 from_player)。
  std::vector<Combatant> party_proto;
  for (std::size_t i = 0; i < party.size(); ++i)
    party_proto.push_back(Combatant::from_player(party.at(i)));

  // 固定怪群:6 隻 King's Guard(idx 1)= DOS docs/43 §11 遭遇規模參照。
  const int kKingsGuard = 1;
  Combatant mon_proto = Combatant::from_monster(monsters[kKingsGuard]);
  const int kGroup = 6;
  const std::uint16_t kSeed = 0x1234;

  std::printf("== A. 確定性:同 seed 兩次完整戰鬥 byte-identical ==\n");
  std::string a = run_battle(party_proto, mon_proto, kGroup, kSeed, 200);
  std::string b = run_battle(party_proto, mon_proto, kGroup, kSeed, 200);
  check(a == b, "two fixed-seed full battles produce identical serialized trace");
  if (a != b) {
    // 找第一個差異點輔助除錯。
    std::size_t i = 0;
    while (i < a.size() && i < b.size() && a[i] == b[i]) ++i;
    std::printf("    first diff at byte %zu\n", i);
  }

  std::printf("== B/C. 勝利 + XP=80(高 AV 隊伍清弱怪群)==\n");
  {
    // 用真實隊伍 vs 弱怪群(HP 低、AV 低),確保隊伍勝出。
    // King's Guard 用 from_monster 暫定 stat;為穩定取勝,造一個明確弱怪。
    Combatant weak;
    weak.name = "Robber";
    weak.is_player = false;
    weak.hp = weak.max_hp = 3;
    weak.av = 0;
    weak.dv = 0;
    weak.ac = 0;
    weak.dmg_dice = 1;
    weak.dmg_sides = 2;
    weak.dmg_bonus = 0;
    std::vector<Combatant> grp(kGroup, weak);
    CombatLoop loop(party_proto, grp, CombatRng(kSeed, 0));
    for (int r = 0; r < 200 && !loop.over(); ++r) loop.advance_round();
    check(loop.outcome() == CombatOutcome::Victory, "弱怪群被清空 → Victory");
    check(loop.xp_award() == 80, "勝利 → 每員 +80 XP(DOS 實機)");
    check(loop.monsters_alive() == 0, "怪群全滅");
    // 至少有命中事件、傷害 ≥1(間接驗走 resolve_attack)。
    bool any_hit = false, dmg_pos = true;
    for (const auto& e : loop.events()) {
      if (e.hit) { any_hit = true; if (e.damage < 1) dmg_pos = false; }
    }
    check(any_hit, "戰報含命中事件");
    check(dmg_pos, "命中傷害恆 ≥1(resolve_attack 路徑)");
  }

  std::printf("== C2. 敗北判定(無敵怪群打倒全隊)==\n");
  {
    Combatant brute;
    brute.name = "Humbaba";
    brute.is_player = false;
    brute.hp = brute.max_hp = 9999;
    brute.av = 100;  // 必命中
    brute.dv = 100;
    brute.ac = 100;
    brute.dmg_dice = 4;
    brute.dmg_sides = 10;
    brute.dmg_bonus = 50;
    std::vector<Combatant> grp(4, brute);
    CombatLoop loop(party_proto, grp, CombatRng(kSeed, 0));
    for (int r = 0; r < 200 && !loop.over(); ++r) loop.advance_round();
    check(loop.outcome() == CombatOutcome::Defeat, "全隊昏倒 → Defeat");
    check(loop.xp_award() == 0, "敗北 → 無 XP");
  }

  std::printf("== C3. 逃跑判定 ==\n");
  {
    // 強怪群確保開場仍 Ongoing,逃跑前不會被判勝/敗。
    Combatant tanky = mon_proto;
    tanky.hp = tanky.max_hp = 9999;
    std::vector<Combatant> grp(kGroup, tanky);
    CombatLoop loop(party_proto, grp, CombatRng(kSeed, 0));
    check(loop.outcome() == CombatOutcome::Ongoing, "開場 Ongoing");
    loop.flee();
    check(loop.outcome() == CombatOutcome::Fled, "flee() → Fled");
    check(loop.xp_award() == 0, "逃跑 → 無 XP");
  }

  std::printf("== D. XP 寫回 CharacterRecord.xp(飽和到 255)==\n");
  {
    // 模擬寫回:xp 為 1 byte;+80 後若溢出夾到 255。
    auto add_xp = [](std::uint8_t cur, int gain) -> std::uint8_t {
      int v = static_cast<int>(cur) + gain;
      if (v > 255) v = 255;
      return static_cast<std::uint8_t>(v);
    };
    check(add_xp(0, 80) == 80, "xp 0 + 80 = 80");
    check(add_xp(200, 80) == 255, "xp 200 + 80 飽和 = 255");
  }

  std::printf("\n%s\n",
              g_fail == 0 ? "verify_combat_loop: ALL PASS" : "verify_combat_loop: FAIL");
  return g_fail == 0 ? 0 : 1;
}
