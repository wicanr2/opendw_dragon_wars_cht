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
#include "game/spells.hpp"  // CastResult / ControlKind(cast_control 整合控制法術)

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
  // ── 控制類事件(remake 設計)──────────────────────────────────────────
  bool dazed_skip = false;   // attacker 因被眩目/迷失/困住而本回合跳過行動(target==attacker)
  bool target_fled = false;  // 控制施法使 target 逃離戰鬥(cast_control,Cowardice/Scare)
  bool dazed_applied = false;// 控制施法使 target 進入眩目(cast_control,Dazzle 等)
  // ── 特殊攻擊事件(remake 設計 grounded 手冊;見 combat.hpp SpecialAttack)──────────
  bool special_disarm = false;  // 卸武裝命中:target 武器被打掉(傷害骰回退徒手)
  bool special_dodge = false;   // attacker 採閃避姿態(本回合提高 DV,不攻擊;target==attacker)
  // ── 召喚事件(remake 設計 grounded 手冊;見 spells.hpp make_summon)──────────────
  bool summoned = false;        // 召喚成功:attacker 召出臨時友方(target==召喚物名,加入我方)
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

  // 施放控制類法術(整合進迴圈):caster 對「對方陣營」target_index 結算控制效果。
  //   • Daze(眩目/閃光/火燄柱/圍困/龍捲風/荊棘):目標 N 回合跳過行動(combat_loop 輪到時跳過)。
  //   • Flee(膽怯/驚嚇):目標逃離戰鬥(in_combat=false),不再被瞄準、不計入存活方;可能立即定勝負。
  //   • Disarm:目標傷害降為徒手骰。Dispel(幻影現形):無戰鬥數值意義(N/A)。
  // 效果經 cast_spell 結算(grounded 手冊),並追加 CombatEvent 供 UI 戰報。
  // 回傳 CastResult(power_spent / control / handled);呼叫端負責把 power -= power_spent。
  // group/all 目標由呼叫端對每隻怪各呼叫一次。Power 消耗照舊(cast_spell)。
  CastResult cast_control(std::uint8_t spell_id, int caster_power, int caster_str,
                          bool caster_is_player, int target_index);

  // 高階施法整合(UI 用):caster(預設隊伍第 0 名)施放任意法術,依法術的 SpellTarget
  // 自動鋪到正確對象並全部結算:
  //   • 傷害/控制 → 敵方(OneEnemy=首個參戰怪;Group/AllEnemy=所有參戰怪逐隻)。
  //   • 治療/buff → 我方(OneAlly=施法者;AllAllies=全體參戰隊員)。
  //   • 工具/召喚(handled=false)→ 只扣 Power(維持誠實 TODO)。
  // 每個受影響對象套效果並追加 CombatEvent;最後 recompute_outcome(控制清場可提早結束)。
  // 回傳「對主要對象」的 CastResult(power_spent 為單次施放消耗,呼叫端據此扣 Power)。
  // 確定性:RNG 副作用順序 = 依對象 index 升序逐一 cast_spell。
  //   caster_int / magic_ranks:Zap 攻擊判定用(docs/58 門檻 12+ranks+INT−DV);預設 0 相容。
  //   power_points:variable_power 法術投入點數(docs/58「N hp/pt」),上限由 cast_spell 依
  //     2×magic_ranks 夾;固定 power 法術忽略。
  CastResult cast(std::uint8_t spell_id, int caster_power, int caster_str,
                  bool caster_is_player = true, int caster_int = 0,
                  int magic_ranks = 0, int power_points = 1);

  // ── 召喚整合(remake 設計 grounded 手冊;見 spells.hpp make_summon)──────────────
  // 隊伍施召喚法術:結算 cast_spell(扣 Power、回填 r.summon),並把召喚出的臨時友方
  //   (make_summon)加入我方陣營(party_,is_player=true、summoned=true),參與後續回合、
  //   可被擊倒。**戰鬥結束後消失**(不回寫存檔;XP 依清場扁平制只發真實隊員)。
  //   召喚物加入後重排行動順序(納入戰鬥)。追加 CombatEvent(summoned=true)供 UI 戰報。
  //   回傳 CastResult(power_spent / summon / handled);呼叫端負責 power -= power_spent。
  CastResult summon(std::uint8_t spell_id, int caster_power, int caster_str,
                    bool caster_is_player = true);

  // 召喚出的臨時友方數(UI / 測試參考)。
  int summoned_count() const;

  // ── 特殊攻擊整合(remake 設計 grounded 手冊;見 combat.hpp SpecialAttack)──────────
  // actor:採取特殊攻擊的單位(is_player=true 預設隊伍第 0 名,actor_index 指定)。
  // 一次「特殊攻擊行動」= 該 actor 對「對方陣營一個參戰目標」結算一次 resolve_special_attack
  //   並追加 CombatEvent。**不自動推進整個回合**(回合推進/反擊由呼叫端 advance_round 編排,
  //   對齊既有 cast 之後 group_round 反擊的模式)。
  //   • MightyBlow / Advance / Disarm:對 target_index 結算(<0 → 自動挑首個參戰敵人)。
  //   • Dodge:對 actor 自身套 dodge_dv(本回合提高 DV);target_index 忽略。
  //   • QuickFight:語意 = 一般攻擊(快速由 UI 跳過動畫;結算同 Normal)。
  // 回傳 AttackResult(hit/damage…);呼叫端可據此顯示戰報。
  AttackResult special_attack(SpecialAttack type, bool actor_is_player,
                              int actor_index, int target_index);

  CombatOutcome outcome() const { return outcome_; }
  bool over() const { return outcome_ != CombatOutcome::Ongoing; }
  int round_count() const { return round_; }

  // 勝利時每員應得 XP(=kXpPerVictory);非勝利回 0。
  // XP 規則(grounded DOS docs/43:「Each member gets 80 experience points for combat.」):
  //   清場(全怪死或逃)即 Victory → 固定每員 +80,**不按擊殺數計**。故「以膽怯/驚嚇把怪
  //   逐出而清場」與「全部打死」一樣給 80(原版為扁平制,逃走怪不額外加也不扣)。**有據(扁平)**。
  int xp_award() const { return outcome_ == CombatOutcome::Victory ? kXpPerVictory : 0; }

  const std::vector<Combatant>& party() const { return party_; }
  const std::vector<Combatant>& monsters() const { return monsters_; }
  const std::vector<CombatEvent>& events() const { return events_; }

  // 存活計數(UI / 群描述用)。alive 含「已逃離但未死」者(仍在 monsters_ 裡)。
  int monsters_alive() const;
  int party_alive() const;
  // 仍參戰計數(存活且未逃離)。勝負判定用此(全怪死或逃 → 勝)。
  int monsters_in_combat() const;
  int party_in_combat() const;
  // 已逃離的怪數(膽怯/驚嚇逐出);UI / XP 規則參考。
  int monsters_fled() const;

 private:
  // 行動順序:全體單位的 (side, index) 對,依 DEX→AV→index 降序一次性排定。
  struct Actor { bool is_player; int index; };
  void build_turn_order();
  // 為 actor 選一個對方陣營的存活目標 index;無存活回 -1。
  int pick_target(bool attacker_is_player);
  // 回傳 pool 中首個「仍參戰」單位 index;無則 -1(特殊攻擊自動選敵用,確定性)。
  static int pick_first_in_combat(const std::vector<Combatant>& pool);
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
