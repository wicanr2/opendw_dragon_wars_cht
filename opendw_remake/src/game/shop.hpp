// shop — 商店買賣(金幣經濟的出口:買裝備 / 賣背包物品)。
//
// Deep module:對外只露「載入商店庫存」+「買一件 / 賣一格」兩條窄介面;內部隱藏
//   23B 物品欄寫入、gold[81] 加減與飽和、空格搜尋、512B raw[] 同步(存檔 round-trip
//   一致)。**不重算戰鬥 / 成長公式**(那是 combat.hpp / progression.hpp 的範疇)。
//
// 純資料 + 規則,無 SDL/render 相依 → 可獨立進 ctest(tools/verify/verify_shop.cpp)。
//
// ── 鐵則:raw[] 與解析欄位同步 ──────────────────────────────────────────────
//   買賣即時改 gold8[81] 與物品欄 [236-511]。**必同步寫回 CharacterRecord.raw[]**
//   (對齊 party.cpp award_xp / progression 的作法),確保「存→讀→存」byte-for-byte
//   一致、512B fraterrisus 格式不破壞。offset 對齊 docs/reverse-engineering/44 §1/§2。
//
// ── grounded 來源 vs remake 設計(誠實標示)──────────────────────────────────
//   [grounded,fraterrisus / docs44 §2]:物品 23B 格式 + 售價編碼(M×10^E)。
//       購買價 = decode_price(bit[32-39])(玩家買要付的錢);
//       售價(賣出可得)= 購買價 ÷ 2(equipment.cpp 既有解碼)。
//   [grounded]:金幣存於角色 record gold8[81](fraterrisus;見 party.hpp 註)。
//   [remake 設計,明標——商店買賣邏輯 opendw C 未實作]:
//     • 商店庫存清單(賣哪些物品)= bundle/shop/stock.json curated(來源於 DATA1 真實
//       物品 items.bin + 標準裝備,逐項標 grounded/curated;見 stock.json source 欄)。
//     • 「買」扣付款方角色的 gold[81]、物品入其第一個空背包格;gold 不足 → 擋下。
//     • 「賣」把該格物品移除、加 gold[81](半價);已裝備物品亦可賣(先記為移除)。
//     • gold8[81] 為 1 byte → 上限 255;買賣均夾到 [0,255](誠實:原版 gold 可能更寬,
//       但 remake 以 fraterrisus 1B 欄為準,飽和處理避免破壞 512B 格式)。
#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "game/equipment.hpp"
#include "game/party.hpp"

namespace dw::game {

// 商店庫存一筆(可購買的物品)。
struct ShopEntry {
  std::array<std::uint8_t, 23> bytes{};  // 完整 23B 物品欄(header 11B + 名 12B)
  std::string name_key;                  // i18n 鍵(查無回退物品自身英文名)
  bool grounded = false;                 // true=源自 DATA1 真實物品;false=remake curated 標準裝備
  ItemInstance parsed() const { return parse_item(bytes.data(), 23); }
  // 購買價(玩家買要付):decode_price(bit[32-39])。0=非賣品。
  int buy_price() const { return parsed().purchase_price; }
};

// 買 / 賣的結果(供 UI 提示 + 驗證)。
struct ShopResult {
  bool ok = false;          // 操作成功
  int gold_delta = 0;       // gold[81] 變動量(買為負、賣為正;已套飽和)
  int gold_after = 0;       // 操作後角色 gold[81]
  const char* reason = "";  // 失敗原因鍵(i18n):"shop_no_gold" / "shop_full" / "shop_empty_slot"
};

// 商店:一份可購買庫存。庫存來源於 bundle/shop/stock.json(curated;見檔頭)。
class Shop {
public:
  // 從 bundle 載入商店庫存(assets/bundle/shop/stock.json + items.bin)。
  // 失敗回傳空商店(stock 為空);仍可用(只是無貨)。
  static Shop load(const std::filesystem::path& bundle_dir);

  // 直接以一組 ShopEntry 建立(供測試)。
  static Shop from_entries(std::vector<ShopEntry> entries);

  const std::vector<ShopEntry>& stock() const { return stock_; }
  std::size_t size() const { return stock_.size(); }

  // 買第 idx 件庫存物品給 buyer:
  //   gold[81] ≥ 購買價 且 背包有空格 → 扣 gold、物品入第一個空格、同步 raw[]。
  //   gold 不足 → reason="shop_no_gold";無空格 → reason="shop_full"。
  // 庫存為「無限供貨」(不從 stock_ 移除;對齊原版商店常駐販售)。
  ShopResult buy(CharacterRecord& buyer, std::size_t idx) const;

  // 賣 seller 第 slot 格(0..12)物品:
  //   該格有物品 → 移除(清 23B)、加 gold[81](售價=購買價÷2,飽和到 255)、同步 raw[]。
  //   空格 → reason="shop_empty_slot"。已裝備物品也可賣(視為移除)。
  static ShopResult sell(CharacterRecord& seller, int slot);

private:
  std::vector<ShopEntry> stock_;
};

// ── 純函式介面(供 UI / 測試;不需 Shop 實例)────────────────────────────────
// 取 / 設角色 gold[81](飽和到 [0,255];同步 raw[81])。
int  get_gold(const CharacterRecord& c);
void set_gold(CharacterRecord& c, int gold);  // 夾到 [0,255]

// 買一件物品(23B)給 buyer,付 price 金幣:gold 足且有空格才成功。
ShopResult buy_item(CharacterRecord& buyer, const std::array<std::uint8_t, 23>& item_bytes, int price);

}  // namespace dw::game
