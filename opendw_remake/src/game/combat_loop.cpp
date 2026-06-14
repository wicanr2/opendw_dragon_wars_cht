// combat_loop — 完整戰鬥迴圈實作。真值界定見 combat_loop.hpp 檔頭。
#include "game/combat_loop.hpp"

#include <algorithm>

namespace dw::game {

std::vector<Combatant> make_monster_group(const MonsterRecord& m, int count) {
  if (count <= 0) count = 1;
  std::vector<Combatant> g;
  g.reserve(static_cast<std::size_t>(count));
  for (int i = 0; i < count; ++i) g.push_back(Combatant::from_monster(m));
  return g;
}

CombatLoop::CombatLoop(std::vector<Combatant> party_members,
                       std::vector<Combatant> monster_group, CombatRng rng)
    : party_(std::move(party_members)),
      monsters_(std::move(monster_group)),
      rng_(rng) {
  build_turn_order();
  recompute_outcome();  // 開場若有一方已空(理論上不會)→ 立即定勝負
}

void CombatLoop::build_turn_order() {
  // 行動順序(remake 設計,SDA「DEX 高先攻」定性):全體單位一次性依排序鍵降序排定,
  // 整場固定。我方排序鍵 = DEX(以 av 近似;Combatant 不存 DEX,但 from_player 的 av
  // 已含 DEX/4 base,單調對應)→ 統一用 av 當主鍵,tie 用固定 index 保確定性。
  // 不耗 RNG,故不影響攻擊 RNG 序列的可重現性。
  order_.clear();
  for (int i = 0; i < static_cast<int>(party_.size()); ++i)
    order_.push_back({true, i});
  for (int i = 0; i < static_cast<int>(monsters_.size()); ++i)
    order_.push_back({false, i});
  auto key = [&](const Actor& a) -> int {
    return a.is_player ? party_[a.index].av : monsters_[a.index].av;
  };
  std::stable_sort(order_.begin(), order_.end(),
                   [&](const Actor& x, const Actor& y) {
                     int kx = key(x), ky = key(y);
                     if (kx != ky) return kx > ky;  // av 高先攻
                     // tie-break:我方優先,再以 index 升序 → 完全確定性
                     if (x.is_player != y.is_player) return x.is_player;
                     return x.index < y.index;
                   });
}

int CombatLoop::pick_target(bool attacker_is_player) {
  // 目標 = 對方陣營(攻擊者為玩家 → 怪群;反之 → 隊伍)的存活單位,隨機選一(確定性)。
  const std::vector<Combatant>& enemies = attacker_is_player ? monsters_ : party_;
  std::vector<int> alive;
  for (int i = 0; i < static_cast<int>(enemies.size()); ++i)
    if (enemies[i].alive()) alive.push_back(i);
  if (alive.empty()) return -1;
  std::uint16_t pick = rng_.below(static_cast<std::uint16_t>(alive.size()));
  return alive[pick];
}

std::size_t CombatLoop::advance_round() {
  if (over()) return 0;
  std::size_t before = events_.size();
  ++round_;
  for (const Actor& a : order_) {
    if (over()) break;  // 一方全滅即停(本回合剩餘行動不再進行)
    Combatant& attacker = a.is_player ? party_[a.index] : monsters_[a.index];
    if (!attacker.alive()) continue;  // 行動者已倒下 → 跳過
    int ti = pick_target(a.is_player);
    if (ti < 0) { recompute_outcome(); break; }  // 對方已全滅
    Combatant& target = a.is_player ? monsters_[ti] : party_[ti];
    int hp_before = target.hp;
    AttackResult ar = resolve_attack(attacker, target, rng_);  // 原版真值公式(不改)
    CombatEvent ev;
    ev.attacker = attacker.name;
    ev.target = target.name;
    ev.attacker_is_player = a.is_player;
    ev.hit = ar.hit;
    ev.damage = ar.damage;
    ev.target_died = ar.target_died;
    // 暈眩:命中、未致死、傷害達門檻(對齊 DOS「stunning him」;remake 門檻)。
    ev.stunned = ar.hit && !ar.target_died && ar.damage >= kStunThreshold &&
                 hp_before > 0;
    events_.push_back(std::move(ev));
    recompute_outcome();
  }
  recompute_outcome();
  return events_.size() - before;
}

void CombatLoop::flee() {
  if (over()) return;
  outcome_ = CombatOutcome::Fled;
}

void CombatLoop::recompute_outcome() {
  if (outcome_ == CombatOutcome::Fled) return;
  bool mon = monsters_alive() > 0;
  bool party = party_alive() > 0;
  if (!mon) outcome_ = CombatOutcome::Victory;        // 怪全滅 → 勝
  else if (!party) outcome_ = CombatOutcome::Defeat;  // 全隊昏倒 → 敗
  else outcome_ = CombatOutcome::Ongoing;
}

int CombatLoop::monsters_alive() const {
  int n = 0;
  for (const auto& m : monsters_) if (m.alive()) ++n;
  return n;
}

int CombatLoop::party_alive() const {
  int n = 0;
  for (const auto& p : party_) if (p.alive()) ++n;
  return n;
}

}  // namespace dw::game
