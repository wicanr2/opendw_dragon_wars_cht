// verify_chest_acquire — 端到端驗證:grounded 寶箱開箱給的真實物品經 Party::add_item
//   寫進第 0 名背包,並通過 raw_records()→from_raw_records()(= 存檔 round-trip 的搬運層)
//   後仍存在 → 證明「寶箱物品能拿 + 存檔持久」。對齊 main.cpp 寶箱 K 分支與 items.bin 載入。
#include <cstdio>
#include <array>
#include <cstdint>
#include <string>
#include <vector>
#include "game/party.hpp"
#include "game/equipment.hpp"

using namespace dw;
static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){std::printf("FAIL: %s\n", m);++g_fail;} else std::printf("ok: %s\n", m);}while(0)

// 與 main.cpp 同格式載入 items.bin(DWITM 標頭;cnt@8;每筆 4B data1_off + 23B 記錄)。
static std::vector<std::array<std::uint8_t, 23>> load_items(const std::string& path) {
  std::vector<std::array<std::uint8_t, 23>> pool;
  std::FILE* f = std::fopen(path.c_str(), "rb");
  if (!f) return pool;
  std::fseek(f, 0, SEEK_END); long n = std::ftell(f); std::fseek(f, 0, SEEK_SET);
  std::vector<std::uint8_t> blob(n > 0 ? (std::size_t)n : 0);
  if (!blob.empty() && std::fread(blob.data(), 1, blob.size(), f) == blob.size() &&
      blob.size() >= 10 && blob[0]=='D'&&blob[1]=='W'&&blob[2]=='I'&&blob[3]=='T'&&blob[4]=='M') {
    std::uint16_t cnt = (std::uint16_t)(blob[8] | (blob[9] << 8));
    std::size_t off = 10;
    for (std::uint16_t k = 0; k < cnt && off + 4 + 23 <= blob.size(); ++k) {
      off += 4;
      std::array<std::uint8_t, 23> rec{};
      for (int b = 0; b < 23; ++b) rec[(std::size_t)b] = blob[off + b];
      pool.push_back(rec);
      off += 23;
    }
  }
  std::fclose(f);
  return pool;
}

int main(int argc, char** argv) {
  std::string bundle = (argc > 1) ? argv[1] : "assets/bundle";

  auto pool = load_items(bundle + "/items/items.bin");
  CHECK(!pool.empty(), "items.bin 物品池非空(寶箱給物品來源)");
  if (pool.empty()) { std::printf("\nCHEST ACQUIRE FAIL\n"); return 1; }

  auto party = game::Party::load_default(bundle);
  CHECK(party.size() > 0, "預設隊伍載入成功");
  if (party.size() == 0) { std::printf("\nCHEST ACQUIRE FAIL\n"); return 1; }

  // 模擬 main.cpp 寶箱 K 成功分支:依「位置」決定性選一件 → add_item 給第 0 名。
  int current_area = 2, px = 7, py = 7;   // area2 真實寶箱格(find_chests 實測)
  std::size_t idx = (std::size_t)((current_area + px * 7 + py * 13) % (int)pool.size());
  auto expect = game::parse_item(pool[idx].data(), 23);
  CHECK(expect.present, "選中的寶箱物品本身有效(present)");
  std::printf("  寶箱物品 idx=%zu name='%s'\n", idx, expect.name.c_str());

  int slot = party.add_item(0, pool[idx]);
  CHECK(slot >= 0, "add_item 找到空背包格(slot>=0)");

  // 立即驗:第 0 名背包該格有物品且名稱相符。
  auto got = party.at(0).item_at(slot);
  CHECK(got.present && got.name == expect.name, "加入後第 0 名背包該格 = 該物品");

  // ── 存檔 round-trip(raw_records → from_raw_records;= 存/讀檔搬運層)──
  auto recs = party.raw_records();
  auto reloaded = game::Party::from_raw_records(recs);
  CHECK(reloaded.size() == party.size(), "round-trip 後隊伍人數不變");
  auto rget = reloaded.at(0).item_at(slot);
  CHECK(rget.present && rget.name == expect.name,
        "存檔 round-trip 後物品仍在背包該格(端到端持久化)");

  // 同箱不重複:再開同一格不應再給(由 main.cpp opened_chests 守;此處驗 add_item 會用下一空格)。
  int slot2 = party.add_item(0, pool[idx]);
  CHECK(slot2 >= 0 && slot2 != slot, "二次 add_item 落在不同空格(背包不覆寫)");

  if (g_fail == 0) { std::printf("\nCHEST ACQUIRE PASS\n"); return 0; }
  std::printf("\nCHEST ACQUIRE FAIL (%d)\n", g_fail); return 1;
}
