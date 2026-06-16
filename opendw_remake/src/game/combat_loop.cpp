// combat_loop — 完整戰鬥迴圈實作。真值界定見 combat_loop.hpp 檔頭。
#include "game/combat_loop.hpp"

#include <algorithm>

#include "game/spells.hpp"  // cast_control 透過 cast_spell 結算控制狀態(grounded 手冊)

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
  // 目標 = 對方陣營(攻擊者為玩家 → 怪群;反之 → 隊伍)的「仍參戰」單位,隨機選一(確定性)。
  // in_combat():存活且未逃離。逃離(膽怯/驚嚇)者視同離場,不被瞄準。
  const std::vector<Combatant>& enemies = attacker_is_player ? monsters_ : party_;
  std::vector<int> alive;
  for (int i = 0; i < static_cast<int>(enemies.size()); ++i)
    if (enemies[i].in_combat()) alive.push_back(i);
  if (alive.empty()) return -1;
  std::uint16_t pick = rng_.below(static_cast<std::uint16_t>(alive.size()));
  return alive[pick];
}

int CombatLoop::pick_first_in_combat(const std::vector<Combatant>& pool) {
  for (int i = 0; i < static_cast<int>(pool.size()); ++i)
    if (pool[i].in_combat()) return i;
  return -1;
}

std::size_t CombatLoop::advance_round() {
  if (over()) return 0;
  std::size_t before = events_.size();
  ++round_;
  for (const Actor& a : order_) {
    if (over()) break;  // 一方全滅 / 全逃即停(本回合剩餘行動不再進行)
    Combatant& attacker = a.is_player ? party_[a.index] : monsters_[a.index];
    // Dodge(閃避)臨時 DV 只保護到「該單位下次行動前」:輪到它時清除(remake 設計)。
    //   故閃避後對方的一輪攻擊享受加成,等自己再行動時失效。
    attacker.dodge_dv = 0;
    if (!attacker.in_combat()) continue;  // 行動者已倒下或已逃離 → 跳過
    if (attacker.dazzled()) {  // 被眩目/迷失/困住 → 本回合不行動,消耗一層控制(remake 設計)
      attacker.dazzle_turns--;
      CombatEvent ev;
      ev.attacker = attacker.name;
      ev.target = attacker.name;  // 自身(被控制),UI 以 dazed_skip 旗標套訊息
      ev.attacker_is_player = a.is_player;
      ev.hit = false;
      ev.dazed_skip = true;
      events_.push_back(std::move(ev));
      continue;
    }
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
  // 勝負以「仍參戰」(in_combat:存活且未逃離)判定:全怪死或逃 → 勝;全隊昏倒 → 敗。
  bool mon = monsters_in_combat() > 0;
  bool party = party_in_combat() > 0;
  if (!mon) outcome_ = CombatOutcome::Victory;        // 怪全滅或全逃 → 勝
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

int CombatLoop::monsters_in_combat() const {
  int n = 0;
  for (const auto& m : monsters_) if (m.in_combat()) ++n;
  return n;
}

int CombatLoop::party_in_combat() const {
  int n = 0;
  for (const auto& p : party_) if (p.in_combat()) ++n;
  return n;
}

int CombatLoop::monsters_fled() const {
  int n = 0;
  for (const auto& m : monsters_) if (m.fled) ++n;
  return n;
}

namespace {
// 把一次 cast_spell 結果反映成戰報事件(供 UI i18n)。caster_name 為施法者名。
CombatEvent make_cast_event(const std::string& caster_name, bool caster_is_player,
                            const Combatant& tgt, const CastResult& r) {
  CombatEvent ev;
  ev.attacker = caster_name;
  ev.target = tgt.name;
  ev.attacker_is_player = caster_is_player;
  ev.hit = (r.handled && r.amount > 0);  // 傷害/治療:amount>0 視為「命中」(套模板)
  ev.damage = r.amount;
  ev.target_died = r.target_died;
  ev.dazed_applied = (r.control == ControlKind::Daze);
  ev.target_fled = (r.control == ControlKind::Flee);
  return ev;
}
}  // namespace

AttackResult CombatLoop::special_attack(SpecialAttack type, bool actor_is_player,
                                        int actor_index, int target_index) {
  // 特殊攻擊整合(remake 設計 grounded 手冊)。actor 對對方陣營目標結算一次特殊攻擊。
  AttackResult r;
  std::vector<Combatant>& allies = actor_is_player ? party_ : monsters_;
  std::vector<Combatant>& foes = actor_is_player ? monsters_ : party_;
  if (actor_index < 0 || actor_index >= static_cast<int>(allies.size())) return r;
  Combatant& actor = allies[actor_index];
  if (!actor.in_combat()) return r;

  // Dodge:不攻擊,對自身套本回合 DV 加成。事件 special_dodge=true(target==attacker)。
  if (type == SpecialAttack::Dodge) {
    actor.dodge_dv = kDodgeDvBonus;
    CombatEvent ev;
    ev.attacker = actor.name;
    ev.target = actor.name;
    ev.attacker_is_player = actor_is_player;
    ev.hit = false;
    ev.special_dodge = true;
    events_.push_back(std::move(ev));
    return r;  // hit=false, damage=0(no-op 攻擊)
  }

  // 攻擊類:挑目標(指定 index,或自動首個參戰敵人)。
  int ti = target_index;
  if (ti < 0 || ti >= static_cast<int>(foes.size()) || !foes[ti].in_combat())
    ti = pick_first_in_combat(foes);
  if (ti < 0) { recompute_outcome(); return r; }  // 對方已全滅
  Combatant& target = foes[ti];

  r = resolve_special_attack(actor, target, type, rng_);
  CombatEvent ev;
  ev.attacker = actor.name;
  ev.target = target.name;
  ev.attacker_is_player = actor_is_player;
  ev.hit = r.hit;
  ev.damage = r.damage;
  ev.target_died = r.target_died;
  ev.stunned = r.hit && !r.target_died && r.damage >= kStunThreshold;
  ev.special_disarm = (type == SpecialAttack::Disarm && r.hit);
  events_.push_back(std::move(ev));
  recompute_outcome();
  return r;
}

CastResult CombatLoop::cast_control(std::uint8_t spell_id, int caster_power,
                                    int caster_str, bool caster_is_player,
                                    int target_index) {
  // 隊伍施控制法術:對「對方陣營」指定 index(或 group/all 由呼叫端逐隻呼叫)。
  // 透過 cast_spell 結算控制狀態(grounded 手冊),並把效果反映成事件供 UI 戰報。
  CastResult r;
  std::vector<Combatant>& foes = caster_is_player ? monsters_ : party_;
  if (target_index < 0 || target_index >= static_cast<int>(foes.size())) {
    r.note = "control: bad target index";
    return r;
  }
  Combatant& tgt = foes[target_index];
  r = cast_spell(spell_id, caster_power, caster_str, tgt, rng_);
  std::string caster = caster_is_player && !party_.empty() ? party_[0].name : "caster";
  if (r.ok && r.handled &&
      (r.control == ControlKind::Daze || r.control == ControlKind::Flee)) {
    events_.push_back(make_cast_event(caster, caster_is_player, tgt, r));
  }
  recompute_outcome();  // Flee 可能讓對方全部離場 → 立即定勝負(控制提早結束戰鬥)
  return r;
}

CastResult CombatLoop::cast(std::uint8_t spell_id, int caster_power,
                            int caster_str, bool caster_is_player) {
  CastResult primary;
  const SpellDef* sp = find_spell(spell_id);
  if (!sp) { primary.note = "unknown spell"; return primary; }
  std::vector<Combatant>& foes = caster_is_player ? monsters_ : party_;
  std::vector<Combatant>& allies = caster_is_player ? party_ : monsters_;
  std::string caster = (!allies.empty()) ? allies[0].name : "caster";

  // 決定鋪設對象集合(依 SpellTarget),確定性按 index 升序。
  bool ally_side = (sp->target == SpellTarget::OneAlly ||
                    sp->target == SpellTarget::AllAllies);
  std::vector<Combatant>& pool = ally_side ? allies : foes;

  std::vector<int> targets;
  switch (sp->target) {
    case SpellTarget::OneEnemy: {
      // 首個仍參戰的敵人(對齊 UI「對一個敵人」)。
      for (int i = 0; i < static_cast<int>(pool.size()); ++i)
        if (pool[i].in_combat()) { targets.push_back(i); break; }
      break;
    }
    case SpellTarget::GroupEnemy:
    case SpellTarget::AllEnemy: {
      for (int i = 0; i < static_cast<int>(pool.size()); ++i)
        if (pool[i].in_combat()) targets.push_back(i);
      break;
    }
    case SpellTarget::OneAlly: {
      // 施法者自己(allies[0]);若已不在場則取首個參戰隊員。
      if (!allies.empty() && allies[0].in_combat()) targets.push_back(0);
      else for (int i = 0; i < static_cast<int>(pool.size()); ++i)
             if (pool[i].in_combat()) { targets.push_back(i); break; }
      break;
    }
    case SpellTarget::AllAllies: {
      for (int i = 0; i < static_cast<int>(pool.size()); ++i)
        if (pool[i].in_combat()) targets.push_back(i);
      break;
    }
    default:  // Item / Special(工具/召喚):無戰鬥對象,對施法者自身呼叫一次(只扣 Power)。
      if (!allies.empty()) targets.push_back(0);
      break;
  }

  bool first = true;
  for (int idx : targets) {
    Combatant& t = pool[idx];
    CastResult r = cast_spell(spell_id, caster_power, caster_str, t, rng_);
    if (first) { primary = r; first = false; }
    // 套效果有意義者(傷害/治療/控制 Daze/Flee)→ 追加事件。buff/debuff/dispel/工具不發事件。
    bool emit = r.ok && r.handled &&
                (r.amount > 0 || r.control == ControlKind::Daze ||
                 r.control == ControlKind::Flee);
    if (emit) events_.push_back(make_cast_event(caster, caster_is_player, t, r));
  }
  if (first) {  // 無任何對象(理論上不會)→ 至少回報一次以利扣 Power 判斷。
    primary.note = "no target in combat";
  }
  recompute_outcome();  // 傷害致死 / 控制清場 → 可能提早定勝負
  return primary;
}

}  // namespace dw::game
