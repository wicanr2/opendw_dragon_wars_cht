// verify_progression — RPG 成長迴圈確定性 PASS/FAIL(ctest)。
//
// 全部 deterministic、無外部資產(用 chargen 序列化 + 手工 raw record 建測資)。
// 涵蓋任務五項:
//  1. 升級:XP 達門檻 → level+1、+2 AP、HP/STUN/STR 提升;連升;XP 消耗制不爆 byte。
//  2. X 配點:花 AP → 屬性/技能 +1、AP 守恆、effective_av 隨 DEX 變。
//  3. 使用物品 U:回 Power / 教法術 / 施法效果 + 消耗(充能 / 移除)。
//  4. 裝備穿脫:翻 bit[00] → main_weapon/effective_av/effective_ac 隨之變;同種互斥。
//  5. 技能檢定:等級↑ → 成功率↑;開鎖;包紮回 HP。
//  + raw 512B round-trip:成長後 Party::from_raw_records 解析回欄位一致(存檔相容)。
//
// grounded(+2AP=SDA/docs44 L103;STR/HP 隨等級=手冊33)vs remake(XP 曲線/技能難度,
// 見 progression.hpp 檔頭),測試只驗「行為自洽 + 守恆 + 確定性 + round-trip」,不對 oracle。
#include "game/chargen.hpp"
#include "game/party.hpp"
#include "game/progression.hpp"
#include "game/combat.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <vector>

using namespace dw::game;
using namespace dw::game::progression;

namespace {
int g_fail = 0;
void check(bool cond, const char* what) {
  std::printf("  [%s] %s\n", cond ? "PASS" : "FAIL", what);
  if (!cond) g_fail++;
}

// 用 chargen 造一名合法 512B record,再以欄位 patch(回傳解析好的 CharacterRecord)。
CharacterRecord make_char(const char* name, int str, int dex, int intel, int spi) {
  DraftCharacter d;
  d.name = name;
  // chargen 起始 9;用 inc 配到目標值(夾在 [9,18];測試夠用)。
  auto bump = [&](int target_attr, int target_val) {
    int cur = 9;
    while (cur < target_val && d.inc(target_attr)) ++cur;
  };
  bump(chargen::kStr, str); bump(chargen::kDex, dex);
  bump(chargen::kInt, intel); bump(chargen::kSpi, spi);
  auto raw = d.serialize();
  Party p = Party::from_raw_records({raw});
  return p.at(0);
}

// 在 record 第 slot 格寫一件物品(11B header bit-packed + 名)。
// 參數直接以 byte 設關鍵欄(equipped bit0、charges bit3-7、type@byte5 低5bit、
// magic@byte6/7、主傷骰@byte8)。對齊 equipment.cpp parse_item 位元規則。
void put_item(CharacterRecord& c, int slot, bool equipped, int charges,
              std::uint8_t type, std::uint8_t magic_hi, std::uint8_t magic_lo,
              std::uint8_t primary_dmg, int av_mod, int ac_mod,
              const char* name) {
  const int base = CharacterRecord::kInventoryBase + slot * CharacterRecord::kItemStride;
  for (int i = 0; i < CharacterRecord::kItemStride; ++i) c.raw[base + i] = 0;
  // byte0:bit0 equipped、bit3-7 charges。
  c.raw[base + 0] = static_cast<std::uint8_t>((equipped ? 0x01 : 0) | ((charges & 0x1F) << 3));
  // byte1:bit8 av 負號(這裡 av_mod 正);bit10-15 需求(略)。
  if (av_mod < 0) c.raw[base + 1] |= 0x01;  // bit8 = byte1 bit0
  // bit24-31 = byte3:bit24-27 av_mod、bit28-31 ac_mod。
  int av_abs = av_mod < 0 ? -av_mod : av_mod;
  c.raw[base + 3] = static_cast<std::uint8_t>((av_abs & 0x0F) | ((ac_mod & 0x0F) << 4));
  // bit40-47 = byte5:type 低 5 bit。
  c.raw[base + 5] = static_cast<std::uint8_t>(type & 0x1F);
  // bit48-55 = byte6 = magic_hi;bit56-63 = byte7 = magic_lo。
  c.raw[base + 6] = magic_hi;
  c.raw[base + 7] = magic_lo;
  // bit64-71 = byte8 = 主傷害骰。
  c.raw[base + 8] = primary_dmg;
  // 名(高位元終止):寫入 base+11..。
  int n = 0; while (name[n]) ++n;
  for (int i = 0; i < n && i < 12; ++i) {
    std::uint8_t b = static_cast<std::uint8_t>(name[i] & 0x7F);
    if (i + 1 < n && i + 1 < 12) b |= 0x80;
    c.raw[base + 11 + i] = b;
  }
}

// 重新解析(把 raw 改動投影回欄位)→ round-trip 解析回 CharacterRecord。
CharacterRecord reparse(const CharacterRecord& c) {
  Party p = Party::from_raw_records({c.raw});
  return p.at(0);
}
}  // namespace

int main() {
  std::printf("verify_progression: RPG 成長迴圈確定性 PASS/FAIL\n");

  // ── 1. 升級 ────────────────────────────────────────────────────────────
  {
    CharacterRecord c = make_char("Hero", 14, 16, 10, 10);  // level=1, xp=0
    check(c.level == 1 && c.xp == 0, "起始 level=1 xp=0");
    check(available_ap(c) == 0, "起始 AP=0");

    int need = xp_threshold_for_next(1);
    check(need == kXpPerLevelFactor * 1, "level1 門檻 = 80×1 = 80");

    int hp0 = c.max_health, st0 = c.max_stun, str0 = c.max_strength;

    // 未達門檻:加一點 XP < 80 → 不升級。
    c.xp = 50; c.raw[80] = 50;
    LevelUpResult r0 = check_level_up(c);
    check(!r0.leveled() && c.level == 1, "XP 50 < 80 → 不升級");

    // 達門檻:+80 XP(50+80=130 ≥ 80)→ 升 1 級,消耗 80 後 xp=50。
    c.xp = static_cast<std::uint8_t>(50 + 80); c.raw[80] = c.xp;
    LevelUpResult r1 = check_level_up(c);
    check(r1.levels_gained == 1, "XP 130 → 升 1 級");
    check(c.level == 2, "level → 2");
    check(c.xp == 50, "升級消耗門檻 80 → 剩 xp=50");
    check(r1.ap_gained == kApPerLevel && available_ap(c) == kApPerLevel, "升級 +2 AP(SDA)");
    check(c.max_strength == str0 + kStrGainPerLevel, "升級 STR +1(手冊)");
    check(c.max_health > hp0 && c.max_stun > st0, "升級 HP/STUN 提升");
    check(r1.hp_gained > 0 && r1.stun_gained > 0, "結算回報 HP/STUN 增量 >0");
    // cur 同步上限(滿血升級)。
    check(c.health == c.max_health && c.stun == c.max_stun, "升級 cur=max(滿血)");

    // round-trip:升級後 raw 解析回的 level/xp/AP/STR/HP 一致。
    CharacterRecord c2 = reparse(c);
    check(c2.level == 2 && c2.xp == 50, "round-trip level=2 xp=50");
    check(available_ap(c2) == 2, "round-trip AP=2");
    check(c2.max_strength == c.max_strength && c2.max_health == c.max_health,
          "round-trip STR/HP 一致");

    // 連升:灌大量 XP → 一次連升多級,XP 始終 ≤255(消耗制)。
    CharacterRecord c3 = make_char("Multi", 14, 16, 10, 10);
    c3.xp = 255; c3.raw[80] = 255;
    LevelUpResult rm = check_level_up(c3);
    check(rm.levels_gained >= 2, "XP 255 → 一次連升 ≥2 級");
    check(static_cast<int>(c3.xp) < xp_threshold_for_next(c3.level),
          "連升後殘餘 xp < 下一級門檻(消耗制收斂)");
    check(c3.level == 1 + rm.levels_gained, "level = 1 + 連升數");
  }

  // ── 2. X 配點(花 AP)──────────────────────────────────────────────────
  {
    CharacterRecord c = make_char("Allocator", 12, 12, 10, 10);  // DEX 12 → eff_av/dv=3
    c.raw[59] = 4;  // 給 4 AP
    check(available_ap(c) == 4, "設定 AP=4");
    check(c.effective_av() == 12 / 4, "配點前 effective_av = DEX/4 = 3");

    // 花 AP 加 DEX → AP-1、DEX+1、effective_av 隨之變。
    int dex0 = c.dexterity;
    bool ok = spend_ap_on_attr(c, kAttrDex);
    check(ok && c.dexterity == dex0 + 1, "花 AP 加 DEX → DEX+1");
    check(available_ap(c) == 3, "AP 守恆:4-1=3");

    // 再加 3 次 DEX(到 16)→ effective_av = 16/4 = 4(隨 DEX 變)。
    spend_ap_on_attr(c, kAttrDex); spend_ap_on_attr(c, kAttrDex); spend_ap_on_attr(c, kAttrDex);
    check(c.dexterity == 16, "DEX → 16");
    check(c.effective_av() == 16 / 4, "effective_av 隨 DEX 變 → 4");
    check(available_ap(c) == 0, "AP 用罄");
    check(!spend_ap_on_attr(c, kAttrStr), "AP=0 時配點失敗");

    // 配技能:給 AP,加開鎖技能 +1。
    CharacterRecord s = make_char("Skiller", 12, 12, 10, 10);
    s.raw[59] = 2;
    int lp0 = s.skills[kLockpick];
    bool sok = spend_ap_on_skill(s, kLockpick);
    check(sok && s.skills[kLockpick] == lp0 + 1, "花 AP 加開鎖技能 +1");
    check(available_ap(s) == 1, "技能配點 AP 守恆:2-1=1");
    // round-trip:技能/AP 寫回 raw。
    CharacterRecord s2 = reparse(s);
    check(s2.skills[kLockpick] == s.skills[kLockpick], "round-trip 技能一致");
    check(available_ap(s2) == 1, "round-trip AP 一致");

    // 配屬性 round-trip。
    CharacterRecord c2 = reparse(c);
    check(c2.dexterity == 16 && c2.max_dexterity == 16, "round-trip DEX cur=max=16");
  }

  // ── 3. 使用物品 U ──────────────────────────────────────────────────────
  {
    // (a) 回復法力藥水:magic_hi=0x84、magic_lo=20、charges=1 → 用後消耗。
    CharacterRecord c = make_char("Mage", 12, 12, 14, 14);
    c.power = 1; c.raw[0x1C] = 1; c.raw[0x1D] = 0;  // 當前 Power=1
    int maxp = c.max_power;
    put_item(c, 0, false, 1, 0x00, 0x84, 20, 0, 0, 0, "Power Potion");
    check(c.item_at(0).present && c.item_at(0).restores_power(), "物品0 = 回法力藥水");
    UseItemResult u = use_item(c, 0);
    check(u.ok && u.restored_power, "Use → 回復法力");
    int expect = std::min(1 + 20, maxp) - 1;
    check(u.power_restored == expect, "回復量正確(夾到 max_power)");
    check(u.consumed && !c.item_at(0).present, "充能1 → 用後移除");

    // (b) 教法術卷軸:magic_hi=0x83、magic_lo=spell id 5 → 習得 spells bit5。
    CharacterRecord t = make_char("Learner", 12, 12, 14, 14);
    put_item(t, 1, false, 1, 0x17, 0x83, 5, 0, 0, 0, "Scroll");
    check(t.item_at(1).teaches_spell(), "物品1 = 教法術卷軸");
    UseItemResult u2 = use_item(t, 1);
    check(u2.ok && u2.taught_spell && u2.taught_spell_id == 5, "Use → 習得法術 id=5");
    check((t.spells[0] & (1 << 5)) != 0, "spells bit5 已設");
    check(u2.consumed, "卷軸用後消耗");
    CharacterRecord t2 = reparse(t);
    check((t2.spells[0] & (1 << 5)) != 0, "round-trip 習得法術保留");

    // (c) 施法物品 + 多充能:magic_hi=0x10(casts spell id 0x10)、charges=3。
    CharacterRecord w = make_char("Wand", 12, 12, 14, 14);
    put_item(w, 2, false, 3, 0x00, 0x10, 0, 0, 0, 0, "Wand");
    check(w.item_at(2).casts_spell() && w.item_at(2).cast_spell_id() == 0x10, "物品2 = 施法杖 id=0x10");
    UseItemResult u3 = use_item(w, 2);
    check(u3.ok && u3.casts_spell && u3.cast_spell_id == 0x10, "Use → 回傳施法 id");
    check(!u3.consumed && w.item_at(2).present, "多充能不移除");
    check(w.item_at(2).charges == 2, "充能 3 → 2");

    // (d) 空格 / 無效果 → ok=false。
    CharacterRecord e = make_char("Empty", 12, 12, 10, 10);
    check(!use_item(e, 5).ok, "空格 Use 無效");
  }

  // ── 4. 裝備穿脫 ────────────────────────────────────────────────────────
  {
    CharacterRecord c = make_char("Fighter", 16, 16, 10, 10);  // DEX16 → base av 4
    int base_av = c.effective_av();
    check(base_av == 4, "徒手 effective_av = DEX/4 = 4");
    int base_ac = c.effective_ac();
    check(base_ac == 0, "無甲 effective_ac = 0");

    // 放一把劍(type=0x05),AV 修正 +2,未裝備。
    put_item(c, 0, false, 0, 0x05, 0, 0, 0 /*dmg*/, +2, 0, "Sword");
    check(!c.main_weapon().present || !c.item_at(0).equipped, "劍尚未裝備");
    EquipResult e1 = toggle_equip(c, 0);
    check(e1.ok && e1.now_equipped, "裝備劍 → equipped");
    check(c.item_at(0).equipped, "bit[00] 已設");
    check(c.effective_av() == base_av + 2, "裝備劍 → effective_av +2(隨裝備變)");

    // 放一面盾(type=0x01),AC 修正 +3,裝備 → effective_ac +3。
    put_item(c, 1, false, 0, 0x01, 0, 0, 0, 0, +3, "Shield");
    toggle_equip(c, 1);
    check(c.effective_ac() == base_ac + 3, "裝備盾 → effective_ac +3(隨裝備變)");

    // 互斥:放第二把武器(斧),裝備 → 自動卸下劍。
    put_item(c, 2, false, 0, 0x03, 0, 0, 0, +1, 0, "Axe");
    toggle_equip(c, 2);
    check(c.item_at(2).equipped, "斧已裝備");
    check(!c.item_at(0).equipped, "互斥:裝斧 → 劍自動卸下");
    check(c.effective_av() == base_av + 1, "現主武器=斧 → effective_av +1");
    check(c.effective_ac() == base_ac + 3, "盾仍裝備 → AC 不受武器互斥影響");

    // 脫:卸斧 → 回徒手 AV。
    EquipResult e2 = toggle_equip(c, 2);
    check(e2.ok && !e2.now_equipped, "卸下斧");
    check(c.effective_av() == base_av, "脫光武器 → effective_av 回 DEX/4");

    // round-trip:裝備狀態寫回 raw。
    CharacterRecord c2 = reparse(c);
    check(c2.item_at(1).equipped && !c2.item_at(0).equipped && !c2.item_at(2).equipped,
          "round-trip 裝備狀態:盾裝、劍/斧卸");
  }

  // ── 5. 技能檢定 / 包紮 ─────────────────────────────────────────────────
  {
    // 等級↑ → 成功率↑(同一組固定 seed 序列,統計命中數單調不減)。
    auto success_rate = [](int skill, int difficulty) {
      CombatRng rng(0xABCD, 0);
      int hits = 0, n = 200;
      for (int i = 0; i < n; ++i)
        if (skill_check(skill, difficulty, rng).success) ++hits;
      return hits;
    };
    int r_lo = success_rate(0, kLockNormal);
    int r_mid = success_rate(6, kLockNormal);
    int r_hi = success_rate(12, kLockNormal);
    check(r_mid >= r_lo && r_hi >= r_mid, "技能等級↑ → 成功率單調不減");
    check(r_hi > r_lo, "高技能成功率 > 低技能(payoff)");

    // SDA 校準:lockpick 4 對一般鎖(diff10)應「多數夠」(>50%)。
    int r4 = success_rate(4, kLockNormal);
    check(r4 > 100, "lockpick 4 對一般鎖 >50% 命中(SDA「多數夠」)");

    // 確定性:同 seed 兩次同結果。
    {
      CombatRng a(0x1111, 0), b(0x1111, 0);
      auto x = skill_check(5, 10, a);
      auto y = skill_check(5, 10, b);
      check(x.success == y.success && x.roll == y.roll, "技能檢定確定性(同 seed 同結果)");
    }

    // 開鎖:用角色 lockpick 技能。
    CharacterRecord c = make_char("Thief", 12, 16, 10, 10);
    c.skills[kLockpick] = 8; c.raw[32 + kLockpick] = 8;
    CombatRng rng(0x2222, 0);
    SkillCheckResult lr = try_lockpick(c, kLockEasy, rng);
    check(lr.total == lr.roll + 8, "try_lockpick 用角色開鎖技能 8");

    // 包紮:回 HP(夾到 max),死亡不可包紮。
    CharacterRecord healer = make_char("Medic", 16, 12, 10, 10);
    healer.skills[kBandage] = 3; healer.raw[32 + kBandage] = 3;
    CharacterRecord patient = make_char("Hurt", 12, 12, 10, 10);
    patient.health = 5; patient.raw[0x14] = 5; patient.raw[0x15] = 0;
    int before = patient.health;
    CombatRng brng(0x3333, 0);
    int healed = bandage(healer, patient, brng);
    check(healed > 0 && patient.health == before + healed, "包紮回 HP");
    check(patient.health <= patient.max_health, "回血夾到 max_health");

    // 滿血包紮:回 0(已滿)。
    CharacterRecord full = make_char("Full", 12, 12, 10, 10);
    int h2 = bandage(healer, full, brng);
    check(h2 == 0, "已滿血包紮回 0");

    // 死亡不可包紮。
    CharacterRecord dead = make_char("Dead", 12, 12, 10, 10);
    dead.health = 1; dead.raw[0x14] = 1; dead.status = 0x01; dead.raw[0x4C] = 0x01;
    check(bandage(healer, dead, brng) == 0, "死亡角色不可包紮");

    // 無包紮技能 → 無效。
    CharacterRecord nub = make_char("NoSkill", 12, 12, 10, 10);
    check(bandage(nub, patient, brng) == 0, "無包紮技能 → 回 0");

    // round-trip:包紮後 HP 寫回 raw。
    CharacterRecord p2 = reparse(patient);
    check(p2.health == patient.health, "round-trip 包紮後 HP 一致");
  }

  std::printf("%s: progression (%d checks failed)\n", g_fail ? "FAIL" : "PASS", g_fail);
  return g_fail ? 1 : 0;
}
