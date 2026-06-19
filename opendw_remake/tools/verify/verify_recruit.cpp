// verify_recruit — 酒館招募 NPC 確定性 PASS/FAIL(ctest)。
//
// 涵蓋任務驗證項:
//  1. 名冊:roster() 含 4 名 NPC(Ulrik 0x01 / Louie 0x03 / Valar 0x04 / Halifax 0x05)。
//  2. NPC record 序列化:512B 合法、名字解析回正確、identifier 寫入 [77]、屬性/HP 寫對。
//  3. 招募:NPC 入隊(party.size +1)、新成員 raw[77]=identifier。
//  4. identifier[77] gate:同 NPC 不可重複招募(reason=recruit_already)。
//  5. 未知 id:擋下(reason=recruit_unknown)。
//  6. 隊伍上限 7:滿 7 員 → 擋下(reason=recruit_full)。
//  7. 隊伍 round-trip:招募後 raw_records → from_raw_records byte-for-byte 一致(存檔相容)。
//  8. >4 員處理:招募使隊伍可達 5/6/7 員,size() 正確、各成員可解析。
//
// grounded(512B 格式 + identifier[77] + NPC 名=fraterrisus docs/reverse-engineering/44 §1)vs remake
// (NPC 屬性/招募邏輯,見 recruit.hpp 檔頭),測試只驗「行為自洽 + gate + 上限 + round-trip」。
#include "game/chargen.hpp"
#include "game/party.hpp"
#include "game/recruit.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

using namespace dw::game;

namespace {
int g_fail = 0;
void check(bool cond, const char* what) {
  std::printf("  [%s] %s\n", cond ? "PASS" : "FAIL", what);
  if (!cond) g_fail++;
}

std::array<std::uint8_t, 512> make_pc(const char* name) {
  DraftCharacter d;
  d.name = name;
  return d.serialize();  // identifier[77]=0(玩家自建,非 NPC)
}
}  // namespace

int main(int argc, char** argv) {
  std::filesystem::path bundle = (argc > 1) ? argv[1] : "assets/bundle";
  (void)bundle;
  std::printf("== verify_recruit ==\n");

  // ── 1. 名冊 ──────────────────────────────────────────────────────────
  std::printf("-- A. roster --\n");
  const auto& roster = RecruitRoster::roster();
  check(roster.size() == 4, "roster has 4 recruitable NPCs");
  check(RecruitRoster::find(0x01) && RecruitRoster::find(0x01)->name == "Ulrik", "Ulrik=0x01");
  check(RecruitRoster::find(0x03) && RecruitRoster::find(0x03)->name == "Louie", "Louie=0x03");
  check(RecruitRoster::find(0x04) && RecruitRoster::find(0x04)->name == "Valar", "Valar=0x04");
  check(RecruitRoster::find(0x05) && RecruitRoster::find(0x05)->name == "Halifax", "Halifax=0x05");
  check(RecruitRoster::find(0x02) == nullptr, "unknown id 0x02 not in roster");

  // ── 2. NPC record 序列化 ─────────────────────────────────────────────
  std::printf("-- B. NPC serialize --\n");
  {
    const NpcTemplate* t = RecruitRoster::find(0x01);
    auto rec = t->serialize();
    check(rec[77] == 0x01, "identifier written to [77]");
    check(rec[0x0C] == t->strength && rec[0x0E] == t->dexterity, "attrs written");
    check(rec[0x4F] == t->level, "level written to [79]");
    Party p = Party::from_raw_records({rec});
    check(p.at(0).name == "Ulrik", "name parses back as 'Ulrik'");
    check(p.at(0).strength == t->strength, "strength parses back");
  }

  // ── 3/4/5. 招募 + gate + 未知 ────────────────────────────────────────
  std::printf("-- C. recruit + gate --\n");
  {
    Party party = Party::from_raw_records({make_pc("Hero"), make_pc("Mage")});
    check(party.size() == 2, "start party size 2");

    RecruitResult r1 = recruit_npc(party, 0x01);  // Ulrik
    check(r1.ok, "recruit Ulrik ok");
    check(party.size() == 3, "party size 3 after recruit");
    check(party.at(2).raw[77] == 0x01, "new member raw[77]=Ulrik id");

    RecruitResult r2 = recruit_npc(party, 0x01);  // 重複 Ulrik
    check(!r2.ok, "re-recruit Ulrik blocked");
    check(std::string(r2.reason) == "recruit_already", "reason=recruit_already");
    check(party.size() == 3, "party size unchanged on duplicate");

    RecruitResult r3 = recruit_npc(party, 0x02);  // 未知 id
    check(!r3.ok, "recruit unknown id blocked");
    check(std::string(r3.reason) == "recruit_unknown", "reason=recruit_unknown");
  }

  // ── 6. 隊伍上限 7 + >4 員 + round-trip ────────────────────────────────
  std::printf("-- D. party cap 7 + >4 + round-trip --\n");
  {
    // 4 名 PC 起手(已滿主戰槽)。
    Party party = Party::from_raw_records(
        {make_pc("A"), make_pc("B"), make_pc("C"), make_pc("D")});
    check(party.size() == 4, "start with 4 PCs");

    // 招募 4 名 NPC:應只能上 3 名(達 7 上限),第 4 名擋下。
    int recruited = 0;
    for (std::uint8_t id : {0x01, 0x03, 0x04, 0x05}) {
      RecruitResult r = recruit_npc(party, id);
      if (r.ok) ++recruited;
      else check(std::string(r.reason) == "recruit_full", "4th recruit blocked by cap");
    }
    check(recruited == 3, "exactly 3 NPCs recruited to fill 7 slots");
    check(party.size() == 7, "party at max 7");

    // >4 員:各成員可解析、identifier 正確。
    bool all_parse = true;
    for (std::size_t i = 0; i < party.size(); ++i)
      if (party.at(i).name.empty()) all_parse = false;
    check(all_parse, "all 7 members parse (name non-empty)");

    // round-trip:存→讀→存 byte-for-byte。
    auto recs = party.raw_records();
    Party p2 = Party::from_raw_records(recs);
    auto recs2 = p2.raw_records();
    bool identical = recs.size() == recs2.size();
    for (std::size_t i = 0; i < recs.size() && identical; ++i)
      if (recs[i] != recs2[i]) identical = false;
    check(identical, "party 7-member round-trip byte-for-byte");
    check(p2.size() == 7, "round-trip preserves 7 members");
    // identifier gate 在 round-trip 後仍生效:已招的 Ulrik(0x01)→ recruit_already(gate 先於 cap)。
    RecruitResult r = recruit_npc(p2, 0x01);
    check(!r.ok && std::string(r.reason) == "recruit_already",
          "post-roundtrip identifier gate still blocks duplicate");
  }

  std::printf(g_fail == 0 ? "verify_recruit: ALL PASS\n" : "verify_recruit: %d FAIL\n", g_fail);
  return g_fail == 0 ? 0 : 1;
}
