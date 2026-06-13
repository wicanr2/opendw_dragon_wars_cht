// combat — 戰鬥結算核心(確定性切片)。
//
// Deep module:對外露 (1) 載入怪物、(2) 可 seed 的 RNG、(3) 單次物理攻擊結算;
// 內部隱藏怪物 blob 格式、op_4D PRNG 演算法、to-hit/傷害模型。
//
// ── Oracle 對齊狀態(務必誠實)──────────────────────────────────────────
//  • 怪物資料:byte-for-byte 對齊 opendw monster_info.cpp 對 res31 的走訪
//    (record+0x00..0x20 屬性 + record+0x21 5-bit 名)。已知 attr[0x0B]=sprite 基底。
//  • RNG:忠實移植 opendw engine.c op_prng(@0x4132)+ update_random_seed(@0x2CF5)。
//    opendw 用 sys_ticks() 當亂源(非確定);本實作以「可 seed 的遞增 tick」取代
//    (與 remake VM vm_state.fake_ticks 同策略),故可確定性對拍。
//  • to-hit / 傷害 / HP 扣減 / 死亡:**opendw C 碼未實作戰鬥結算**(只到載圖+動畫,
//    見 check_random_encounter_timer @0x4D5C)。真正公式藏在原版 res-script bytecode,
//    opendw 尚未逆出。因此本模組的結算公式是 **remake 的乾淨室模型**,不是 oracle 移植;
//    其「確定性」可驗證(固定 seed → 固定結果),但「數值是否等同原版」無 oracle 可對。
//    待後續逆出 bytecode 戰鬥 primitive(op_5D/5E/61 + 算術 op)後再對齊真值。
// ──────────────────────────────────────────────────────────────────────
#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "game/party.hpp"

namespace dw::game {

// 怪物記錄:21 bytes 原始屬性 + 解碼後名字。
struct MonsterRecord {
  std::array<std::uint8_t, 21> attr{};  // record+0x00..+0x20(byte-for-byte)
  std::string name;

  // 已知欄位:sprite 資源索引基底(engine.c trigger_random_encounter @0x4818)。
  std::uint8_t sprite_base() const { return attr[0x0B]; }
  // sprite 資源編號 = (base << 1) + 0x8A。
  std::uint16_t sprite_res() const {
    return static_cast<std::uint16_t>((sprite_base() << 1) + 0x8A);
  }
};

// 怪物表:從 bundle/monsters/monsters.bin 載入(自包含,執行期不需 DATA1)。
class MonsterTable {
 public:
  // bundle_dir 為 assets/bundle;讀 monsters/monsters.bin。失敗回 std::nullopt。
  static std::vector<MonsterRecord> load(const std::filesystem::path& bundle_dir);
  // 直接從 blob bytes 解析(供測試)。失敗回空。
  static std::vector<MonsterRecord> parse(const std::vector<std::uint8_t>& blob);
};

// 確定性 RNG:忠實移植 opendw op_prng + update_random_seed。
//   update: ax = next_tick(); ax += seed; seed = ax。
//   prng:  mul = ax * r2; 回傳 (mul >> 16)。byte 模式取低 byte,word 模式取整 word。
// opendw 的 next_tick = sys_ticks()(非確定);此處以可 seed 的遞增 tick 取代。
class CombatRng {
 public:
  // seed:對照 opendw random_seed(預設 0x1234)。tick_seed:fake_ticks 起點。
  explicit CombatRng(std::uint16_t seed = 0x1234, std::uint16_t tick_seed = 0)
      : seed_(seed), tick_(tick_seed) {}

  // 對照 op_prng(word 模式):回傳 (ax * r2) 的高 16 bit。
  // r2 為呼叫端帶入的「範圍/乘數暫存」(對照 word_3AE2)。
  std::uint16_t next_word(std::uint16_t r2) {
    update_seed();
    std::uint32_t mul = static_cast<std::uint32_t>(seed_) * r2;
    return static_cast<std::uint16_t>((mul & 0xFFFF0000u) >> 16);
  }
  // 對照 op_prng(byte 模式):回傳 (ax * r2) 的 bits 16..23。
  std::uint8_t next_byte(std::uint16_t r2) {
    update_seed();
    std::uint32_t mul = static_cast<std::uint32_t>(seed_) * r2;
    return static_cast<std::uint8_t>((mul & 0x00FF0000u) >> 16);
  }
  // 便利:回傳 [0, n) 的均勻值。n==0 回 0。
  //
  // 注意:這裡**不**直接用 op_prng 的 (seed*n)>>16,因為原版 bytecode 會先把一個
  // 大乘數載入 word_3AE2 再呼叫 prng;在小 n(如 d20)且 tick 增量極小時,(seed*n)>>16
  // 幾乎恆為 0/定值(分佈退化)。原版真實縮放藏在尚未逆出的戰鬥 script(見檔頭)。
  // 為讓切片產生有意義且可重現的分佈,below() 改為:推進一次種子(維持與 op_prng
  // 相同的 update_seed 副作用順序),再對「種子全 16 bit」取模。仍 100% 確定性、可 seed。
  std::uint16_t below(std::uint16_t n) {
    if (n == 0) return 0;
    update_seed();
    return static_cast<std::uint16_t>(seed_ % n);
  }
  // 便利:擲 count 顆 sides 面骰之和(+0 base)。sides<=1 視為固定值。
  int roll(int count, int sides) {
    int sum = 0;
    for (int i = 0; i < count; ++i) sum += 1 + static_cast<int>(below(static_cast<std::uint16_t>(sides)));
    return sum;
  }

  std::uint16_t seed() const { return seed_; }
  std::uint16_t tick() const { return tick_; }

 private:
  void update_seed() {
    std::uint16_t ax = ++tick_;  // sys_ticks() 的可重現替身(對照 vm_state.fake_ticks)
    ax = static_cast<std::uint16_t>(ax + seed_);
    seed_ = ax;
  }
  std::uint16_t seed_;
  std::uint16_t tick_;
};

// ── 戰鬥單位 ───────────────────────────────────────────────────────────
// 把 CharacterRecord / MonsterRecord 投影成統一的戰鬥屬性視圖。
// 注意:怪物 21 bytes 的逐欄語意未由 oracle 確認,以下 from_monster 的對映是
//       **remake 暫定假設**(已標 TODO),僅為驅動確定性結算切片,非原版真值。
struct Combatant {
  std::string name;
  bool is_player = false;
  int hp = 0;          // 當前生命
  int max_hp = 0;
  int attack = 0;      // 命中加值(技能/等級綜合)
  int defense = 0;     // AC / 閃避(被命中難度)
  int dmg_dice = 1;    // 傷害骰數
  int dmg_sides = 4;   // 傷害骰面
  int dmg_bonus = 0;   // 傷害固定加值
  std::uint8_t status = 0;  // 對照 player_record 0x4C bitfield

  bool alive() const { return hp > 0 && (status & 0x01) == 0; }

  // 由玩家角色投影(欄位語意已由 oracle 確認,見 party.hpp)。
  static Combatant from_player(const CharacterRecord& c);
  // 由怪物投影(stat 對映為 remake 暫定,見上註解)。
  static Combatant from_monster(const MonsterRecord& m);
};

// 單次攻擊結算結果(供逐回合對拍 / log)。
struct AttackResult {
  bool hit = false;
  int to_hit_roll = 0;    // 命中骰值
  int to_hit_need = 0;    // 命中門檻
  int damage = 0;         // 實際造成傷害
  int target_hp_after = 0;
  bool target_died = false;
};

// 解算 attacker → target 的一次物理攻擊(會改 target.hp / status)。
//   命中模型(remake 乾淨室,可確定性對拍):
//     to_hit_roll = 1 + rng.below(20)            // d20
//     hit  ⇔  to_hit_roll + attacker.attack >= 10 + target.defense
//     damage = roll(dmg_dice, dmg_sides) + dmg_bonus  (命中才擲)
//     target.hp -= damage;  hp<=0 → status |= 0x01 (dead)
// RNG 副作用順序固定(先擲命中,命中才擲傷害),確保可重現。
AttackResult resolve_attack(Combatant& attacker, Combatant& target, CombatRng& rng);

}  // namespace dw::game
