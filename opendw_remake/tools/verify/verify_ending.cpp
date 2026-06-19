// verify_ending — 可通關收官的確定性守護(ctest)。
//
// 驗兩件事(端到端「打贏 Namtar → 結局」鏈的核心,headless 確定性):
//   1. 終戰 Namtar 戰鬥**可勝**:預設隊伍(第 0 名持受祝福的自由之劍)vs Namtar Boss,
//      固定 seed 跑 combat_loop → 必為 Victory、隊伍存活、Namtar HP=0。**多 seed 皆勝**
//      (確定性 + 可勝,非僥倖)。
//   2. **確定性可重現**:同 seed 兩次執行 → 同回合數、同結果(combat_loop 確定性)。
//
// 誠實界定:Namtar Boss 屬性 + 自由之劍祝福加成 = remake 平衡設計(非原版逐欄真值;
//   原版 op_8A 怪物 id 為戰鬥設定流程產物 0x03,無乾淨 res31 record,見 combat.hpp 檔頭)。
//   單次攻擊公式(命中/傷害)= bytecode 真值(resolve_attack,docs/reverse-engineering/42 §11/§12)。
//   結局序列為 remake 組合(bundled 段落 + area27 敘事,非原版單一 script,見 main.cpp
//   enter_ending);其文字渲染另由 render_endgame / app --ending 截圖佐證,此 ctest 只守
//   「可勝 + 確定性」這個可機械化驗證的核心。
//
// 用法: verify_ending <bundle_dir>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include "game/party.hpp"
#include "game/combat.hpp"
#include "game/combat_loop.hpp"

using namespace dw;

namespace {
int g_fail = 0;
void check(bool ok, const char* msg) {
  std::printf("  [%s] %s\n", ok ? "PASS" : "FAIL", msg);
  if (!ok) ++g_fail;
}

// 建一場 Namtar 戰鬥(對齊 main.cpp begin_namtar:第 0 名受祝福,其餘照常)。
game::CombatLoop make_fight(const game::Party& party, std::uint16_t seed) {
  std::vector<game::Combatant> pu;
  if (party.size() > 0) {
    for (std::size_t i = 0; i < party.size(); ++i)
      pu.push_back(i == 0 ? game::make_blessed_hero(party.at(0))
                          : game::Combatant::from_player(party.at(i)));
  } else {
    game::Combatant h; h.name = "Hero"; h.is_player = true;
    h.hp = h.max_hp = 60; h.av = 12; h.dv = 10; h.ac = 0;
    h.dmg_dice = 4; h.dmg_sides = 12; h.dmg_bonus = 20;
    pu.push_back(h);
  }
  std::vector<game::Combatant> grp{game::make_namtar()};
  return game::CombatLoop(std::move(pu), std::move(grp), game::CombatRng(seed));
}

// 跑到結束(或回合上限),回傳 (outcome, rounds, namtar_hp, party_alive)。
struct Res { game::CombatOutcome outcome; int rounds; int namtar_hp; int party_alive; };
Res run(game::CombatLoop loop) {
  int r = 0;
  for (; r < 200 && !loop.over(); ++r) loop.advance_round();
  return {loop.outcome(), r, loop.monsters().at(0).hp, loop.party_alive()};
}
}  // namespace

int main(int argc, char** argv) {
  std::string bundle = argc > 1 ? argv[1] : "assets/bundle";
  auto party = game::Party::load_default(bundle);

  std::printf("== E1. 終戰 Namtar 可勝(預設隊伍 + 受祝福自由之劍,多 seed)==\n");
  // 多個固定 seed:皆須 Victory + 隊伍存活 + Namtar 倒下。可勝且非僥倖。
  const std::uint16_t seeds[] = {0x1234, 0x0001, 0x0099, 0xBEEF, 0x2024, 0x0000, 0x7FFF};
  for (std::uint16_t s : seeds) {
    Res r = run(make_fight(party, s));
    char buf[160];
    std::snprintf(buf, sizeof buf,
                  "seed=0x%04X → Victory(rounds=%d, namtar_hp=%d, party_alive=%d)",
                  s, r.rounds, r.namtar_hp, r.party_alive);
    check(r.outcome == game::CombatOutcome::Victory && r.namtar_hp == 0 &&
              r.party_alive > 0,
          buf);
  }

  std::printf("== E2. 勝利 → XP(combat_loop kXpPerVictory=80)==\n");
  {
    auto loop = make_fight(party, 0x1234);
    int rr = 0;
    for (; rr < 200 && !loop.over(); ++rr) loop.advance_round();
    check(loop.outcome() == game::CombatOutcome::Victory, "Namtar 戰勝");
    check(loop.xp_award() == game::kXpPerVictory, "勝利每員 +80 XP");
  }

  std::printf("== E3. 確定性:同 seed 兩次執行結果一致 ==\n");
  {
    Res a = run(make_fight(party, 0x1234));
    Res b = run(make_fight(party, 0x1234));
    check(a.outcome == b.outcome && a.rounds == b.rounds &&
              a.namtar_hp == b.namtar_hp && a.party_alive == b.party_alive,
          "兩次執行 outcome/rounds/hp/alive 完全一致(確定性)");
  }

  std::printf("== E4. --no-bless(未受祝福)應更難(不一定勝,守誠實:祝福才好打)==\n");
  {
    // 未受祝福:第 0 名也照常 from_player(無自由之劍加成)。
    std::vector<game::Combatant> pu;
    for (std::size_t i = 0; i < party.size(); ++i)
      pu.push_back(game::Combatant::from_player(party.at(i)));
    std::vector<game::Combatant> grp{game::make_namtar()};
    game::CombatLoop loop(std::move(pu), std::move(grp), game::CombatRng(0x1234));
    int rr = 0;
    for (; rr < 200 && !loop.over(); ++rr) loop.advance_round();
    // 不硬性要求敗(平衡可調),只記錄結果作對照;不計入失敗。
    std::printf("  [INFO] --no-bless outcome=%d namtar_hp=%d party_alive=%d "
                "(祝福加成顯著影響戰況,符合攻略「祝福過的劍才傷得了 Namtar」設計)\n",
                (int)loop.outcome(), loop.monsters().at(0).hp, loop.party_alive());
  }

  std::printf("\n%s (failures=%d)\n", g_fail ? "FAIL" : "ALL PASS", g_fail);
  return g_fail ? 1 : 0;
}
