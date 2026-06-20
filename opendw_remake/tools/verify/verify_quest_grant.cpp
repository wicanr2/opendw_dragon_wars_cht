// verify_quest_grant — 端到端驗證 grounded quest 物品給予:
//   讀 quest/grants.tsv → make_quest_item(name) 編 23B → Party::add_item 進背包 →
//   raw_records round-trip 後仍在(持久)→ 再給同名走 party_has_item 判重(不重給)→
//   每件 quest 物品名都有 zh-TW 譯(items.tsv 覆蓋)。
#include <cstdio>
#include <array>
#include <cstdint>
#include <fstream>
#include <map>
#include <string>
#include <vector>
#include "game/party.hpp"
#include "game/equipment.hpp"
#include "i18n/strings.hpp"

using namespace dw;
static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){std::printf("FAIL: %s\n", m);++g_fail;} else std::printf("ok: %s\n", m);}while(0)

// 與 main.cpp make_quest_item 同編碼:header 全 0;name 走 7-bit 高位元終止(非末字|0x80)。
static std::array<std::uint8_t, 23> make_quest_item(const std::string& name) {
  std::array<std::uint8_t, 23> rec{};
  rec[6] = 0x80;   // header 非零 → parse_item present=true(同 main.cpp make_quest_item)
  std::size_t n = name.size() < 12 ? name.size() : 12;
  for (std::size_t i = 0; i < n; ++i) {
    std::uint8_t b = (std::uint8_t)(name[i] & 0x7F);
    if (i + 1 < n) b |= 0x80;
    rec[11 + i] = b;
  }
  return rec;
}

static bool party_has_item(const game::Party& p, const std::string& name) {
  for (std::size_t i = 0; i < p.size(); ++i)
    for (int s = 0; s < game::CharacterRecord::kInventorySlots; ++s) {
      auto it = p.at(i).item_at(s);
      if (it.present && it.name == name) return true;
    }
  return false;
}

int main(int argc, char** argv) {
  std::string bundle = (argc > 1) ? argv[1] : "assets/bundle";
  std::string i18n = (argc > 2) ? argv[2] : "assets/i18n";

  // 讀 grants.tsv 的 english_name(第 3 欄)。
  std::vector<std::string> names;
  { std::ifstream gf(bundle + "/quest/grants.tsv"); std::string line;
    while (std::getline(gf, line)) {
      if (line.empty() || line[0] == '#') continue;
      std::vector<std::string> f; std::string cur;
      for (char c : line) { if (c == '\t') { f.push_back(cur); cur.clear(); } else cur.push_back(c); }
      f.push_back(cur);
      if (f.size() >= 3 && !f[2].empty()) names.push_back(f[2]);
    } }
  CHECK(!names.empty(), "grants.tsv 載入到 quest 物品(>=1)");

  auto party = game::Party::load_default(bundle);
  CHECK(party.size() > 0, "預設隊伍載入成功");
  if (party.size() == 0 || names.empty()) { std::printf("\nQUEST GRANT FAIL\n"); return 1; }

  // 載 items.tsv 檢查譯名覆蓋。
  auto tr = i18n::Strings::load(i18n + "/zh-TW/items.tsv");
  CHECK(tr.has_value(), "items.tsv 可載入(譯名來源)");

  // 逐件:編碼 → 解析回名稱一致 → 給入背包 → 判重。
  for (const auto& nm : names) {
    auto rec = make_quest_item(nm);
    auto parsed = game::parse_item(rec.data(), 23);
    bool name_ok = parsed.present && parsed.name == nm;
    CHECK(name_ok, ("編碼 round-trip 名稱一致: " + nm).c_str());

    bool had = party_has_item(party, nm);
    int slot = party.add_item(0, rec);
    CHECK(slot >= 0 && !had, ("首次給入背包: " + nm).c_str());

    // 判重:已持有 → 不再給。
    bool now_has = party_has_item(party, nm);
    CHECK(now_has, ("給入後 party_has_item 為真(判重依據): " + nm).c_str());

    // 譯名覆蓋:items.tsv 應有此鍵且非回退原文。
    if (tr) {
      std::string zh = tr->tr(nm);
      CHECK(zh != nm && !zh.empty(), ("zh-TW 譯名存在: " + nm + " -> " + zh).c_str());
    }
  }

  // 存檔 round-trip:全部 quest 物品在 raw_records → from_raw_records 後仍在。
  auto reloaded = game::Party::from_raw_records(party.raw_records());
  for (const auto& nm : names)
    CHECK(party_has_item(reloaded, nm), ("存檔 round-trip 後仍持有: " + nm).c_str());

  if (g_fail == 0) { std::printf("\nQUEST GRANT PASS\n"); return 0; }
  std::printf("\nQUEST GRANT FAIL (%d)\n", g_fail); return 1;
}
