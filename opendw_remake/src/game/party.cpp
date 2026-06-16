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
// 把完整 23B 解析的 ItemInstance 投影成戰鬥精簡視圖 EquipItem(回歸相容)。
EquipItem to_equip_item(const ItemInstance& it) {
  EquipItem e;
  e.present = it.present;
  e.equipped = it.equipped;
  e.av_mod = it.av_mod;
  e.ac_mod = it.ac_mod;
  e.item_type = static_cast<std::uint8_t>(it.type);
  e.primary_dmg = it.primary_dmg;
  return e;
}
}  // namespace

ItemInstance CharacterRecord::item_at(int slot) const {
  if (slot < 0 || slot >= kInventorySlots) return ItemInstance{};
  const int base = kInventoryBase + slot * kItemStride;
  if (base + 11 > 512) return ItemInstance{};
  return parse_item(raw.data() + base, 512 - static_cast<std::size_t>(base));
}

std::array<ItemInstance, CharacterRecord::kInventorySlots>
CharacterRecord::inventory() const {
  std::array<ItemInstance, kInventorySlots> out{};
  for (int s = 0; s < kInventorySlots; ++s) out[s] = item_at(s);
  return out;
}

EquipItem CharacterRecord::main_weapon() const {
  // 取第一件「已裝備且為武器」的物品;無則回退第 0 格(維持既有行為:起始
  // 隊伍第 0 格空 → present=false → 戰鬥回退徒手)。
  for (int s = 0; s < kInventorySlots; ++s) {
    ItemInstance it = item_at(s);
    if (it.present && it.equipped && is_weapon(it.type))
      return to_equip_item(it);
  }
  return to_equip_item(item_at(0));
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
  // 只有「已裝備的武器」才貢獻 AV 修正與武器技能加成。main_weapon() 在無已裝備
  // 武器時會回退第 0 格的精簡視圖(可能是未裝備物品)→ 須以 w.equipped 過濾,
  // 否則背包裡未裝備的武器也會錯誤地加成(裝備穿脫 UI 上線後此 bug 會顯形)。
  bool has_equipped_weapon = w.present && w.equipped;
  int skill_bonus = 0;
  if (has_equipped_weapon) {
    int si = weapon_skill_index(w.item_type);
    if (si >= 0) skill_bonus = skills[si];
  }
  if (av > 0) return static_cast<int>(av) + skill_bonus;
  int base = dexterity / 4;
  return base + (has_equipped_weapon ? w.av_mod : 0) + skill_bonus;
}

int CharacterRecord::effective_dv() const {
  // SDA:base DV = DEX/4(與 AV 同源)。stored dv>0 採用。
  if (dv > 0) return static_cast<int>(dv);
  return dexterity / 4;
}

int CharacterRecord::effective_ac() const {
  // stored ac>0 採用(原版 runtime 計算);否則累加「所有已裝備物品」的 AC 修正
  // (盾/全身盾/各甲/頭盔/有 AC 修正的物品皆計入)。起始隊伍無裝備 → 0。
  if (ac > 0) return static_cast<int>(ac);
  int total = 0;
  for (int s = 0; s < kInventorySlots; ++s) {
    ItemInstance it = item_at(s);
    if (it.present && it.equipped) total += it.ac_mod;
  }
  return total;
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
  r.level = p[0x4F];           // [79] 1 byte(docs/44 §1:level=[79]、XP=[80] 各 1B)。
  // ↑ 原以 rd16(0x4F) 讀 2B,會把 [80] XP 併入高位元組;XP 一旦非 0(戰鬥 +80)
  //   Level 顯示即被污染。docs/44 明列 level=[79] 1B、XP=[80] 1B → 改讀單 byte,
  //   與 chargen.serialize(只寫 raw[0x4F]=level 1B)一致;回歸:起始隊伍/建角 level
  //   高位元組恆 0,單 byte 與 rd16 結果相同。
  r.xp = p[80];                // [80]
  r.gold8 = p[81];             // [81](fraterrisus gold;見 hpp 註)
  r.av = p[82]; r.dv = p[83]; r.ac = p[84]; r.flags = p[85];
  r.gold = static_cast<std::uint32_t>(p[0x55] | (p[0x56] << 8) |
                                      (p[0x57] << 16) | (p[0x58] << 24));
  return r;
}

void Party::award_xp(int per_member) {
  if (per_member <= 0) return;
  for (auto& m : members_) {
    int v = static_cast<int>(m.xp) + per_member;
    if (v > 255) v = 255;  // [80] 為 1 byte;飽和夾頂(存檔 round-trip 一致)
    m.xp = static_cast<std::uint8_t>(v);
    m.raw[80] = m.xp;      // 同步原始 record(raw_records() 供存檔)
  }
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

void Party::add_record(const std::array<std::uint8_t, 512>& rec) {
  members_.push_back(parse_record(rec.data()));
}

// ── 次要指令:刪除 / 改名 / 重排 / 物品轉移 / 丟棄(手冊明列)────────────────

// 名字寫進 raw[0..11]:高位元終止編碼(與 chargen.cpp write_name 同規則)。
namespace {
void write_name_into(std::array<std::uint8_t, 512>& r, const std::string& name) {
  int n = static_cast<int>(name.size());
  if (n > 12) n = 12;
  for (int i = 0; i < 12; ++i) r[i] = 0;             // 先清名欄
  for (int i = 0; i < n; ++i) {
    std::uint8_t b = static_cast<std::uint8_t>(name[i] & 0x7F);
    if (i + 1 < n) b |= 0x80;                         // 非末字元設高位元
    r[i] = b;                                         // 末字元高位元為 0 → 終止
  }
}
}  // namespace

bool Party::remove(std::size_t i) {
  if (i >= members_.size()) return false;
  members_.erase(members_.begin() + static_cast<std::ptrdiff_t>(i));
  return true;
}

bool Party::rename(std::size_t i, const std::string& name) {
  if (i >= members_.size() || name.empty()) return false;
  auto& m = members_[i];
  write_name_into(m.raw, name);
  m.name = read_name(m.raw.data());   // 從 raw 回讀 → 與存檔內容一致
  return true;
}

bool Party::move(std::size_t from, std::size_t to) {
  std::size_t n = members_.size();
  if (from >= n || to >= n || from == to) return false;
  CharacterRecord moved = members_[from];
  members_.erase(members_.begin() + static_cast<std::ptrdiff_t>(from));
  members_.insert(members_.begin() + static_cast<std::ptrdiff_t>(to), moved);
  return true;
}

// 第 slot 格的 raw 起始 offset;格超出 512B record 容納範圍時回 -1(slot 12 不適用,
// 與 item_at 的 base+11>512 守門一致:13 格中實際只有 0..11 落在 record 內)。
namespace {
int slot_base(int slot) {
  if (slot < 0 || slot >= CharacterRecord::kInventorySlots) return -1;
  int base = CharacterRecord::kInventoryBase + slot * CharacterRecord::kItemStride;
  if (base + CharacterRecord::kItemStride > 512) return -1;
  return base;
}
}  // namespace

bool Party::discard_item(std::size_t owner, int slot) {
  if (owner >= members_.size()) return false;
  int base = slot_base(slot);
  if (base < 0) return false;
  auto& m = members_[owner];
  if (!m.item_at(slot).present) return false;
  for (int b = 0; b < CharacterRecord::kItemStride; ++b) m.raw[base + b] = 0;
  return true;
}

bool Party::transfer_item(std::size_t from, int slot, std::size_t to) {
  if (from >= members_.size() || to >= members_.size() || from == to) return false;
  int sb = slot_base(slot);
  if (sb < 0) return false;
  auto& src = members_[from];
  auto& dst = members_[to];
  if (!src.item_at(slot).present) return false;
  // 目標第一個「在 record 容納範圍內且為空」的格。
  int db = -1;
  for (int s = 0; s < CharacterRecord::kInventorySlots; ++s) {
    int b = slot_base(s);
    if (b >= 0 && !dst.item_at(s).present) { db = b; break; }
  }
  if (db < 0) return false;   // 目標背包已滿
  for (int b = 0; b < CharacterRecord::kItemStride; ++b) {
    dst.raw[db + b] = src.raw[sb + b];   // 整 23B 搬移(含裝備位元 / 名)
    src.raw[sb + b] = 0;                 // 清來源格
  }
  return true;
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
