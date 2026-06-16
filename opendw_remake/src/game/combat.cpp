// combat — 結算核心實作。對齊狀態見 combat.hpp 檔頭。
#include "game/combat.hpp"

#include <cstdio>

namespace dw::game {

namespace {
std::uint16_t rd_u16(const std::uint8_t* p) {
  return static_cast<std::uint16_t>(p[0] | (p[1] << 8));
}
}  // namespace

std::vector<MonsterRecord> MonsterTable::parse(
    const std::vector<std::uint8_t>& blob) {
  std::vector<MonsterRecord> out;
  // header: magic[6] "DWMON\0", version u16, count u16
  if (blob.size() < 10) return out;
  if (!(blob[0] == 'D' && blob[1] == 'W' && blob[2] == 'M' && blob[3] == 'O' &&
        blob[4] == 'N' && blob[5] == 0)) {
    return out;
  }
  std::uint16_t version = rd_u16(&blob[6]);
  if (version != 1) return out;
  std::uint16_t count = rd_u16(&blob[8]);
  std::size_t off = 10;
  for (std::uint16_t i = 0; i < count; ++i) {
    if (off + 21 + 1 > blob.size()) break;
    MonsterRecord m;
    for (int k = 0; k < 21; ++k) m.attr[k] = blob[off + k];
    off += 21;
    std::uint8_t nlen = blob[off++];
    if (off + nlen > blob.size()) break;
    m.name.assign(reinterpret_cast<const char*>(&blob[off]), nlen);
    off += nlen;
    out.push_back(std::move(m));
  }
  return out;
}

std::vector<MonsterRecord> MonsterTable::load(
    const std::filesystem::path& bundle_dir) {
  const auto path = bundle_dir / "monsters" / "monsters.bin";
  std::FILE* f = std::fopen(path.string().c_str(), "rb");
  if (!f) return {};
  std::fseek(f, 0, SEEK_END);
  long len = std::ftell(f);
  std::fseek(f, 0, SEEK_SET);
  std::vector<std::uint8_t> blob(len > 0 ? static_cast<std::size_t>(len) : 0);
  if (!blob.empty()) {
    if (std::fread(blob.data(), 1, blob.size(), f) != blob.size()) {
      std::fclose(f);
      return {};
    }
  }
  std::fclose(f);
  return parse(blob);
}

int str_damage_bonus(int strength) {
  // 【bytecode 反推 + DOS 對拍,證據確鑿】res3 徒手傷害路徑(0x0D63→0x0D7F):
  //   char_data[0x0c](STR)→ op_36 0x05(÷5)→ 加到傷害骰和(0x0DAD op_30)。
  //   即 STR 修正 = floor(STR/5)(**加法**,非 ×3/2)。
  //   端到端跑 res3 bytecode 驗證:STR=10→+2、STR=20→+4、STR=25→+5,與每點分布一致。
  //   (取代先前 DOS best-fit 的 Str/16;那是 53 筆小樣本的近似。)
  return strength / 5;
}

Combatant Combatant::from_player(const CharacterRecord& c) {
  Combatant u;
  u.name = c.name;
  u.is_player = true;
  // HP=Stun(SDA):戰鬥傷害作用於 STUN → 載入 STUN 為耐打值。
  u.hp = c.stun;
  u.max_hp = c.max_stun;
  // AV/DV/AC:依 fraterrisus 欄位 + SDA 公式(stored 為 0 時回退 DEX/4;見 party.cpp)。
  u.av = c.effective_av();
  u.dv = c.effective_dv();
  u.ac = c.effective_ac();
  // 傷害骰:解碼主武器主傷害骰;無武器 → 徒手回退。
  // 【bytecode 反推 + 端到端驗證,證據確鑿】res3 武器傷害路徑(0x0D68)以
  //   op_68 0x08 讀「裝備記錄 byte[8]= 主傷害骰 descriptor」(op_68 已自原始 DRAGON.COM
  //   反組譯 @0x450A,opendw targets[] 原標 NULL)。descriptor 解碼與徒手共用骰子子程式
  //   (0x06EC):sides = {4,6,8,10,12,20,30,100}[d>>5]、count = (d & 0x1f)+1。
  //   verify_combat_script 端到端驗:descriptor 0x00/0x21/0x05/0xA3 → 純骰 [1,4]/[2,12]/[6,24]/[4,80]。
  //   (此處 primary_dmg 用 fraterrisus bit[64-71] 解,與 bytecode descriptor 同編碼,已對齊。)
  EquipItem w = c.main_weapon();
  if (w.present && w.primary_dmg.valid()) {
    u.dmg_dice = w.primary_dmg.count;
    u.dmg_sides = w.primary_dmg.sides;
  } else {
    u.dmg_dice = kUnarmedDice;
    u.dmg_sides = kUnarmedSides;
  }
  u.dmg_bonus = str_damage_bonus(c.strength);  // STR 傷害修正
  u.status = c.status;
  return u;
}

Combatant Combatant::from_monster(const MonsterRecord& m) {
  Combatant u;
  u.name = m.name;
  u.is_player = false;
  // ── res31 怪物記錄 → 戰鬥屬性(2026-06-16 反組譯逆出 + 推斷,見 combat.hpp 檔頭)──
  //
  // 【反組譯逆出,bytecode 逐指令對齊】res3 戰鬥屬性 setup driver @0x08B6 怪物分支:
  //   • AV/DV base = byte[0x01] >> 2(0x08CE `op_10 0x41,0x01; >>1 >>1`;= 玩家 DEX/4)。
  //   • STR(傷害修正源)= byte[0x00](傷害路徑 0x0D63 char_data[0x0c]=STR,怪物同槽)。
  //   交叉驗:Robber DEX8→AV2、King's Guard DEX16→AV4、Humbaba STR66(終戰強怪)。
  //
  // 【推斷,逆出受阻(誠實標示)】HP/AC/傷害骰:
  //   • HP 是「遭遇群模板」屬性(res31 0x4FE 區),經 (template>>2)&0x0f 算出,非個別
  //     怪 record byte;記錄端代理 = byte[0x09](hp_hint),夾合理範圍。
  //   • AC/防禦:to-hit 目標防禦 = DEF_array(0x0372)= 群緩衝 byte[0x28] + DV,落在
  //     res31 記錄名字打包區 + 多實例 builder 寫入,21B 記錄端無法乾淨逆出。DOS §9:
  //     有甲怪(King's Guard)耐打來自命中率低(高 DV/AC),非高 HP;但記錄端無乾淨
  //     armor 旗標欄 → 不臆造 AC/DV 加成,DV=AV=base(見下)。
  //   • 傷害骰:怪物攻擊走 op_68 裝備路徑或徒手骰表,記錄哪欄=武器 descriptor 受
  //     多實例 builder + op_68 自改碼阻擋,未逐欄逆出 → 以 STR 推小骰。
  //   完整逐怪精確逆出卡在 actor 迴圈動作指派 driver(docs/42 §14),非單一 opcode 缺。
  //
  // blob 21B 格式不動;此函式只改「怪物 record → Combatant」投影。

  // AV/DV:bytecode 逆出 = 敏捷 >> 2(與玩家 DEX/4 同式)。
  int avdv = m.avdv_base();                    // = byte[0x01] >> 2

  // HP/STUN:推斷(群模板間接化;byte[0x09] 代理)。
  u.max_hp = m.hp_hint();
  u.hp = u.max_hp;

  // AV / DV:bytecode 逆出 = 敏捷 >> 2(setup driver 怪物分支 0x08CE,同玩家 DEX/4)。
  //   AV bonus(byte[0x27])與 AC/防禦(byte[0x28] → DEF_array)落在 res31 記錄名字
  //   打包區 + 多實例 builder,**無法由 21B 記錄端乾淨逆出**(見 combat.hpp 檔頭)。
  //   故 AV=DV=base;不臆造 armor/AC 加成(誠實標示)。有甲怪的耐打(DOS §9:命中率
  //   低而非高 HP)在原版來自 DEF_array;remake 此處無乾淨來源,維持 base,標「受阻」。
  u.av = avdv;
  u.dv = avdv;
  // AC:不另減傷(bytecode 真值:AC 在命中側,見 resolve_attack)。怪物 AC 來源受阻。
  u.ac = 0;

  // 傷害骰:推斷。怪物 STR(byte[0x00])決定傷害量級,以小骰 + STR/5 回退。
  //   強怪(STR 高)傷害較大;弱怪 1d4 左右。徒手骰表回退(無法逐欄取武器 descriptor)。
  int str = m.strength();
  u.dmg_dice = 1;
  u.dmg_sides = str >= 40 ? 8 : (str >= 16 ? 6 : 4);  // STR 量級 → 骰面(推斷)
  u.dmg_bonus = str_damage_bonus(str);                // floor(STR/5)(逆出係數)
  u.status = 0;
  return u;
}

Combatant make_namtar() {
  // 終戰 Boss(remake 設計;屬性暫定,誠實標示見 combat.hpp 檔頭)。
  // 設計目標:強到「需祝福過的隊伍/劍」才能在合理回合內勝,但確定性 seed 下可勝。
  //  • HP(STUN):攻略以「自由之劍一擊削 100 HP」描述其耐打 → 設 150(數回合祝福攻擊可解決)。
  //  • AV/DV:高(精銳 Boss),但低於「祝福過的劍 + 全屬性 +3」隊伍的可命中範圍。
  //  • AC:0(命中側已含 DV;不另減傷,對齊 bytecode「AC 在命中側不減傷」)。
  //  • 傷害:中高(會傷人但不秒殺滿血隊員)。
  Combatant u;
  u.name = "Namtar";            // i18n 鍵(combat.tsv:納達)
  u.is_player = false;
  // 平衡(remake 設計):起始隊伍 STUN 極低(12–16),受祝福勇者(4d12+24)約 3–4 擊可破。
  //   故 Namtar HP 設 120(可在合理回合內被祝福劍解決),傷害壓到 1d6+1(2–7,不秒殺滿血
  //   隊員、讓祝福勇者撐得到打完),av 5(命中但非必中)。確定性 seed 下「受祝福隊伍」可勝。
  u.hp = u.max_hp = 120;        // remake 設計(攻略定性「極耐打」;平衡至可勝)
  u.av = 5;                     // 命中(roll-under:門檻 = 13+AV−def)
  u.dv = 6;                     // 高閃避
  u.ac = 0;                     // 不減傷(AC 在命中側,bytecode 真值)
  u.dmg_dice = 1;               // 1d6+1:會傷人但不秒殺滿血隊員
  u.dmg_sides = 6;
  u.dmg_bonus = 1;
  u.status = 0;
  return u;
}

Combatant make_blessed_hero(const CharacterRecord& c) {
  // 「受祝福的自由之劍持有者」(remake 設計;攻略:Irkalla 0x80 + 永恆之神 0x10 祝福 →
  //  自由之劍一擊削 100 HP;永恆之神另 +3 全屬性)。為讓「打贏 Namtar」可達成,對該角色
  //  套用攻略定性的祝福:命中/傷害提升 + STR/DEX +3(反映在 av/dv/傷害骰)。
  //  **非原版逐欄真值;祝福旗標 flags[85]=char_data 0x55 的 op_5F/op_61 已實作,但「祝福
  //   → 具體戰鬥加成數值」原版未逆出 → 此處為 remake 平衡設計,誠實標示。**
  Combatant u = Combatant::from_player(c);
  u.av += 3;                    // 永恆之神 +3(對齊攻略「全屬性 +3」→ AV 連帶提升)
  u.dv += 3;
  // 自由之劍(受祝福):一擊削 100 HP(攻略)→ 以強力傷害骰反映(remake 設計)。
  u.dmg_dice = 4; u.dmg_sides = 12; u.dmg_bonus += 20;  // ~24..68/擊(數擊可破 Namtar 150 HP)
  return u;
}

AttackResult resolve_attack(Combatant& attacker, Combatant& target,
                            CombatRng& rng) {
  AttackResult r;
  // 命中【bytecode 反推 + 端到端驗證:res3 0x0F73】:**roll-under** 系統。
  //   roll = 1d16+3 ∈ [3,18];HIT ⟺ roll ≤ 門檻,門檻 = kToHitBase + AV − (DV+AC)。
  //   特例:roll==3 恆 HIT(0x0F7A)、roll==18 恆 MISS(0x0F7F)。AC 在命中側(DOS §9)。
  //   端到端跑 res3 驗證:門檻 = 13 + AV − def(def=DV+AC;armor char_data[0x59] 不影響)。
  //   RNG 副作用順序固定:先擲命中,命中才擲傷害 → 可重現。
  r.to_hit_roll = static_cast<int>(rng.below(kToHitDie)) + kToHitAdd;   // 1d16+3
  r.to_hit_need = kToHitBase + attacker.av - (target.dv + target.ac);   // 命中門檻
  if (r.to_hit_roll == kToHitRollMin) {
    r.hit = true;             // roll==3 恆命中
  } else if (r.to_hit_roll == kToHitRollMax) {
    r.hit = false;            // roll==18 恆失手
  } else {
    r.hit = (r.to_hit_roll <= r.to_hit_need);  // roll ≤ 門檻 → 命中
  }
  if (r.hit) {
    // 傷害【bytecode 反推 + DOS 對拍,徒手證據確鑿】:
    //   res3 0x06EC 骰子子程式擲「傷害骰」(徒手 = descriptor[min(Fist,7)],未技能 = 1d4),
    //   再 + floor(STR/5)(0x0DAD op_30 加 word_3ADF[0x0dae]=STR/5)。
    //   **無 ×3/2、無 floor(3)** —— 端到端跑 res3 bytecode 驗證:
    //   STR10 徒手 → {3,4,5,6}(含 5;DOS §9 的「無 5」是 53 筆小樣本雜訊)。
    //   AC 不參與傷害(已在命中側)。
    // 【武器傷害 STR bonus 的誠實界定(op_68 已反組譯後更新)】:
    //   op_68(@0x450A)已反組譯 → 武器主傷害骰來源(byte[8])與骰式解碼已 bytecode 確認。
    //   但「武器傷害是否 +floor(STR/5)」**bytecode 證據矛盾**:res3 武器路徑在
    //   byte[2]&0x1f==0(常規武器)時,0x0D73 jz **跳過 op_36(÷STR)** → 隔離執行下武器路徑
    //   **不加 STR bonus**(端到端驗:1d4 STR20 → [1,4] 純骰)。然 0x0DAD 加的是自我修改位址
    //   0x0DAE 的值,完整一場戰鬥中該值可能由前序攻擊者的 op_36 殘留 → **隔離分析無法判定**
    //   真機是否殘留 STR/5(self-modifying code 不確定性)。另:byte[2]&0x1f!=0 時傷害 =
    //   定值(byte[2]&0x1f),覆寫骰擲(端到端驗,= 遠程/特殊武器定傷)。
    //   【決策】DOS 實機(docs/43:Str14→3~4、Str21→6)+ SDA 均顯示武器傷害隨 STR 增,
    //   故 **保留 +floor(STR/5) 為 best-fit**(與徒手同係數),不依隔離 bytecode 逕刪 STR bonus
    //   (無完整戰鬥 oracle 可確認殘留與否,刪除恐 regress DOS 校準)。詳見 docs/42 §13。
    r.damage = rng.roll(attacker.dmg_dice, attacker.dmg_sides) + attacker.dmg_bonus;
    if (r.damage < 1) r.damage = 1;  // 骰至少 1(無人為下限 3)
    // 作用於 STUN(HP=Stun);STUN≤0 → 死亡。
    target.hp -= r.damage;
    if (target.hp <= 0) {
      target.hp = 0;
      target.status |= 0x01;  // dead(對照 player_record 0x4C bit0)
      r.target_died = true;
    }
  }
  r.target_hp_after = target.hp;
  return r;
}

}  // namespace dw::game
