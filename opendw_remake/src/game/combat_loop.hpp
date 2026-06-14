// combat_loop — 完整戰鬥迴圈(4 人隊伍 vs 怪物群、多回合、勝負、XP)。
//
// Deep module:對外只露 (1) 建立一場戰鬥(隊伍 + 怪群)、(2) 推進一回合、
// (3) 結構化事件流(供 i18n 戰報)、(4) 勝負/XP 查詢。
// 內部隱藏行動順序、目標選擇、存活掃描、回合結算等。
//
// ── 真值來源界定(務必誠實)──────────────────────────────────────────
//  • 單次物理攻擊一律走 game::resolve_attack(combat.hpp,bytecode 反推 + 端到端
//    驗證的命中/傷害公式),本模組**不重算公式**,只負責編排「誰打誰、何時打」。
//  • XP:清怪每員 +80(DOS 實機 docs/43:「Each member gets 80 experience
//    points for combat.」)→ kXpPerVictory = 80。**有據**。
//  • 行動順序 / 目標選擇 / 逃跑:**remake 設計**(SDA 定性「DEX 高先攻、隨機選
//    存活目標」,但無 oracle 確切實作)。以確定性規則實作,標明非原版真值:
//      - 行動順序:全體(我方 + 怪群)依 (DEX→AV→固定 index)降序一次性排定,
//        整場固定(SDA「DEX 高先攻」定性);DEX 怪物側無 oracle 欄位 → 用 av。
//      - 目標選擇:行動者隨機選**對方陣營一個存活單位**(rng.below,確定性)。
//      - 逃跑:本模組不處理(由 UI 層決定離場);提供 flee() 標記結束。
//  • 確定性:固定 seed + 固定隊伍 + 固定怪群 → 逐回合事件/HP/XP 完全可重現
//    (RNG 為 CombatRng,副作用順序固定:行動順序排定不耗 RNG;每次攻擊先選目標
//    再 resolve_attack)。
// ──────────────────────────────────────────────────────────────────────
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "game/combat.hpp"

namespace dw::game {

// 清怪後每名隊員獲得的 XP(DOS 實機 docs/43,有據)。
inline constexpr int kXpPerVictory = 80;

// 一次攻擊行動的結構化事件(供 UI 層套 i18n 模板成戰報文字)。
struct CombatEvent {
  std::string attacker;   // 攻擊者名(英文鍵;UI 層 tr 在地化)
  std::string target;     // 目標名(英文鍵)
  bool attacker_is_player = false;
  bool hit = false;
  int damage = 0;
  bool stunned = false;   // 本次攻擊造成暈眩(大傷害;DOS「stunning him」)— 見 kStunThreshold
  bool target_died = false;  // 目標本次被擊倒(STUN≤0)
};

// 戰鬥結果。
enum class CombatOutcome {
  Ongoing,   // 進行中
  Victory,   // 怪群全滅
  Defeat,    // 全隊昏倒
  Fled,      // 逃跑離場(UI 層觸發)
};

// 一場戰鬥(隊伍 vs 怪群)。確定性:同 seed + 同輸入 → 同事件流。
class CombatLoop {
 public:
  // party_members:我方戰鬥單位(由 Combatant::from_player 投影);
  // monster_group:怪群(由 Combatant::from_monster 投影,可重複同種);
  // rng:確定性 RNG(seed 由呼叫端帶入)。
  CombatLoop(std::vector<Combatant> party_members,
             std::vector<Combatant> monster_group, CombatRng rng);

  // 推進一個完整回合:依固定行動順序,每個存活單位攻擊對方陣營一個隨機存活目標。
  // 事件追加到 events()(不清空,累積整場戰報)。回合結束後更新 outcome()。
  // 若已非 Ongoing 則為 no-op。回傳本回合新增的事件數。
  std::size_t advance_round();

  // 標記逃跑離場(UI 層 R 鍵);outcome → Fled。不發 XP。
  void flee();

  CombatOutcome outcome() const { return outcome_; }
  bool over() const { return outcome_ != CombatOutcome::Ongoing; }
  int round_count() const { return round_; }

  // 勝利時每員應得 XP(=kXpPerVictory);非勝利回 0。
  int xp_award() const { return outcome_ == CombatOutcome::Victory ? kXpPerVictory : 0; }

  const std::vector<Combatant>& party() const { return party_; }
  const std::vector<Combatant>& monsters() const { return monsters_; }
  const std::vector<CombatEvent>& events() const { return events_; }

  // 存活計數(UI / 群描述用)。
  int monsters_alive() const;
  int party_alive() const;

 private:
  // 行動順序:全體單位的 (side, index) 對,依 DEX→AV→index 降序一次性排定。
  struct Actor { bool is_player; int index; };
  void build_turn_order();
  // 為 actor 選一個對方陣營的存活目標 index;無存活回 -1。
  int pick_target(bool attacker_is_player);
  void recompute_outcome();

  std::vector<Combatant> party_;
  std::vector<Combatant> monsters_;
  CombatRng rng_;
  std::vector<Actor> order_;
  std::vector<CombatEvent> events_;
  int round_ = 0;
  CombatOutcome outcome_ = CombatOutcome::Ongoing;
};

// 暈眩門檻(remake 設計):單次傷害 ≥ 此值時事件標 stunned(對齊 DOS「stunning him」)。
// DOS 觀察:7 點致暈、4 點不致暈(docs/43 §16/17)→ 取 ≥7 為門檻。**非確切公式**。
inline constexpr int kStunThreshold = 7;

// 依 seed 生成一組怪群:同種怪 count 隻(沿用怪物表記錄)。
// count<=0 視為 1。純資料展開,不耗 RNG(確定性)。
std::vector<Combatant> make_monster_group(const MonsterRecord& m, int count);

}  // namespace dw::game
