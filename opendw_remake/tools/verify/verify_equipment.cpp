// verify_equipment — 物品/裝備格式解碼的 byte-grounded PASS/FAIL 驗證(ctest)。
//
// 對拍(grounded in docs/reverse-engineering/44 §2 = fraterrisus Equipment Format):
//  A. 物品類型解碼:對幾組 (item_type byte) → ItemType + 大分類(武器/護甲/盾)逐項相等。
//  B. 售價編碼:0x08→8、0x48→800、0xFF→310,000,000、0x00→0(指數/尾數 M×10^E)。
//  C. 真實 DATA1 物品萃取對拍:載 bundle/items/items.bin(extract_items 產出),
//     逐筆 parse_item → 斷言「名稱可讀 + 類型/AV 修正解碼 = 期望」。
//     期望值源自對 fraterrisus 規格的人工驗證(Earth/Fire/Aura Shield = shield、
//     Air Armor/Water Wings = cuir_bouilli 甲、Air Talons = two_hander 1d12、
//     Dragon Stone = general + 回復法力 + 售價 250)。
//  D. CharacterRecord 13 格物品欄解析:default_party 全空(present=false)。
//     並以「合成裝備」注入一格 → 驗 inventory()/item_at()/effective_ac() 聚合正確。
//
// 用法:verify_equipment <bundle_dir>
// 退出碼:0=PASS,非 0=FAIL。

#include "game/equipment.hpp"
#include "game/party.hpp"

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

std::vector<std::uint8_t> read_file(const std::filesystem::path& p) {
  std::vector<std::uint8_t> v;
  std::FILE* f = std::fopen(p.string().c_str(), "rb");
  if (!f) return v;
  std::fseek(f, 0, SEEK_END);
  long n = std::ftell(f);
  std::fseek(f, 0, SEEK_SET);
  if (n > 0) { v.resize(static_cast<std::size_t>(n)); if (std::fread(v.data(),1,v.size(),f)!=v.size()) v.clear(); }
  std::fclose(f);
  return v;
}

std::uint16_t rd16(const std::vector<std::uint8_t>& d, std::size_t o) {
  return static_cast<std::uint16_t>(d[o] | (d[o + 1] << 8));
}
std::uint32_t rd32(const std::vector<std::uint8_t>& d, std::size_t o) {
  return static_cast<std::uint32_t>(d[o] | (d[o+1]<<8) | (d[o+2]<<16) | (d[o+3]<<24));
}

// items.bin 內每筆期望(對齊 extract_items kSamples + fraterrisus 驗證)。
struct Expect { const char* name; ItemType type; int av_mod; };
const Expect kExpect[] = {
    {"Air Talons",   ItemType::TwoHander,    0},
    {"Air Armor",    ItemType::CuirBouilli, -8},
    {"Earth Shield", ItemType::Shield,      -10},
    {"Fire Shield",  ItemType::Shield,      -15},
    {"Aura Shield",  ItemType::Shield,      -12},
    {"Water Wings",  ItemType::CuirBouilli, -11},
    {"Dragon Stone", ItemType::General,       0},
};
constexpr int kExpectCount = sizeof(kExpect) / sizeof(kExpect[0]);

}  // namespace

int main(int argc, char** argv) {
  if (argc < 2) { std::fprintf(stderr, "usage: %s <bundle_dir>\n", argv[0]); return 2; }
  const std::filesystem::path bundle = argv[1];

  std::printf("== A. item type decode (fraterrisus item types) ==\n");
  {
    struct C { std::uint8_t t; ItemType want; bool weapon; bool armor; bool shield; };
    const C cs[] = {
        {0x00, ItemType::General,      false, false, false},
        {0x01, ItemType::Shield,       false, false, true},
        {0x02, ItemType::FullShield,   false, false, true},
        {0x03, ItemType::Axe,          true,  false, false},
        {0x05, ItemType::Sword,        true,  false, false},
        {0x08, ItemType::Bow,          true,  false, false},
        {0x10, ItemType::CuirBouilli,  false, true,  false},
        {0x15, ItemType::FullPlate,    false, true,  false},
        {0x16, ItemType::Helmet,       false, false, false},
        {0x17, ItemType::Scroll,       false, false, false},
        {0x18, ItemType::Boots,        false, false, false},
    };
    bool all = true;
    for (const auto& c : cs) {
      ItemType t = static_cast<ItemType>(c.t);
      if (t != c.want || is_weapon(t) != c.weapon || is_armor(t) != c.armor ||
          is_shield(t) != c.shield) {
        std::printf("    0x%02X mismatch\n", c.t);
        all = false;
      }
    }
    check(all, "item type enum + weapon/armor/shield classification (11 cases)");
    check(std::string(item_type_key(ItemType::Shield)) == "item_shield" &&
          std::string(item_type_key(ItemType::FullPlate)) == "item_full_plate",
          "item_type_key i18n keys");
  }

  std::printf("== B. price decode (M x 10^E) ==\n");
  {
    bool ok = decode_price(0x08) == 8 &&      // 0b000 01000 = 8
              decode_price(0x48) == 800 &&    // 0b010 01000 = 8x10^2
              decode_price(0xFF) == 310000000 && // 0b111 11111 = 31x10^7
              decode_price(0x00) == 0;        // 不可販售
    check(ok, "price exponent/mantissa decode (0x08/0x48/0xFF/0x00)");
  }

  std::printf("== C. real DATA1 item records (extract_items, fraterrisus-verified) ==\n");
  {
    auto blob = read_file(bundle / "items" / "items.bin");
    bool have = blob.size() >= 10 && blob[0]=='D'&&blob[1]=='W'&&blob[2]=='I'&&blob[3]=='T'&&blob[4]=='M';
    check(have, "items.bin present + magic 'DWITM'");
    if (have) {
      std::uint16_t ver = rd16(blob, 6);
      std::uint16_t cnt = rd16(blob, 8);
      check(ver == 1, "items.bin version == 1");
      check(static_cast<int>(cnt) == kExpectCount,
            ("item count == " + std::to_string(kExpectCount)).c_str());
      std::size_t off = 10;
      bool all_name = true, all_type = true, all_av = true;
      for (std::uint16_t i = 0; i < cnt && off + 4 + 23 <= blob.size(); ++i) {
        std::uint32_t d1off = rd32(blob, off); off += 4;
        ItemInstance it = parse_item(&blob[off], 23); off += 23;
        const Expect* e = (i < kExpectCount) ? &kExpect[i] : nullptr;
        std::printf("    @0x%05X '%s' type=%s AVmod=%d ACmod=%d pri=%dd%d price=%d charges=%d%s\n",
                    d1off, it.name.c_str(), item_type_key(it.type), it.av_mod, it.ac_mod,
                    it.primary_dmg.count, it.primary_dmg.sides, it.sale_price, it.charges,
                    it.equipped ? " [equipped]" : "");
        if (!it.present) { all_name = all_type = all_av = false; continue; }
        if (e) {
          if (it.name != e->name) { all_name = false; std::printf("       name want '%s'\n", e->name); }
          if (it.type != e->type) { all_type = false; std::printf("       type want %s\n", item_type_key(e->type)); }
          if (it.av_mod != e->av_mod) { all_av = false; std::printf("       AVmod want %d\n", e->av_mod); }
        }
      }
      check(all_name, "all item names decode readable + match expected");
      check(all_type, "all item types decode = expected (3 shields, 2 armor, 1 two-hander, 1 general)");
      check(all_av, "all AV modifiers decode = expected (sign + magnitude)");
    }
  }

  std::printf("== D. CharacterRecord 13-slot inventory + AC aggregation ==\n");
  {
    auto party = Party::load_default(bundle);
    check(party.size() == 4, "default party 4 members");
    // 起始隊伍 13 格全空。
    bool all_empty = true;
    for (std::size_t m = 0; m < party.size(); ++m) {
      auto inv = party.at(m).inventory();
      if (inv.size() != static_cast<std::size_t>(CharacterRecord::kInventorySlots)) all_empty = false;
      for (const auto& it : inv) if (it.present) all_empty = false;
    }
    check(all_empty, "default party: all 13 inventory slots empty (per blob)");

    // 合成測試:取一名角色的 raw,注入兩件「已裝備」護甲到 slot 1/2,驗聚合 AC。
    if (party.size() > 0) {
      auto recs = party.raw_records();
      std::array<std::uint8_t, 512>& r = recs[0];
      // slot 0 @236, slot 1 @259, slot 2 @282(每格 23B)。
      // 構造一件 shield(type=0x01)AC修正+3、已裝備:
      //   bit[00]=1 equipped; bit[28-31]=3 (AC mod); bit[40-44]=0x01 (type)
      auto write_item = [&](int slot, int ac_mod, std::uint8_t type, bool equipped) {
        int base = 236 + slot * 23;
        for (int k = 0; k < 23; ++k) r[base + k] = 0;
        // bit0 equipped
        if (equipped) r[base + 0] |= 0x01;
        // bit28-31 AC mod → byte 3 (bits 28..31 = byte 3 bits 4..7)
        r[base + 3] |= static_cast<std::uint8_t>((ac_mod & 0x0F) << 4);
        // bit40-44 type → byte 5 low 5 bits
        r[base + 5] |= static_cast<std::uint8_t>(type & 0x1F);
        // 名字 "T"
        r[base + 11] = 'T';  // 單字元,高位元清除即終止
      };
      write_item(1, 3, 0x01, true);   // 已裝備盾 +3 AC
      write_item(2, 5, 0x10, true);   // 已裝備甲 +5 AC
      Party p2 = Party::from_raw_records(recs);
      const auto& c = p2.at(0);
      auto inv = c.inventory();
      check(inv[1].present && inv[1].equipped && inv[1].type == ItemType::Shield && inv[1].ac_mod == 3,
            "synthesized shield parsed (slot1 equipped, type=shield, AC+3)");
      check(inv[2].present && inv[2].type == ItemType::CuirBouilli && inv[2].ac_mod == 5,
            "synthesized armor parsed (slot2 type=cuir_bouilli, AC+5)");
      check(c.effective_ac() == 8, "effective_ac aggregates equipped AC mods (3+5=8)");
    }
  }

  std::printf("\n%s\n", g_fail == 0 ? "verify_equipment: ALL PASS" : "verify_equipment: FAIL");
  return g_fail == 0 ? 0 : 1;
}
