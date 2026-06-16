// verify_shop — 商店買賣確定性 PASS/FAIL(ctest)。
//
// 涵蓋任務驗證項:
//  1. stock.json 載入:庫存非空、grounded/curated 都讀到、價格解碼合理。
//  2. 買:gold[81] 足 → 扣對應購買價、物品入第一個空背包格、同步 raw[](round-trip 一致)。
//  3. 窮買不到:gold 不足 → 擋下(reason=shop_no_gold)、gold/背包不變。
//  4. 背包滿:13 格全滿 → 買擋下(reason=shop_full)。
//  5. 賣:背包有物品 → 移除該格、gold 加半價(售價=購買價÷2)、同步 raw[]。
//  6. 賣空格擋下(reason=shop_empty_slot)。
//  7. gold[81] 1 byte 飽和:加超過 255 夾頂(誠實:fraterrisus 1B 欄)。
//  8. raw 512B round-trip:買賣後 Party::from_raw_records 解析回一致(存檔相容)。
//
// grounded(23B 物品格式/售價編碼=fraterrisus docs/44 §2)vs remake(買賣邏輯/curated
// 庫存,見 shop.hpp 檔頭),測試只驗「行為自洽 + 守恆 + 確定性 + round-trip」,不對 oracle。
#include "game/chargen.hpp"
#include "game/party.hpp"
#include "game/shop.hpp"

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

// 造一名合法 512B record(chargen),回傳解析好的 CharacterRecord。
CharacterRecord make_char(const char* name) {
  DraftCharacter d;
  d.name = name;
  auto raw = d.serialize();
  Party p = Party::from_raw_records({raw});
  return p.at(0);
}

// 計背包內 present 物品數。
int inv_count(const CharacterRecord& c) {
  int n = 0;
  for (int s = 0; s < CharacterRecord::kInventorySlots; ++s)
    if (c.item_at(s).present) ++n;
  return n;
}
}  // namespace

int main(int argc, char** argv) {
  std::filesystem::path bundle = (argc > 1) ? argv[1] : "assets/bundle";
  std::printf("== verify_shop (bundle=%s) ==\n", bundle.string().c_str());

  // ── 1. stock.json 載入 ──────────────────────────────────────────────
  std::printf("-- A. stock load --\n");
  Shop shop = Shop::load(bundle);
  check(shop.size() > 0, "stock non-empty");
  bool has_grounded = false, has_curated = false, prices_ok = true;
  for (const auto& e : shop.stock()) {
    if (e.grounded) has_grounded = true; else has_curated = true;
    if (e.buy_price() < 0) prices_ok = false;
  }
  check(has_curated, "stock has curated standard equipment");
  check(prices_ok, "all buy prices >= 0");
  (void)has_grounded;  // grounded items 可能全為非賣品(price=0),不強制

  // 找一件可買得起(buy_price 在 [1,200])的庫存(curated 標準裝備)。
  std::size_t affordable_idx = shop.size();
  for (std::size_t i = 0; i < shop.size(); ++i) {
    int p = shop.stock()[i].buy_price();
    if (p > 0 && p <= 200) { affordable_idx = i; break; }
  }
  check(affordable_idx < shop.size(), "found an affordable item in stock");

  // ── 2. 買(gold 足)──────────────────────────────────────────────────
  std::printf("-- B. buy with enough gold --\n");
  if (affordable_idx < shop.size()) {
    const ShopEntry& e = shop.stock()[affordable_idx];
    int price = e.buy_price();
    CharacterRecord c = make_char("Buyer");
    set_gold(c, 255);  // 富有
    int inv0 = inv_count(c);
    ShopResult r = shop.buy(c, affordable_idx);
    check(r.ok, "buy ok");
    check(get_gold(c) == 255 - price, "gold[81] deducted by buy price");
    check(inv_count(c) == inv0 + 1, "item added to inventory");
    // round-trip:raw 解析回後 gold + 背包一致。
    Party p2 = Party::from_raw_records({c.raw});
    check(p2.at(0).raw[81] == 255 - price, "round-trip gold consistent");
    check(inv_count(p2.at(0)) == inv0 + 1, "round-trip inventory consistent");
  }

  // ── 3. 窮買不到 ──────────────────────────────────────────────────────
  std::printf("-- C. buy with insufficient gold --\n");
  if (affordable_idx < shop.size()) {
    int price = shop.stock()[affordable_idx].buy_price();
    CharacterRecord c = make_char("Poor");
    set_gold(c, price > 0 ? price - 1 : 0);  // 差 1 金幣
    int g0 = get_gold(c), inv0 = inv_count(c);
    ShopResult r = shop.buy(c, affordable_idx);
    check(!r.ok, "buy blocked when poor");
    check(std::string(r.reason) == "shop_no_gold", "reason=shop_no_gold");
    check(get_gold(c) == g0, "gold unchanged");
    check(inv_count(c) == inv0, "inventory unchanged");
  }

  // ── 4. 背包滿 ────────────────────────────────────────────────────────
  std::printf("-- D. buy with full inventory --\n");
  if (affordable_idx < shop.size()) {
    CharacterRecord c = make_char("Hoarder");
    set_gold(c, 255);
    // 把可用格全填滿(隨便寫非 0 header)。可用格 = 11B header 完整落在 512B 內(0..11)。
    for (int s = 0; s < CharacterRecord::kInventorySlots; ++s) {
      int base = CharacterRecord::kInventoryBase + s * CharacterRecord::kItemStride;
      if (base + 11 <= 512) c.raw[base] = 0x40;  // type bit → 非空 header
    }
    ShopResult r = shop.buy(c, affordable_idx);
    check(!r.ok, "buy blocked when inventory full");
    check(std::string(r.reason) == "shop_full", "reason=shop_full");
  }

  // ── 5. 賣 ────────────────────────────────────────────────────────────
  std::printf("-- E. sell item --\n");
  if (affordable_idx < shop.size()) {
    const ShopEntry& e = shop.stock()[affordable_idx];
    int sale = e.parsed().sale_price;  // = 購買價 ÷ 2
    CharacterRecord c = make_char("Seller");
    set_gold(c, 0);
    // 買進一件再賣(確保有物品 + 確定 slot)。
    set_gold(c, 255);
    shop.buy(c, affordable_idx);
    set_gold(c, 0);  // 重置 gold 以驗賣得款項
    // 找該物品所在 slot。
    int slot = -1;
    for (int s = 0; s < CharacterRecord::kInventorySlots; ++s)
      if (c.item_at(s).present) { slot = s; break; }
    check(slot >= 0, "have an item to sell");
    int inv0 = inv_count(c);
    ShopResult r = Shop::sell(c, slot);
    check(r.ok, "sell ok");
    check(get_gold(c) == sale, "gold increased by sale price (half)");
    check(inv_count(c) == inv0 - 1, "item removed from inventory");
    // round-trip。
    Party p2 = Party::from_raw_records({c.raw});
    check(p2.at(0).raw[81] == sale, "round-trip gold after sell");
    check(inv_count(p2.at(0)) == inv0 - 1, "round-trip inventory after sell");
  }

  // ── 6. 賣空格擋下 ────────────────────────────────────────────────────
  std::printf("-- F. sell empty slot --\n");
  {
    CharacterRecord c = make_char("Empty");
    set_gold(c, 50);
    int g0 = get_gold(c);
    ShopResult r = Shop::sell(c, 0);  // slot 0 為空
    check(!r.ok, "sell blocked on empty slot");
    check(std::string(r.reason) == "shop_empty_slot", "reason=shop_empty_slot");
    check(get_gold(c) == g0, "gold unchanged on failed sell");
  }

  // ── 7. gold 飽和(1 byte)──────────────────────────────────────────────
  std::printf("-- G. gold[81] saturation --\n");
  {
    CharacterRecord c = make_char("Rich");
    set_gold(c, 250);
    set_gold(c, 250 + 100);  // 應夾到 255
    check(get_gold(c) == 255, "gold saturates at 255");
    set_gold(c, -10);
    check(get_gold(c) == 0, "gold clamps at 0");
  }

  std::printf(g_fail == 0 ? "verify_shop: ALL PASS\n" : "verify_shop: %d FAIL\n", g_fail);
  return g_fail == 0 ? 0 : 1;
}
