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
//
// ── res31 21B/33B 記錄 → 戰鬥屬性逐欄語意(2026-06-16 反組譯逆出)──────────────
//  方法:docker 編 opendw `disasm` 反組譯 res3(戰鬥腳本),逐指令逆出
//    (a) 遭遇 setup loop @res3 0x0500/0x053B/0x05C1(把 res31 記錄展開進群緩衝
//        data[0x03D6])、(b) 戰鬥屬性 setup driver @res3 0x08B6(怪物分支 0x08C7、
//        玩家分支 0x08F4)、(c) to-hit @0x0F73、傷害 @0x0D54/0x0D68、HP 結算
//        @0x0F1C/0x0B46。動態以 probe_monster_stats(headless 跑 res3 setup)佐證
//        群緩衝實際 byte。res31 記錄逐欄與 25 隻已命名怪(monsters.bin)交叉比對。
//
//  逆出確鑿(bytecode 逐指令對齊):
//    • byte[0x01] = 敏捷(DEX 類)→ **AV/DV base = byte[0x01] >> 2**。
//        res3 setup driver 怪物分支 0x08CE-0x08D7:`op_10 0x41,0x01; >>1 >>1`,
//        與玩家分支 0x08F8 `get_char_data 0x0e(DEX); >>1 >>1` 同式(= stat/4)。
//        交叉驗:Robber DEX=8→AV2、King's Guard DEX=16→AV4、Gladiator DEX=23→AV5,
//        對齊 DOS §9「無甲近戰 AV 3–6」。
//    • byte[0x00] = 力量(STR 類)→ 傷害 STR 修正 floor(byte[0x00]/5)。
//        傷害路徑 0x0D63 `get_char_data 0x0c(STR)` + `op_36 0x05(÷5)`,怪物 STR
//        = 記錄 byte[0x00](= char_data[0x0c] 同槽)。交叉驗:Humbaba STR=66(終戰
//        強怪)、robbers STR 6–14,量級合理。
//    • byte[0x0B] = sprite 資源基底(原已知,engine.c 0x4818)。
//
//  逆出受阻(誠實標示,**不臆造**):
//    • HP/STUN:res3 HP 由「遭遇群定義模板」(res31 0x4FE 區,非個別怪記錄)在
//        setup 0x05D2-0x05E0 以 `(template>>2)&0x0f`(0→1d10)算出 → **HP 是群層級
//        屬性,非個別怪 record byte**。記錄端最接近的代理 = byte[0x09],但經群模板
//        間接化,無法逐怪精確逆出。實作以 byte[0x09] 推 HP 並夾合理範圍,標「推斷」。
//    • AC/防禦:to-hit 目標防禦 = DEF_array[gs[0x84]](0x0372),setup 0x08E8 由
//        群緩衝 byte[0x28] + DV 算出;byte[0x28] 落在 res31 記錄 0x21+ 區(與 5-bit
//        名字打包重疊),且經多實例 builder(0x0646)寫入 → **無法由 21B 記錄端乾淨
//        逆出**。DOS §9:King's Guard 耐打來自命中率低(有甲),非高 HP;但記錄端無
//        乾淨 armor 旗標欄(byte[0x12] 等對絕大多數怪非零,非可靠 armor 指標)→
//        **不臆造 AC/DV 加成**,DV=AV=base,標「受阻」。
//    • 傷害骰:怪物攻擊走 op_68 裝備路徑(byte[8]=主傷害骰 descriptor,§13 已反組譯)
//        或徒手骰表;怪物記錄哪欄 = 武器 descriptor 受多實例 builder + op_68 自改碼
//        阻擋,**未逐欄逆出**。實作以 byte[0x00](STR)+ 小骰回退,標「推斷」。
//    • 精確逐怪 HP/AC/傷害的完整逆向卡在「per-character 動作指派 driver」(res3
//        @0x08b6 actor 迴圈,docs/42 §14:headless 動作狀態機不收斂),非單一 opcode
//        缺失;靜態反組譯已逆出搬運/setup 邏輯,但 actor 迴圈逐欄投影 char_data 槽
//        需該 driver 跑通才能端到端對拍。
// ──────────────────────────────────────────────────────────────────────────────
struct MonsterRecord {
  std::array<std::uint8_t, 21> attr{};  // record+0x00..+0x14(byte-for-byte;0x15..0x20 為名字打包區,未存)
  std::string name;

  // 已知欄位:sprite 資源索引基底(engine.c trigger_random_encounter @0x4818)。
  std::uint8_t sprite_base() const { return attr[0x0B]; }
  // sprite 資源編號 = (base << 1) + 0x8A。
  std::uint16_t sprite_res() const {
    return static_cast<std::uint16_t>((sprite_base() << 1) + 0x8A);
  }

  // ── 反組譯逆出的戰鬥屬性欄位 ────────────────────────────────────────────
  std::uint8_t strength() const { return attr[0x00]; }   // STR 類(傷害修正源)
  std::uint8_t agility() const { return attr[0x01]; }    // DEX 類(AV/DV 源)
  // AV/DV base = 敏捷 >> 2(= stat/4;bytecode 逆出,res3 0x08CE)。
  int avdv_base() const { return attr[0x01] >> 2; }
  // HP 推斷(群模板間接化;byte[0x09] 為記錄端代理,經 (>>2)&0x0f 後夾範圍)。
  int hp_hint() const {
    int h = (attr[0x09] >> 2) & 0x0f;
    return h > 0 ? h : 4;  // 0 表群模板 d10,以中位數 4 代;見檔頭「逆出受阻」
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
// 怪物側:**AV/DV(=byte[0x01]>>2)與 STR(=byte[0x00])為反組譯逆出(res3 setup
//         driver 0x08B6,見 MonsterRecord 檔頭)**;HP/AC/傷害骰因群模板間接化 +
//         actor 迴圈 driver 受阻(docs/42 §14)為 **推斷**,誠實標示。
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

  // ── 控制狀態(remake 設計;非 opendw byte-for-byte)──────────────────────
  // 控制類法術(眩目/膽怯/驚嚇…)效果在 opendw C 碼未實作(見 combat.hpp 檔頭),
  // 數值結算為依手冊文字 grounded 的 remake 設計。
  //   • fled:已逃離戰鬥(膽怯/驚嚇等)。視同「不再參戰」:不行動、不被選為目標、
  //          不計入存活方判定。逃走怪不計入我方擊殺 XP(見 combat_loop 勝負/XP 規則)。
  //   • dazzle_turns:剩餘「跳過行動」回合數(眩目/閃光/火燄柱/圍困/龍捲風/荊棘);
  //          combat_loop 輪到它時若 >0 則本回合不攻擊並 −−。仍可被選為目標(只是不還手)。
  bool fled = false;
  int dazzle_turns = 0;

  // 仍在場上(未死亡、未逃離)。逃離者視同離場 → 不參戰、不被瞄準、不計入存活。
  bool in_combat() const { return alive() && !fled; }
  // 本回合是否被控制(眩目)而無法行動。
  bool dazzled() const { return dazzle_turns > 0; }

  bool alive() const { return hp > 0 && (status & 0x01) == 0; }

  // 由玩家角色投影(av/dv/ac/傷害骰 grounded in fraterrisus+SDA,見 party.hpp/.cpp)。
  static Combatant from_player(const CharacterRecord& c);
  // 由怪物投影(stat 對映為 remake 暫定,見上註解)。
  static Combatant from_monster(const MonsterRecord& m);
};

// ── 終戰 Boss:Namtar(深淵之獸)─────────────────────────────────────────
// 誠實界定(務必保留標示):
//  • 原版終戰 Namtar 是 area27 tile 0x18/0x19 的 op_8A combat encounter。其怪物 id
//    由戰鬥設定腳本(res3,經 op_58 載入)在 word_3AE2 設出 = 0x03,**為戰鬥設定
//    流程的產物,非乾淨的 res31 record 索引**(probe_encounter_id 實測:area27 tile
//    0x18/0x19、龍谷 tile 0x0A/0x0B/0x10/0x12 全部 = 0x03;對照 area18 tile 0x19 的
//    直接 operand 0x2F)。故 **Namtar 沒有可逐欄對齊的 res31 21B 屬性記錄**。
//  • 因此本 Boss 的數值是 **remake 設計(攻略定性 + 平衡讓玩家可勝)**,非原版真值:
//    攻略 §5.20 + 段落 131/132/135 把 Namtar 描述為極強的最終 Boss(自由之劍受
//    Irkalla+永恆之神祝福後一擊可削 100 HP);本實作給高 HP/AV、中等傷害,使
//    「祝福過的隊伍」在確定性 seed 下可勝,但非無傷。**21B 欄位語意暫定,誠實標示。**
//  • 單次攻擊仍走 resolve_attack(命中/傷害公式 = bytecode 真值,§11/§12);本 Boss
//    只提供「一個強力 Combatant」,不重算公式。
//  • blessed=true:套用「自由之劍受祝福」效果(攻略:一擊削 100 HP)→ 傷害骰升級,
//    讓玩家在合理回合內可勝(對齊攻略「祝福過的劍才能傷得了 Namtar」設定)。
Combatant make_namtar();          // 終戰 Boss(remake 設計,屬性暫定)
Combatant make_blessed_hero(const CharacterRecord& c);  // 受祝福的自由之劍持有者

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

// to-hit 參數【bytecode 反推 + 端到端驗證:res3 to-hit 子程式 @0x0F73】。
//   roll = 1d16+3 ∈ [3,18](0x0F73 r2=0x10;op_4D→[0,16);op_30 0x03 → +3)。
//   **roll-under 系統**:HIT ⟺ roll ≤ 門檻;門檻 = kToHitBase + AV − (DV+AC)。
//   特例:roll==3 恆 HIT(0x0F7A jz);roll==18 恆 MISS(0x0F7F:cmp 0x12 jc)。
//   端到端跑 res3 驗證(掃 AV/def):門檻 = 13 + AV − def(def = DV+AC,armor 0x59 不影響)。
//   → kToHitBase = 13;kToHitDie = 16;kToHitAdd = 3。
inline constexpr int kToHitDie = 16;       // op_4D r2=0x10 → roll 基底 [0,16)
inline constexpr int kToHitAdd = 3;        // op_30 0x03 → +3,roll ∈ [3,18]
inline constexpr int kToHitBase = 13;      // 門檻 = 13 + AV − (DV+AC)(bytecode 反推)
inline constexpr int kToHitRollMin = 3;    // 恆命中骰值
inline constexpr int kToHitRollMax = 18;   // 恆失手骰值
// (保留供舊測試引用;新命中邏輯不用 dice-count)
inline constexpr int kToHitDiceCount = 1;

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
