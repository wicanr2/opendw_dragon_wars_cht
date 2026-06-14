// equipment — 物品/裝備格式解析實作。對齊依據見 equipment.hpp 檔頭(docs/44 §2)。
#include "game/equipment.hpp"

namespace dw::game {

namespace {

// 取 11B header 內 [lo,hi](inclusive)bit 區段;LSB-first across bytes。
// 對齊 docs/44 §2 的 bit offset(與 party.cpp parse_equip 同一套位元規則)。
std::uint32_t bit_field(const std::uint8_t* q, int lo, int hi) {
  std::uint32_t v = 0;
  for (int b = lo; b <= hi; ++b) {
    int byte = b >> 3, bit = b & 7;
    if (q[byte] & (1u << bit)) v |= (1u << (b - lo));
  }
  return v;
}

// 物品名:自 p 起最多讀 12 byte,高位元終止(末位元組高位元清除即止)。
// 與 text_codec / party.cpp read_name 同規則(僅取可列印 7-bit ASCII)。
std::string read_item_name(const std::uint8_t* p, std::size_t avail) {
  std::string s;
  for (std::size_t i = 0; i < 12 && i < avail; ++i) {
    std::uint8_t b = p[i];
    char c = static_cast<char>(b & 0x7F);
    if (c >= 0x20 && c < 0x7F) s.push_back(c);
    if ((b & 0x80) == 0) break;
  }
  return s;
}

}  // namespace

int decode_price(std::uint8_t encoded) {
  // 指數 3 bit(高)+ 尾數 5 bit(低);值 = M × 10^E。0x00 = 不可販售(回 0)。
  int e = (encoded >> 5) & 0x07;
  int m = encoded & 0x1F;
  int v = m;
  for (int i = 0; i < e; ++i) v *= 10;
  return v;
}

const char* item_type_key(ItemType t) {
  switch (t) {
    case ItemType::General:     return "item_general";
    case ItemType::Shield:      return "item_shield";
    case ItemType::FullShield:  return "item_full_shield";
    case ItemType::Axe:         return "item_axe";
    case ItemType::Flail:       return "item_flail";
    case ItemType::Sword:       return "item_sword";
    case ItemType::TwoHander:   return "item_two_hander";
    case ItemType::Mace:        return "item_mace";
    case ItemType::Bow:         return "item_bow";
    case ItemType::Crossbow:    return "item_crossbow";
    case ItemType::Gun:         return "item_gun";
    case ItemType::Thrown:      return "item_thrown";
    case ItemType::Ammo:        return "item_ammo";
    case ItemType::Gloves:      return "item_gloves";
    case ItemType::MageGloves:  return "item_mage_gloves";
    case ItemType::AmmoClip:    return "item_ammo_clip";
    case ItemType::CuirBouilli: return "item_cuir_bouilli";
    case ItemType::Brigandine:  return "item_brigandine";
    case ItemType::Scale:       return "item_scale";
    case ItemType::Chain:       return "item_chain";
    case ItemType::PlateChain:  return "item_plate_chain";
    case ItemType::FullPlate:   return "item_full_plate";
    case ItemType::Helmet:      return "item_helmet";
    case ItemType::Scroll:      return "item_scroll";
    case ItemType::Boots:       return "item_boots";
  }
  return "item_general";
}

bool is_weapon(ItemType t) {
  switch (t) {
    case ItemType::Axe: case ItemType::Flail: case ItemType::Sword:
    case ItemType::TwoHander: case ItemType::Mace: case ItemType::Bow:
    case ItemType::Crossbow: case ItemType::Gun: case ItemType::Thrown:
      return true;
    default: return false;
  }
}

bool is_armor(ItemType t) {
  std::uint8_t v = static_cast<std::uint8_t>(t);
  return v >= static_cast<std::uint8_t>(ItemType::CuirBouilli) &&
         v <= static_cast<std::uint8_t>(ItemType::FullPlate);  // 0x10..0x15
}

bool is_shield(ItemType t) {
  return t == ItemType::Shield || t == ItemType::FullShield;
}

bool is_helmet(ItemType t) { return t == ItemType::Helmet; }

ItemInstance parse_item(const std::uint8_t* p, std::size_t avail) {
  ItemInstance it;
  if (avail < 11) return it;
  for (int i = 0; i < 11; ++i) it.raw[i] = p[i];

  // header 全 0 → 空格。
  bool any = false;
  for (int i = 0; i < 11; ++i) if (p[i]) { any = true; break; }
  if (!any) return it;
  it.present = true;

  it.equipped         = bit_field(p, 0, 0) != 0;
  it.charges          = static_cast<int>(bit_field(p, 3, 7));
  it.av_mod_negative  = bit_field(p, 8, 8) != 0;
  it.req_attr_skill   = static_cast<int>(bit_field(p, 10, 15));
  it.req_value        = static_cast<int>(bit_field(p, 19, 23));
  int av              = static_cast<int>(bit_field(p, 24, 27));
  it.av_mod           = it.av_mod_negative ? -av : av;
  it.ac_mod           = static_cast<int>(bit_field(p, 28, 31));
  std::uint8_t price_enc = static_cast<std::uint8_t>(bit_field(p, 32, 39));
  it.purchase_price   = decode_price(price_enc);
  it.sale_price       = it.purchase_price / 2;  // docs/44:售價 = 購買價 ÷ 2
  it.type             = static_cast<ItemType>(bit_field(p, 40, 44));  // 低 5 bit
  it.magic_effect     = static_cast<std::uint16_t>(bit_field(p, 48, 63));
  it.primary_dmg      = decode_damage_dice(static_cast<std::uint8_t>(bit_field(p, 64, 71)));
  it.secondary_dmg    = decode_damage_dice(static_cast<std::uint8_t>(bit_field(p, 72, 79)));
  it.ammo_type        = static_cast<int>(bit_field(p, 80, 83));
  it.range_ft         = static_cast<int>(bit_field(p, 84, 87)) * 10;  // ×10 呎

  it.name = read_item_name(p + 11, avail - 11);
  return it;
}

}  // namespace dw::game
