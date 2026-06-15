// probe_namtar_balance — 載入預設隊伍 + Namtar Boss,跑確定性戰鬥,印每員初值與結果。
// 供調平衡(remake 設計):確認「受祝福隊伍」在固定 seed 下可勝。
#include <cstdio>
#include "game/party.hpp"
#include "game/combat.hpp"
#include "game/combat_loop.hpp"
using namespace dw;
int main(int argc, char** argv) {
  std::string bundle = argc > 1 ? argv[1] : "assets/bundle";
  unsigned seed = argc > 2 ? (unsigned)strtoul(argv[2], nullptr, 0) : 0x1234;
  auto party = game::Party::load_default(bundle);
  std::vector<game::Combatant> pu;
  for (std::size_t i = 0; i < party.size(); ++i) {
    auto c = (i == 0) ? game::make_blessed_hero(party.at(0))
                      : game::Combatant::from_player(party.at(i));
    std::printf("party[%zu] %-10s hp=%d av=%d dv=%d ac=%d dmg=%dd%d+%d\n",
                i, c.name.c_str(), c.hp, c.av, c.dv, c.ac, c.dmg_dice, c.dmg_sides, c.dmg_bonus);
    pu.push_back(c);
  }
  auto nam = game::make_namtar();
  std::printf("NAMTAR     hp=%d av=%d dv=%d ac=%d dmg=%dd%d+%d\n",
              nam.hp, nam.av, nam.dv, nam.ac, nam.dmg_dice, nam.dmg_sides, nam.dmg_bonus);
  std::vector<game::Combatant> grp{nam};
  game::CombatLoop loop(std::move(pu), std::move(grp), game::CombatRng((std::uint16_t)seed));
  int r = 0;
  for (; r < 200 && !loop.over(); ++r) loop.advance_round();
  std::printf("seed=0x%X rounds=%d outcome=%d party_alive=%d namtar_hp=%d\n",
              seed, r, (int)loop.outcome(), loop.party_alive(),
              loop.monsters().at(0).hp);
  return loop.outcome() == game::CombatOutcome::Victory ? 0 : 1;
}
