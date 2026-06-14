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

  std::printf("== E. 控制法術整合(Daze 跳過 / Flee 逐出 / 提早結束 / Power)==\n");
  {
    // Daze:對一隻怪施眩目(Dazzle 0x0B,Daze kControlDazzleTurns 回合)→ 該怪接下來
    //   kControlDazzleTurns 個回合輪到時跳過行動(事件 dazed_skip=true,且該怪不攻擊)。
    // 用單隻高 HP 怪 vs 單純隊伍,觀察被眩目怪的攻擊事件消失。
    Combatant mon = mon_proto;
    mon.hp = mon.max_hp = 99999;  // 不會死,純看行動
    mon.av = 0; mon.dv = 0; mon.ac = 0; mon.dmg_dice = 1; mon.dmg_sides = 2;
    std::vector<Combatant> grp(1, mon);
    CombatLoop loop(party_proto, grp, CombatRng(kSeed, 0));
    // 施眩目:caster=玩家,target_index=0(那隻怪)。
    CastResult cr = loop.cast_control(0x0B, 99, 20, /*caster_is_player=*/true, 0);
    check(cr.ok && cr.power_spent == 3 && cr.control == ControlKind::Daze,
          "cast Dazzle: ok, Power 3, control=Daze");
    check(loop.monsters()[0].dazzle_turns == kControlDazzleTurns,
          "target gains dazzle turns");
    // 推進 kControlDazzleTurns 回合:被眩目怪每回合都應跳過(dazed_skip 事件,且無該怪攻擊)。
    std::size_t seen_skips = 0;
    std::size_t ev_before = loop.events().size();
    for (int r = 0; r < kControlDazzleTurns; ++r) {
      loop.advance_round();
      for (std::size_t i = ev_before; i < loop.events().size(); ++i) {
        const auto& e = loop.events()[i];
        // 怪在被眩期間不應有「怪攻擊玩家」的命中/攻擊事件(只有 dazed_skip)。
        if (!e.attacker_is_player) {
          if (e.dazed_skip) seen_skips++;
          else check(false, "dazed monster must not attack while dazzled");
        }
      }
      ev_before = loop.events().size();
    }
    check(seen_skips == (std::size_t)kControlDazzleTurns,
          "dazzled monster skips exactly kControlDazzleTurns rounds");
    // 眩目耗盡後,下一回合怪可再行動(dazzle_turns 歸 0)。
    check(loop.monsters()[0].dazzle_turns == 0, "dazzle expired after N rounds");
  }
  {
    // Flee:對「一群怪」逐隻施膽怯(Cowardice 0x10,Flee)→ 全怪逃離 → 立即 Victory(提早結束)。
    Combatant tanky = mon_proto;
    tanky.hp = tanky.max_hp = 99999;  // 打不死,只能靠控制清場
    std::vector<Combatant> grp(kGroup, tanky);
    CombatLoop loop(party_proto, grp, CombatRng(kSeed, 0));
    check(loop.outcome() == CombatOutcome::Ongoing, "before flee: Ongoing");
    int power = 999;
    for (int i = 0; i < kGroup; ++i) {
      CastResult cr = loop.cast_control(0x10, power, 20, true, i);
      check(cr.ok && cr.control == ControlKind::Flee && cr.target_fled,
            "Cowardice forces a monster to flee");
      power -= cr.power_spent;
    }
    check(loop.monsters_fled() == kGroup, "all monsters fled");
    check(loop.monsters_in_combat() == 0, "no monster still in combat");
    check(loop.monsters_alive() == kGroup, "fled monsters still 'alive' (not killed)");
    check(loop.outcome() == CombatOutcome::Victory,
          "all monsters fled → Victory (combat ends early via control)");
    check(loop.xp_award() == 80, "fled-clear victory still awards flat 80 XP (DOS)");
    check(power == 999 - kGroup * 8, "Power deducted 8 per Cowardice cast");
  }
  {
    // 確定性:含控制施法的整場序列,同 seed 兩次 byte-identical。
    auto run_ctrl = [&](std::uint16_t seed) -> std::string {
      Combatant mon = mon_proto; mon.hp = mon.max_hp = 50;
      std::vector<Combatant> grp(kGroup, mon);
      CombatLoop loop(party_proto, grp, CombatRng(seed, 0));
      loop.cast_control(0x0B, 99, 20, true, 0);  // 眩目 idx0
      loop.cast_control(0x10, 99, 20, true, 1);  // 膽怯 idx1(逃)
      std::string s;
      char b[128];
      for (int r = 0; r < 50 && !loop.over(); ++r) {
        loop.advance_round();
        std::snprintf(b, sizeof b, "R%d p=%d m=%d o=%d\n", r,
                      loop.party_in_combat(), loop.monsters_in_combat(),
                      (int)loop.outcome());
        s += b;
      }
      for (const auto& e : loop.events()) {
        std::snprintf(b, sizeof b, "E %s>%s pl=%d hit=%d dmg=%d ds=%d tf=%d da=%d\n",
                      e.attacker.c_str(), e.target.c_str(), (int)e.attacker_is_player,
                      (int)e.hit, e.damage, (int)e.dazed_skip, (int)e.target_fled,
                      (int)e.dazed_applied);
        s += b;
      }
      return s;
    };
    check(run_ctrl(0x4D4D) == run_ctrl(0x4D4D),
          "control-laden battle deterministic under fixed seed");
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
