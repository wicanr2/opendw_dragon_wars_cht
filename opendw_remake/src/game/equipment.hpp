// equipment — 物品/裝備格式解析(fraterrisus 23B 物品欄格式)。
//
// Deep module:對外只露「把一格 23 byte 物品欄 bytes 解析成 ItemInstance」與
//   幾個查表便利函式(物品類型 → i18n 鍵、傷害骰 → 文字)。內部隱藏 88-bit
//   bit-packed 佈局、售價指數/尾數編碼、高位元終止字串解碼。
//
// 來源(權威依據):
//   - docs/44_DATA_FORMATS_AND_MECHANICS.md §2 裝備格式(= fraterrisus
//     《Dragon Wars Hex Editing Guide (PC)》Equipment Format)。
//   - 一格物品欄 = 23 byte = 11 byte(88 bit)bit-packed 欄位 + 12 byte 物品名
//     (高位元終止字串,末位元組高位元清除;名最長 12B,不足以 0 填補)。
//
// byte-grounded 驗證(tools/verify/verify_equipment + extract_items):
//   以真實 DATA1 物品記錄對拍解碼,例:
//     "Dragon Stone" hdr=4100000039008414000000
//       → type=general、price=250、magic 高位元組 0x84=回復法力。
//     "Fire Shield"  hdr=80bf000f00018000000000
//       → type=shield(0x01)、AV 修正 −15。
//   證明 type / 售價 / AV 修正 / 名稱解碼合理(見 docs/44 §2 與 verify 註解)。
#pragma once

#include <array>
#include <cstdint>
#include <string>

#include "game/damage_dice.hpp"

namespace dw::game {

// 物品類型(fraterrisus item type,bit[40-47] 低 5 bit)。
// 對照 docs/44 §2 物品類型表;名稱以英文鍵表示,i18n 由 item_type_key() 轉譯。
enum class ItemType : std::uint8_t {
  General      = 0x00,  // 一般 / 任務物品
  Shield       = 0x01,
  FullShield   = 0x02,
  Axe          = 0x03,
  Flail        = 0x04,
  Sword        = 0x05,
  TwoHander    = 0x06,
  Mace         = 0x07,
  Bow          = 0x08,
  Crossbow     = 0x09,
  Gun          = 0x0A,
  Thrown       = 0x0B,
  Ammo         = 0x0C,
  Gloves       = 0x0D,
  MageGloves   = 0x0E,
  AmmoClip     = 0x0F,
  CuirBouilli  = 0x10,  // Cuir Bouilli 甲
  Brigandine   = 0x11,
  Scale        = 0x12,  // 鱗甲
  Chain        = 0x13,  // 鎖鏈甲
  PlateChain   = 0x14,  // 板鏈甲
  FullPlate    = 0x15,  // 全板甲
  Helmet       = 0x16,
  Scroll       = 0x17,
  Boots        = 0x18,
};

// 物品類型 → i18n 英文鍵(供 i18n/items.tsv 查表;查無回退英文)。
const char* item_type_key(ItemType t);

// 物品類型大分類(供 UI / 戰鬥邏輯判斷)。
bool is_weapon(ItemType t);   // 近戰/遠程武器(斧/劍/錘/弓/弩…)
bool is_armor(ItemType t);    // 身體護甲(Cuir Bouilli…全板甲)
bool is_shield(ItemType t);   // 盾 / 全身盾
bool is_helmet(ItemType t);

// 完整解析出的一格物品(fraterrisus 23B 物品欄格式)。
// 欄位全部對齊 docs/44 §2;未記載的 bit(01-02/09/16-18 等)不展開(留 TODO)。
struct ItemInstance {
  bool present = false;          // 該格是否有物品(11B header 非全 0)

  // ── bit-packed 欄位(docs/44 §2)──────────────────────────────────────
  bool equipped = false;         // bit[00] 已裝備
  int charges = 0;               // bit[03-07] 充能/使用次數
  bool av_mod_negative = false;  // bit[08] AV 修正為負
  int req_attr_skill = 0;        // bit[10-15] 需求屬性/技能(selector;0=無)
  int req_value = 0;             // bit[19-23] 需求值(0=無需求)
  int av_mod = 0;                // bit[24-27] AV 修正(已套 bit[08] 正負號)
  int ac_mod = 0;                // bit[28-31] AC 修正(恆正)
  int sale_price = 0;            // 售價 = 購買價 ÷ 2(購買價於 bit[32-39] 編碼)
  int purchase_price = 0;        // bit[32-39] 購買價(M×10^E;= 2× 售價)
  ItemType type = ItemType::General;  // bit[40-47] 低 5 bit
  std::uint16_t magic_effect = 0;     // bit[48-63] 魔法效果(2 byte;見下方註)
  DamageDice primary_dmg;        // bit[64-71] 主傷害骰
  DamageDice secondary_dmg;      // bit[72-79] 次傷害骰(有射程近戰武器)
  int ammo_type = 0;             // bit[80-83] 彈藥類型(0=弓 1=弩)
  int range_ft = 0;              // bit[84-87] 射程(×10 呎)→ 已乘 10

  std::string name;              // 高位元終止字串(物品名)

  std::array<std::uint8_t, 11> raw{};  // 原始 11B header(供 round-trip / 對拍)

  // 魔法效果便利判斷(docs/44 §2;fraterrisus magical effects):
  //   magic 高位元組 [48-55]:
  //     0x00          可充能但無魔法效果
  //     0b00xxxxxx    使用時施放法術(法術 id = bit[50-55])
  //     0x80          無魔法效果
  //     0x83          教授法術(法術 id 在低位元組)
  //     0x84          回復法力(數量在低位元組)
  std::uint8_t magic_hi() const { return static_cast<std::uint8_t>(magic_effect & 0xFF); }
  std::uint8_t magic_lo() const { return static_cast<std::uint8_t>(magic_effect >> 8); }
  bool casts_spell() const { return (magic_hi() & 0xC0) == 0x00 && magic_hi() != 0x00; }
  int cast_spell_id() const { return magic_hi() & 0x3F; }
  bool teaches_spell() const { return magic_hi() == 0x83; }
  bool restores_power() const { return magic_hi() == 0x84; }
};

// 解析一格物品欄(23 byte;p 指向該格起點;need 為可讀位元組數,須 ≥ 11)。
// header 全 0 → present=false。名字自 p+11 起最多讀 12B(高位元終止)。
ItemInstance parse_item(const std::uint8_t* p, std::size_t avail);

// 售價編碼解碼(指數 3 bit + 尾數 5 bit;值 = M × 10^E)。
// 對照 docs/44 §2:0x08=8、0x48=800、0xFF=310,000,000;0x00=不可販售。
int decode_price(std::uint8_t encoded);

}  // namespace dw::game
