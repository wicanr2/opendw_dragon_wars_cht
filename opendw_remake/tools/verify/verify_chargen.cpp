// verify_chargen — 建角流程確定性 PASS/FAIL(ctest)。
//
// 對拍(全部 deterministic,無外部資產):
//  A. 點數守恆:配點前後 spent()+leftover()==kPointPool 恆成立;inc/dec 守恆。
//  B. 屬性範圍:配點後每項 ∈ [kAttrMin, kAttrMax];封頂/不足點時 inc 失敗;
//     退點不可低於內定起始值。
//  C. round-trip:DraftCharacter::serialize() → Party::from_raw_records →
//     CharacterRecord 解析回的 名/STR/DEX/INT/SPI/性別/HP/STUN/PWR/level 與草稿一致。
//  D. SDA:解析回的 record effective_av()==effective_dv()==DEX/4。
//  E. 名字編碼:高位元終止格式(除末字元外高位元設 1,末字元高位元清除)。
//  F. 確定性:同 DraftCharacter 連續兩次 serialize() → byte-identical。
//  G. 組隊:多名草稿 → Party 人數正確、順序一致。
//
// 用法:verify_chargen;退出碼 0=PASS。
#include "game/chargen.hpp"
#include "game/party.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

using namespace dw::game;
using namespace dw::game::chargen;

namespace {
int g_fail = 0;
void check(bool cond, const char* what) {
  std::printf("  [%s] %s\n", cond ? "PASS" : "FAIL", what);
  if (!cond) g_fail++;
}

// rd16 little-endian(對拍 party.cpp)。
int rd16(const std::array<std::uint8_t, 512>& r, int off) {
  return r[off] | (r[off + 1] << 8);
}
}  // namespace

int main() {
  std::printf("verify_chargen: 建角流程確定性 PASS/FAIL\n");

  // ── A. 點數守恆 ─────────────────────────────────────────────────
  {
    DraftCharacter d;
    check(d.spent() == 0, "起始配置 spent()==0");
    check(d.leftover() == kPointPool, "起始 leftover()==50(全內定起始值)");
    check(d.spent() + d.leftover() == kPointPool, "守恆:spent+leftover==pool(起始)");

    int before = d.leftover();
    bool ok = d.inc(kStr);
    check(ok && d.leftover() == before - kCostPerPlus, "inc(STR) 扣 1 點");
    check(d.spent() + d.leftover() == kPointPool, "守恆:inc 後");
    d.dec(kStr);
    check(d.leftover() == before, "dec(STR) 退回 1 點");
    check(d.spent() + d.leftover() == kPointPool, "守恆:dec 後");
  }

  // ── B. 屬性範圍 / 封頂 / 點數不足 / 退點下限 ─────────────────────
  {
    DraftCharacter d;
    // 一路加 STR 到封頂(kAttrMax),確認不超界、點數足夠時才加。
    int adds = 0;
    while (d.inc(kStr)) ++adds;
    check(d.attr[kStr] <= kAttrMax, "inc 不超過 kAttrMax");
    // 加到要嘛封頂、要嘛點數耗盡。kAttrStart=9, kAttrMax=18 → 最多 +9 = 9 點(<50)→ 封頂。
    check(d.attr[kStr] == kAttrMax, "STR 加到封頂 18");
    check(adds == kAttrMax - kAttrStart, "加的次數 == 上限-起始");
    check(!d.inc(kStr), "封頂後 inc 失敗");

    // 退點:可退回到內定起始值,但不可更低。
    while (d.dec(kStr)) {}
    check(d.attr[kStr] == kAttrStart, "退點停在內定起始值(不可更低)");
    check(!d.dec(kStr), "起始值處 dec 失敗");
  }

  // ── 全屬性封頂:4 項各 +9 = 36 點(< 池 50),守恆且全封頂後 inc 全失敗 ──
  //   [手冊] 池 50 是內定值之上的預算;4 屬性封頂只吸 36 → leftover 必 >0(設計如此:
  //   建議保留點數供升級)。驗:守恆、封頂後 inc 全失敗、spent 為 36。
  {
    DraftCharacter d;
    int guard = 0;
    while (guard++ < 1000) {
      bool any = d.inc(kStr) || d.inc(kDex) || d.inc(kInt) || d.inc(kSpi);
      if (!any) break;
    }
    check(d.attr[kStr] == kAttrMax && d.attr[kDex] == kAttrMax &&
              d.attr[kInt] == kAttrMax && d.attr[kSpi] == kAttrMax,
          "4 屬性全封頂(各 18)");
    check(!d.inc(kStr) && !d.inc(kDex) && !d.inc(kInt) && !d.inc(kSpi),
          "全封頂後 inc 全失敗");
    check(d.spent() == 4 * (kAttrMax - kAttrStart), "spent()==4×9==36");
    check(d.spent() + d.leftover() == kPointPool, "守恆:全封頂後");
    check(d.leftover() == kPointPool - 36, "leftover==50-36==14(建議保留)");
  }

  // ── C/D/E. round-trip + SDA + 名字編碼 ──────────────────────────
  {
    DraftCharacter d;
    d.name = "Aria";
    d.gender = 1;                       // 女
    // 配點:STR+5, DEX+7, INT+1, SPI+1(DEX=16 → DEX/4=4)。
    for (int i = 0; i < 5; ++i) d.inc(kStr);
    for (int i = 0; i < 7; ++i) d.inc(kDex);
    d.inc(kInt);
    d.inc(kSpi);
    check(d.attr[kStr] == 14 && d.attr[kDex] == 16 && d.attr[kInt] == 10 &&
              d.attr[kSpi] == 10,
          "配點後屬性值如預期(STR14 DEX16 INT10 SPI10)");
    check(d.name_valid(), "名字合法");

    auto raw = d.serialize();

    // 名字高位元編碼:A r i a → 末字元 'a' 高位元 0,其餘高位元 1。
    check((raw[0] & 0x80) && (raw[0] & 0x7F) == 'A', "名[0]='A' 高位元設");
    check((raw[1] & 0x80) && (raw[1] & 0x7F) == 'r', "名[1]='r' 高位元設");
    check((raw[2] & 0x80) && (raw[2] & 0x7F) == 'i', "名[2]='i' 高位元設");
    check(((raw[3] & 0x80) == 0) && (raw[3] & 0x7F) == 'a', "名末字元 'a' 高位元清除(終止)");

    // 由 Party 解析回 CharacterRecord(走既有 parse_record)。
    std::vector<std::array<std::uint8_t, 512>> recs{raw};
    Party p = Party::from_raw_records(recs);
    check(p.size() == 1, "Party 解析得 1 名角色");
    const CharacterRecord& c = p.at(0);

    check(c.name == "Aria", "round-trip 名字");
    check(c.strength == 14 && c.max_strength == 14, "round-trip STR cur=max=14");
    check(c.dexterity == 16 && c.max_dexterity == 16, "round-trip DEX cur=max=16");
    check(c.intel == 10 && c.max_intel == 10, "round-trip INT cur=max=10");
    check(c.spirit == 10 && c.max_spirit == 10, "round-trip SPI cur=max=10");
    check(c.gender == 1, "round-trip 性別=女");
    check(c.level == 1, "round-trip level=1");
    check(c.xp == 0, "round-trip xp=0");

    // 衍生值 round-trip(草稿公式 == 解析回的 record 值)。
    check((int)c.health == d.derived_hp() && c.health == c.max_health,
          "round-trip HP=衍生(cur=max)");
    check((int)c.stun == d.derived_stun() && c.stun == c.max_stun,
          "round-trip STUN=衍生(=HP)");
    check((int)c.power == d.derived_power() && c.power == c.max_power,
          "round-trip PWR=衍生(cur=max)");
    // 直接比對 raw 2B LE(serialize 寫對位置)。
    check(rd16(raw, 0x14) == d.derived_hp(), "raw[20-21] HP LE 正確");
    check(rd16(raw, 0x1C) == d.derived_power(), "raw[28-29] PWR LE 正確");

    // SDA:effective AV/DV == DEX/4(stored=0 → 走計算路徑)。
    check(c.av == 0 && c.dv == 0 && c.ac == 0, "stored AV/DV/AC=0(runtime 計算)");
    check(c.effective_av() == 16 / 4, "effective_av == DEX/4 == 4");
    check(c.effective_dv() == 16 / 4, "effective_dv == DEX/4 == 4");
    check(c.effective_av() == d.base_av() && c.effective_dv() == d.base_dv(),
          "effective AV/DV == 草稿 base_av/base_dv");
  }

  // ── F. 確定性:同草稿兩次 serialize → byte-identical ────────────
  {
    DraftCharacter d;
    d.name = "Borin";
    d.gender = 0;
    for (int i = 0; i < 9; ++i) d.inc(kStr);
    auto a = d.serialize();
    auto b = d.serialize();
    check(a == b, "確定性:同草稿兩次 serialize byte-identical");
  }

  // ── G. 組隊:1~4 名草稿 → Party ─────────────────────────────────
  {
    std::vector<std::array<std::uint8_t, 512>> recs;
    const char* names[4] = {"Kael", "Mira", "Dorn", "Sela"};
    for (int i = 0; i < 4; ++i) {
      DraftCharacter d;
      d.name = names[i];
      d.gender = (std::uint8_t)(i & 1);
      d.inc(kDex); d.inc(kDex);
      recs.push_back(d.serialize());
    }
    Party party = Party::from_raw_records(recs);
    check(party.size() == 4, "組隊:4 名角色");
    bool order_ok = true;
    for (int i = 0; i < 4; ++i)
      if (party.at(i).name != names[i]) order_ok = false;
    check(order_ok, "組隊:順序與名字一致");
    // 1 名隊伍也可。
    Party solo = Party::from_raw_records({recs[0]});
    check(solo.size() == 1 && solo.at(0).name == "Kael", "組隊:1 名角色亦可");
  }

  // ── 邊界:空名 / 過長名 不合法 ──────────────────────────────────
  {
    DraftCharacter d;
    d.name = "";
    check(!d.name_valid(), "空名不合法");
    d.name = "ThisNameIsWayTooLong";
    check(!d.name_valid(), "過長名(>11)不合法");
    d.name = "Ok";
    check(d.name_valid(), "正常名合法");
  }

  std::printf("%s: chargen (%d checks failed)\n", g_fail ? "FAIL" : "PASS", g_fail);
  return g_fail ? 1 : 0;
}
