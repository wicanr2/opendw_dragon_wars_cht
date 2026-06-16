// recruit — 酒館招募 NPC 實作。對齊依據見 recruit.hpp 檔頭(docs/44 §1 + fraterrisus)。
#include "game/recruit.hpp"

#include <cstring>

namespace dw::game {

namespace {

// 名字寫進 record [00-11]:高位元終止編碼(除末字元外皆 OR 0x80;末字元高位元清除)。
// 與 chargen.serialize / party.cpp read_name 同規則。
void write_name(std::array<std::uint8_t, 512>& rec, const std::string& name) {
  int n = static_cast<int>(name.size());
  if (n > 11) n = 11;  // record [00-11] 12B,留終止編碼餘裕
  for (int i = 0; i < n; ++i) {
    std::uint8_t b = static_cast<std::uint8_t>(name[(std::size_t)i] & 0x7F);
    if (i < n - 1) b |= 0x80;  // 非末字元設高位元
    rec[(std::size_t)i] = b;
  }
  // 末字元已是清高位元;若名為空,留 0。
}

void wr16(std::array<std::uint8_t, 512>& rec, int off, std::uint16_t v) {
  rec[(std::size_t)off] = static_cast<std::uint8_t>(v & 0xFF);
  rec[(std::size_t)off + 1] = static_cast<std::uint8_t>(v >> 8);
}

}  // namespace

std::array<std::uint8_t, 512> NpcTemplate::serialize() const {
  std::array<std::uint8_t, 512> rec{};
  write_name(rec, name);
  // 屬性(cur=max,各 1B;fraterrisus offset)。
  rec[0x0C] = strength;  rec[0x0D] = strength;
  rec[0x0E] = dexterity; rec[0x0F] = dexterity;
  rec[0x10] = intel;     rec[0x11] = intel;
  rec[0x12] = spirit;    rec[0x13] = spirit;
  // HP/STUN/PWR(cur=max,各 2B LE)。
  wr16(rec, 0x14, health); wr16(rec, 0x16, health);
  wr16(rec, 0x18, stun);   wr16(rec, 0x1A, stun);
  wr16(rec, 0x1C, power);  wr16(rec, 0x1E, power);
  // status[76]=0、identifier[77]、gender[78]、level[79]、xp[80]=0、gold[81]=0。
  rec[0x4C] = 0;
  rec[77]   = identifier;   // [77] NPC 識別碼(grounded)
  rec[78]   = gender;
  rec[0x4F] = level;
  rec[80]   = 0;
  rec[81]   = 0;
  // AV/DV/AC stored=0(對齊起始隊伍:runtime 由 effective_*() 算 DEX/4)。
  return rec;
}

const std::vector<NpcTemplate>& RecruitRoster::roster() {
  // 名字 grounded(fraterrisus NPC identifier);屬性為 curated 平衡值(remake 設計)。
  // 大致定位:Ulrik 戰士、Louie 盜賊/敏捷、Valar 法師、Halifax 牧師/精神。
  static const std::vector<NpcTemplate> kRoster = [] {
    std::vector<NpcTemplate> v;
    // identifier, name, STR, DEX, INT, SPI, HP, STUN, PWR, level, gender
    v.push_back({0x01, "Ulrik",   18, 16, 10, 12, 28, 28,  8, 2, 0});  // 戰士型
    v.push_back({0x03, "Louie",   13, 20, 13, 11, 22, 22, 10, 2, 0});  // 敏捷/盜賊型
    v.push_back({0x04, "Valar",   10, 14, 19, 16, 18, 18, 24, 2, 0});  // 法師型(高 INT/PWR)
    v.push_back({0x05, "Halifax", 12, 13, 14, 19, 20, 20, 20, 2, 0});  // 牧師型(高 SPI)
    return v;
  }();
  return kRoster;
}

const NpcTemplate* RecruitRoster::find(std::uint8_t identifier) {
  for (const auto& t : roster())
    if (t.identifier == identifier) return &t;
  return nullptr;
}

bool party_has_npc(const Party& party, std::uint8_t identifier) {
  if (identifier == 0) return false;  // 0 = 非 NPC(玩家自建角色)
  for (std::size_t i = 0; i < party.size(); ++i)
    if (party.at(i).raw[77] == identifier) return true;
  return false;
}

RecruitResult recruit_npc(Party& party, std::uint8_t identifier) {
  RecruitResult r;
  r.party_size_after = static_cast<int>(party.size());
  const NpcTemplate* t = RecruitRoster::find(identifier);
  if (!t) { r.reason = "recruit_unknown"; return r; }
  if (party_has_npc(party, identifier)) { r.reason = "recruit_already"; return r; }
  if (static_cast<int>(party.size()) >= kMaxPartyMembers) {
    r.reason = "recruit_full";
    return r;
  }
  party.add_record(t->serialize());
  r.ok = true;
  r.party_size_after = static_cast<int>(party.size());
  return r;
}

}  // namespace dw::game
