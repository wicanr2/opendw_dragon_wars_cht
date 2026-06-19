// shop — 商店買賣實作。對齊依據見 shop.hpp 檔頭(docs/reverse-engineering/44 §1/§2 + fraterrisus)。
#include "game/shop.hpp"

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <sstream>

namespace dw::game {

namespace {

// 角色物品欄第 slot 格在 raw[] 的起點(對齊 progression.cpp)。
int slot_base(int slot) {
  return CharacterRecord::kInventoryBase + slot * CharacterRecord::kItemStride;
}

// 該 slot 的 11B header 是否完整落在 512B record 內。
// 注意:docs/reverse-engineering/44 標 13 格(A..M),但 236 + 13×23 = 535 > 512 → 第 12 格(0-based)起點
//   為 512,**完全越界**;實際可用格僅 0..11(12 格)。對齊 equipment.cpp/progression.cpp
//   的「base+i < 512」防護,把越界格視為「不可用 / 永遠非空」→ 不會被買賣寫入。
bool slot_in_bounds(int slot) {
  int base = slot_base(slot);
  return base >= 0 && base + 11 <= 512;
}

// 第 slot 格是否為空(header 11B 全 0)。越界格視為非空(不可用)。
bool slot_empty(const CharacterRecord& c, int slot) {
  if (!slot_in_bounds(slot)) return false;
  int base = slot_base(slot);
  for (int i = 0; i < 11; ++i) if (c.raw[base + i]) return false;
  return true;
}

// 找第一個空背包格;無則回 -1。
int first_empty_slot(const CharacterRecord& c) {
  for (int s = 0; s < CharacterRecord::kInventorySlots; ++s)
    if (slot_in_bounds(s) && slot_empty(c, s)) return s;
  return -1;
}

// 把 23B 物品寫進第 slot 格(同步 raw[];同時清「已裝備」bit[00],新買物品預設未裝備)。
void write_slot(CharacterRecord& c, int slot, const std::array<std::uint8_t, 23>& bytes) {
  int base = slot_base(slot);
  for (int i = 0; i < CharacterRecord::kItemStride && base + i < 512; ++i)
    c.raw[base + i] = bytes[(std::size_t)i];
  c.raw[base] &= ~0x01;  // 新購入未裝備
}

// 清第 slot 格(賣出 / 移除)。
void clear_slot(CharacterRecord& c, int slot) {
  int base = slot_base(slot);
  for (int i = 0; i < CharacterRecord::kItemStride && base + i < 512; ++i)
    c.raw[base + i] = 0;
}

// 極簡 JSON 取值:抓 "key": <int>(僅供讀 stock.json 的數值欄;不做完整 JSON 解析)。
// 字串欄另以 find 抓。穩健度足夠處理本專案自產的 stock.json。
}  // namespace

int get_gold(const CharacterRecord& c) { return c.raw[81]; }

void set_gold(CharacterRecord& c, int gold) {
  if (gold < 0) gold = 0;
  if (gold > 255) gold = 255;
  c.gold8 = static_cast<std::uint8_t>(gold);
  c.raw[81] = c.gold8;
}

ShopResult buy_item(CharacterRecord& buyer, const std::array<std::uint8_t, 23>& item_bytes, int price) {
  ShopResult r;
  r.gold_after = get_gold(buyer);
  if (price < 0) price = 0;
  if (get_gold(buyer) < price) { r.reason = "shop_no_gold"; return r; }
  int slot = first_empty_slot(buyer);
  if (slot < 0) { r.reason = "shop_full"; return r; }
  write_slot(buyer, slot, item_bytes);
  int after = get_gold(buyer) - price;
  set_gold(buyer, after);
  r.ok = true;
  r.gold_delta = -price;
  r.gold_after = get_gold(buyer);
  return r;
}

ShopResult Shop::buy(CharacterRecord& buyer, std::size_t idx) const {
  ShopResult r;
  r.gold_after = get_gold(buyer);
  if (idx >= stock_.size()) { r.reason = "shop_empty_slot"; return r; }
  const ShopEntry& e = stock_[idx];
  return buy_item(buyer, e.bytes, e.buy_price());
}

ShopResult Shop::sell(CharacterRecord& seller, int slot) {
  ShopResult r;
  r.gold_after = get_gold(seller);
  if (slot < 0 || slot >= CharacterRecord::kInventorySlots || slot_empty(seller, slot)) {
    r.reason = "shop_empty_slot";
    return r;
  }
  ItemInstance it = seller.item_at(slot);
  int sale = it.sale_price;  // = 購買價 ÷ 2(equipment.cpp 既有解碼)
  clear_slot(seller, slot);
  int after = get_gold(seller) + sale;
  set_gold(seller, after);  // 飽和到 255
  r.ok = true;
  r.gold_delta = get_gold(seller) - r.gold_after;
  r.gold_after = get_gold(seller);
  return r;
}

Shop Shop::from_entries(std::vector<ShopEntry> entries) {
  Shop s;
  s.stock_ = std::move(entries);
  return s;
}

Shop Shop::load(const std::filesystem::path& bundle_dir) {
  Shop s;
  std::filesystem::path stock_path = bundle_dir / "shop" / "stock.json";
  std::ifstream f(stock_path);
  if (!f) return s;  // 無檔 → 空商店
  std::stringstream ss;
  ss << f.rdbuf();
  std::string txt = ss.str();

  // 解析 "items":[ { "hex":"<46 hex chars>", "name_key":"...", "grounded":true|false }, ... ]
  // hex = 23B 物品欄(46 hex chars);name_key 為 i18n 鍵。極簡掃描(本專案自產 JSON)。
  std::size_t pos = 0;
  auto find_str = [&](const std::string& key, std::size_t from, std::size_t end) -> std::string {
    std::string pat = "\"" + key + "\"";
    std::size_t k = txt.find(pat, from);
    if (k == std::string::npos || k >= end) return {};
    std::size_t colon = txt.find(':', k);
    if (colon == std::string::npos || colon >= end) return {};
    std::size_t q1 = txt.find('"', colon);
    if (q1 == std::string::npos || q1 >= end) return {};
    std::size_t q2 = txt.find('"', q1 + 1);
    if (q2 == std::string::npos || q2 > end) return {};
    return txt.substr(q1 + 1, q2 - q1 - 1);
  };
  auto find_bool = [&](const std::string& key, std::size_t from, std::size_t end) -> bool {
    std::string pat = "\"" + key + "\"";
    std::size_t k = txt.find(pat, from);
    if (k == std::string::npos || k >= end) return false;
    std::size_t t = txt.find("true", k);
    std::size_t ff = txt.find("false", k);
    if (t == std::string::npos) return false;
    if (ff != std::string::npos && ff < t) return false;
    return t < end;
  };

  // 逐個 object(以 '{' 分段;每段一筆庫存)。
  while (true) {
    std::size_t obj = txt.find('{', pos);
    if (obj == std::string::npos) break;
    std::size_t obj_end = txt.find('}', obj);
    if (obj_end == std::string::npos) break;
    pos = obj_end + 1;
    std::string hex = find_str("hex", obj, obj_end);
    if (hex.size() != 46) continue;  // 非物品 object(如最外層）→ 跳過
    ShopEntry e;
    bool ok = true;
    for (int i = 0; i < 23; ++i) {
      auto hexv = [&](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
      };
      int hi = hexv(hex[(std::size_t)i * 2]);
      int lo = hexv(hex[(std::size_t)i * 2 + 1]);
      if (hi < 0 || lo < 0) { ok = false; break; }
      e.bytes[(std::size_t)i] = static_cast<std::uint8_t>((hi << 4) | lo);
    }
    if (!ok) continue;
    e.name_key = find_str("name_key", obj, obj_end);
    if (e.name_key.empty()) e.name_key = e.parsed().name;
    e.grounded = find_bool("grounded", obj, obj_end);
    s.stock_.push_back(std::move(e));
  }
  return s;
}

}  // namespace dw::game
