#include "party.hpp"

#include <cstdio>
#include <cstring>

namespace dw::game {

namespace {

std::uint16_t rd16(const std::uint8_t* p, int off) {
  return static_cast<std::uint16_t>(p[off] | (p[off + 1] << 8));
}

// 角色名:從 record 起逐位元組讀,直到遇到「高位元已清除」的位元組(含)為止。
// (opendw write_character_name 0x1A40:每位元組 OR 0x80 後輸出,末位元組高位元為 0 即止。)
std::string read_name(const std::uint8_t* p) {
  std::string s;
  for (int i = 0; i < 16; ++i) {
    std::uint8_t b = p[i];
    char c = static_cast<char>(b & 0x7F);
    if (c >= 0x20 && c < 0x7F) s.push_back(c);
    if ((b & 0x80) == 0) break;
  }
  return s;
}

// 狀態 bitmask → 文字(對照 unknown_1BC1[0..3]={02,04,80,01} 與 str_table_status)。
// 檢查順序與 opendw 一致(si 3→0),回傳第一個命中的狀態字串;無則回 nullptr。
// 註:狀態條/名字渲染已拆到 party_panel.cpp(相依 SDL),此 TU 保持純資料。
const char* status_text(std::uint8_t st) {
  if (st & 0x01) return "dead";       // si=3
  if (st & 0x80) return "stunned";    // si=2
  if (st & 0x04) return "poisoned";   // si=1
  if (st & 0x02) return "chained";    // si=0
  return nullptr;
}

}  // namespace

const char* Party::status_key(std::uint8_t status) {
  const char* st = status_text(status);
  return st ? st : "normal";
}

// ── 傷害骰解碼(fraterrisus)──────────────────────────────────────────────
DamageDice decode_damage_dice(std::uint8_t encoded) {
  DamageDice d;
  if (encoded == 0) return d;  // 0 = 無傷害骰
  // 高 3 bit = 骰面 index → 面數。
  static const int kSides[8] = {4, 6, 8, 10, 12, 20, 30, 100};
  int sides_idx = (encoded >> 5) & 0x07;
  int count = (encoded & 0x07) + 1;  // 低 3 bit = 骰數 − 1
  d.sides = kSides[sides_idx];
  d.count = count;
  return d;
}

namespace {
// 從 record raw 取物品欄第 slot 格(每格 23B,首格 @ [236])的 EquipItem。
// fraterrisus 11-byte bit-packed:bit offset 從該格起。LSB-first。
EquipItem parse_equip(const std::uint8_t* rec, int slot) {
  EquipItem e;
  const int base = 236 + slot * 23;
  if (base + 11 > 512) return e;
  const std::uint8_t* q = rec + base;
  // 全 0(無物品)→ present=false。
  bool any = false;
  for (int i = 0; i < 11; ++i) if (q[i]) { any = true; break; }
  if (!any) return e;
  e.present = true;
  // bit helper:取 [lo,hi] inclusive bit 區段(LSB-first across bytes)。
  auto bits = [&](int lo, int hi) -> std::uint32_t {
    std::uint32_t v = 0;
    for (int b = lo; b <= hi; ++b) {
      int byte = b >> 3, bit = b & 7;
      if (q[byte] & (1u << bit)) v |= (1u << (b - lo));
    }
    return v;
  };
  e.equipped = bits(0, 0) != 0;
  bool av_neg = bits(8, 8) != 0;
  int av = static_cast<int>(bits(24, 27));
  e.av_mod = av_neg ? -av : av;
  e.ac_mod = static_cast<int>(bits(28, 31));
  e.item_type = static_cast<std::uint8_t>(bits(40, 44));  // 低 5 bit
  e.primary_dmg = decode_damage_dice(static_cast<std::uint8_t>(bits(64, 71)));
  return e;
}
}  // namespace

EquipItem CharacterRecord::main_weapon() const {
  return parse_equip(raw.data(), 0);
}

// 武器類型 → 對應武器技能 index;非武器(或一般)回 -1。
static int weapon_skill_index(std::uint8_t item_type) {
  switch (item_type) {
    case 0x03: return kSkillAxe;
    case 0x04: return kSkillFlail;
    case 0x05: return kSkillSword;
    case 0x06: return kSkillTwoHand;
    case 0x07: return kSkillMace;       // 錘
    case 0x08: return kSkillBow;
    case 0x09: return kSkillCrossbow;
    case 0x0A: return kSkillCrossbow;   // 槍(以弩技能近似;docs 未細分)
    case 0x0B: return kSkillThrown;
    default:   return -1;
  }
}

int CharacterRecord::effective_av() const {
  // SDA:base AV = DEX/4;最終 AV =(base ± 武器 AV 修正)+ 武器技能(1:1 隱形)。
  // stored av(>0)代表原版已 runtime 算好 → 直接採用(起始隊伍為 0,走計算路徑)。
  EquipItem w = main_weapon();
  int skill_bonus = 0;
  if (w.present) {
    int si = weapon_skill_index(w.item_type);
    if (si >= 0) skill_bonus = skills[si];
  }
  if (av > 0) return static_cast<int>(av) + skill_bonus;
  int base = dexterity / 4;
  return base + (w.present ? w.av_mod : 0) + skill_bonus;
}

int CharacterRecord::effective_dv() const {
  // SDA:base DV = DEX/4(與 AV 同源)。stored dv>0 採用。
  if (dv > 0) return static_cast<int>(dv);
  return dexterity / 4;
}

int CharacterRecord::effective_ac() const {
  // stored ac>0 採用;否則由裝備 AC 修正累加(起始隊伍無裝備 → 0)。
  if (ac > 0) return static_cast<int>(ac);
  EquipItem w = main_weapon();
  return w.present ? w.ac_mod : 0;
}

CharacterRecord Party::parse_record(const std::uint8_t* p) {
  CharacterRecord r;
  std::memcpy(r.raw.data(), p, 512);
  r.name = read_name(p);
  r.strength = p[0x0C]; r.max_strength = p[0x0D];
  r.dexterity = p[0x0E]; r.max_dexterity = p[0x0F];
  r.intel = p[0x10]; r.max_intel = p[0x11];
  r.spirit = p[0x12]; r.max_spirit = p[0x13];
  r.health = rd16(p, 0x14); r.max_health = rd16(p, 0x16);
  r.stun = rd16(p, 0x18); r.max_stun = rd16(p, 0x1A);
  r.power = rd16(p, 0x1C); r.max_power = rd16(p, 0x1E);
  // skills[32-58] / spells[60-67](fraterrisus）
  std::memcpy(r.skills.data(), p + 32, 27);
  std::memcpy(r.spells.data(), p + 60, 8);
  r.status = p[0x4C];          // [76]
  r.gender = p[0x4E];          // [78]
  r.level = rd16(p, 0x4F);     // [79]
  r.xp = p[80];                // [80]
  r.gold8 = p[81];             // [81](fraterrisus gold;見 hpp 註)
  r.av = p[82]; r.dv = p[83]; r.ac = p[84]; r.flags = p[85];
  r.gold = static_cast<std::uint32_t>(p[0x55] | (p[0x56] << 8) |
                                      (p[0x57] << 16) | (p[0x58] << 24));
  return r;
}

Party Party::from_records(const std::vector<std::uint8_t>& bytes) {
  Party party;
  std::size_t n = bytes.size() / 512;
  for (std::size_t i = 0; i < n; ++i) {
    const std::uint8_t* p = bytes.data() + i * 512;
    // 空槽(record 起為 0x00 終止 + 0xFF 填充)視為無此角色 → 不加入隊伍。
    if (p[0] == 0x00 || p[0] == 0xFF) continue;
    party.members_.push_back(parse_record(p));
  }
  return party;
}

Party Party::from_raw_records(
    const std::vector<std::array<std::uint8_t, 512>>& records) {
  Party party;
  // 讀檔還原:逐筆解析,不過濾空槽 — 存什麼、讀回什麼,順序與內容一一對應,
  // 確保存→讀→存 byte-for-byte 一致。
  for (const auto& rec : records)
    party.members_.push_back(parse_record(rec.data()));
  return party;
}

std::vector<std::array<std::uint8_t, 512>> Party::raw_records() const {
  std::vector<std::array<std::uint8_t, 512>> out;
  out.reserve(members_.size());
  for (const auto& m : members_) out.push_back(m.raw);
  return out;
}

Party Party::load_default(const std::filesystem::path& bundle_dir) {
  std::filesystem::path p = bundle_dir / "party" / "default_party.bin";
  std::FILE* f = std::fopen(p.string().c_str(), "rb");
  if (!f) {
    std::fprintf(stderr, "party: open failed: %s\n", p.string().c_str());
    return Party{};
  }
  std::fseek(f, 0, SEEK_END);
  long sz = std::ftell(f);
  std::fseek(f, 0, SEEK_SET);
  std::vector<std::uint8_t> buf(sz > 0 ? static_cast<std::size_t>(sz) : 0);
  if (!buf.empty() && std::fread(buf.data(), 1, buf.size(), f) != buf.size()) {
    std::fclose(f);
    std::fprintf(stderr, "party: short read: %s\n", p.string().c_str());
    return Party{};
  }
  std::fclose(f);
  return from_records(buf);
}

// Party::draw_status_panel 已移至 party_panel.cpp(相依 render::TextLayer / SDL)。

}  // namespace dw::game
