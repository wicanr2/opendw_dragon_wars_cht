// combat — 戰鬥結算核心(確定性切片)。
//
// Deep module:對外露 (1) 載入怪物、(2) 可 seed 的 RNG、(3) 單次物理攻擊結算;
// 內部隱藏怪物 blob 格式、op_4D PRNG 演算法、to-hit/傷害模型。
//
// ── 對齊狀態(務必誠實)────────────────────────────────────────────────
//  • 怪物資料:byte-for-byte 對齊 opendw monster_info.cpp 對 res31 的走訪
//    (record+0x00..0x20 屬性 + record+0x21 5-bit 名)。已知 attr[0x0B]=sprite 基底。
//  • RNG:忠實移植 opendw engine.c op_prng(@0x4132)+ update_random_seed(@0x2CF5)。
//    opendw 用 sys_ticks() 當亂源(非確定);本實作以「可 seed 的遞增 tick」取代
//    (與 remake VM vm_state.fake_ticks 同策略),故可確定性對拍。
//  • 戰鬥結算公式來源 = **fraterrisus 資料格式 + SDA 戰鬥機制 + 臺灣中文版手冊**
//    (彙整於 docs/44_DATA_FORMATS_AND_MECHANICS.md / docs/33_MANUAL_TRANSCRIPTION.md):
//      - 命中:攻擊者 AV vs 目標 DV;base AV/DV = DEX÷4;AV 含武器技能(1:1 隱形)。
//      - 傷害:解碼武器主傷害骰(高3bit 骰面 / 低3bit 骰數-1)+ STR 修正 − 目標 AC。
//      - AC 先從物理傷害扣除,再作用於 STUN(HP=Stun);STUN≤0 → status bit0(死亡)。
//    這些是 **依規格 grounded 的實作**,非 opendw byte-for-byte 移植(opendw C 碼未實作
//    戰鬥結算,只到載圖+動畫,見 check_random_encounter_timer @0x4D5C;真正 bytecode
//    結算 op 尚未逆出)。因此 **不可宣稱為 oracle 真值**。
//  • ⚠ **to-hit 骰分布為暫定**:SDA 只記「AV vs DV、疑似小骰 D&D-like」,未給確切骰式。
//    本實作採參數化小骰(見 ToHitModel),**待 docs/43_DOS_PLAYTEST.md 的 DOS 實機觀察校準**。
//    其「確定性」可驗證(固定 seed → 固定結果),但確切骰分布尚待校準。
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
// 玩家側:av/dv/ac/傷害骰 依 fraterrisus+SDA 規格(見 from_player)。
// 怪物側:21 bytes 逐欄語意未由 oracle 確認,from_monster 的對映是 **remake 暫定假設**
//         (已標 TODO),僅為驅動結算切片,非原版真值。
//
// HP=Stun(SDA):戰鬥傷害作用於 STUN;故 hp 欄載入角色 STUN 值。
struct Combatant {
  std::string name;
  bool is_player = false;
  int hp = 0;          // 當前耐打值(=STUN;SDA「HP=Stun」)
  int max_hp = 0;
  int av = 0;          // 攻擊值(命中);SDA base = DEX/4 + 武器技能 ± 武器 AV 修正
  int dv = 0;          // 防禦值(閃避);SDA base = DEX/4
  int ac = 0;          // 護甲等級;先從物理傷害扣除
  int dmg_dice = 1;    // 主武器傷害骰數(徒手回退 1d2)
  int dmg_sides = 2;   // 主武器傷害骰面
  int dmg_bonus = 0;   // STR 傷害修正
  std::uint8_t status = 0;  // 對照 player_record 0x4C bitfield(bit0 dead)

  bool alive() const { return hp > 0 && (status & 0x01) == 0; }

  // 由玩家角色投影(av/dv/ac/傷害骰 grounded in fraterrisus+SDA,見 party.hpp/.cpp)。
  static Combatant from_player(const CharacterRecord& c);
  // 由怪物投影(stat 對映為 remake 暫定,見上註解)。
  static Combatant from_monster(const MonsterRecord& m);
};

// STR → 傷害修正(SDA:「STR 愈大傷害愈大」,但未給確切曲線)。
// 暫用簡單線性:每 4 點 STR +1 傷害(類 D&D)。**待 DOS 校準**。
int str_damage_bonus(int strength);

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
//   命中:roll = 2dN(N=kToHitDie,**暫定**;bytecode 真值 = 1d16+3,門檻鏈待續驗 docs/42 §11);
//         hit ⇔ roll + attacker.av >= kToHitBase + target.dv + target.ac。AC 在命中側(不減傷)。
//   傷害:**【bytecode 反推 + 端到端驗證,徒手證據確鑿】dmg = 傷害骰 + floor(STR/5)**。
//         徒手傷害骰 = res3 0x0EC2 descriptor[min(Fist,7)](Fist=0 → 1d4);**無 ×3/2、無 floor(3)**。
//         verify_combat_script 對拍 res3 bytecode(STR10→{3,4,5,6}、STR4→[1,4])。
//         dmg 作用於 target.hp(=STUN);hp<=0 → status|=0x01(死亡)。**AC 不參與傷害**。
//         **武器路徑因 op_68(原版未逆向)仍 best-fit**。
// RNG 副作用順序固定(先擲命中,命中才擲傷害),確保可重現。
AttackResult resolve_attack(Combatant& attacker, Combatant& target, CombatRng& rng);

// to-hit 參數。
// 【bytecode 反推:res3 to-hit 子程式 @0x0F73 = 1d16+3】:r2=0x10;op_4D(→[0,16));
//   op_30 0x03(+3)→ roll ∈ [3,18];roll==3 自動命中。**門檻側比較鏈(裝甲/AV/AC)
//   尚未端到端驗證**,故 remake 命中骰式暫保留 2d10、門檻 base+dv+ac(待續逆向後對齊)。
//   骰式真值已記 docs/42 §11;此處先不動門檻邏輯以免破壞未驗證行為。
inline constexpr int kToHitDie = 10;   // 暫定 2d10(bytecode 真值為 1d16+3,門檻鏈待驗)
inline constexpr int kToHitDiceCount = 2;
inline constexpr int kToHitBase = 11;  // 命中門檻基數(roll+av >= base+dv+ac)

// 徒手傷害骰【bytecode 反推 + 端到端驗證】:descriptor table[min(Fist,7)] @res3 0x0EC2;
//   未技能(Fist=0)= descriptor 0x00 → 1d4(跑 res3 骰子程式驗 [1,4])。
inline constexpr int kUnarmedDice = 1;
inline constexpr int kUnarmedSides = 4;

// 傷害【bytecode 反推:徒手無縮放】:dmg = 傷害骰 + floor(STR/5)。**無 ×3/2、無 floor(3)**。
//   端到端跑 res3:STR10 徒手 → {3,4,5,6}(含 5)。DOS §9 的「×3/2+floor3→{3,4,6} 無 5」
//   為 53 筆小樣本近似,bytecode 證偽其「無 5」。kDmg* 設為恆等(1/1、floor1)= 不縮放。
//   (武器路徑因 op_68 原版未逆向,仍走 best-fit;但徒手已不經此縮放。)
inline constexpr int kDmgFloor = 1;
inline constexpr int kDmgMulNum = 1;
inline constexpr int kDmgMulDen = 1;

}  // namespace dw::game
