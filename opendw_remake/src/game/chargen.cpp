// chargen — 實作。見 chargen.hpp 檔頭(grounded 來源 vs remake 設計)。
#include "game/chargen.hpp"

#include <algorithm>
#include <cctype>

namespace dw::game {

using namespace chargen;

int DraftCharacter::spent() const {
  int s = 0;
  for (auto a : attr) s += (static_cast<int>(a) - kAttrStart) * kCostPerPlus;
  return s;
}

int DraftCharacter::leftover() const { return kPointPool - spent(); }

bool DraftCharacter::inc(int i) {
  if (i < 0 || i >= kAttrCount) return false;
  if (attr[i] >= kAttrMax) return false;          // 單項封頂
  if (leftover() < kCostPerPlus) return false;    // 點數不足
  ++attr[i];
  return true;
}

bool DraftCharacter::dec(int i) {
  if (i < 0 || i >= kAttrCount) return false;
  // 退點只退回玩家自己加上去的(不可低於內定起始值,也不可低於硬下限)。
  if (attr[i] <= kAttrStart || attr[i] <= kAttrMin) return false;
  --attr[i];
  return true;
}

// ── 衍生值(remake 設計 / docs/44;見檔頭)──
int DraftCharacter::derived_hp() const {
  return 10 + static_cast<int>(attr[kStr]) + static_cast<int>(attr[kSpi]);
}
int DraftCharacter::derived_stun() const { return derived_hp(); }  // docs/44:HP=Stun
int DraftCharacter::derived_power() const {
  return static_cast<int>(attr[kSpi]) + static_cast<int>(attr[kInt]) / 2;
}
int DraftCharacter::base_av() const { return static_cast<int>(attr[kDex]) / 4; }
int DraftCharacter::base_dv() const { return static_cast<int>(attr[kDex]) / 4; }

bool DraftCharacter::name_valid() const {
  if (name.empty() || static_cast<int>(name.size()) > kNameMaxLen) return false;
  for (char c : name)
    if (c < 0x20 || static_cast<unsigned char>(c) >= 0x7F) return false;
  return true;
}

namespace {
// 16-bit little-endian 寫入。
void wr16(std::array<std::uint8_t, 512>& r, int off, int v) {
  r[off] = static_cast<std::uint8_t>(v & 0xFF);
  r[off + 1] = static_cast<std::uint8_t>((v >> 8) & 0xFF);
}
// 名字高位元終止編碼(docs/44 §4 / 對拍 party.cpp read_name):
//   每字元 7-bit ASCII,除最後一字元外皆設高位元;最後一字元高位元清除。
//   寫進 [00..11](12B);名後填 0(不設高位元 → read_name 在名末即止)。
void write_name(std::array<std::uint8_t, 512>& r, const std::string& name) {
  int n = std::min<int>(static_cast<int>(name.size()), kNameMaxLen);
  for (int i = 0; i < 12; ++i) r[i] = 0;          // 先清空名欄
  for (int i = 0; i < n; ++i) {
    std::uint8_t b = static_cast<std::uint8_t>(name[i] & 0x7F);
    if (i + 1 < n) b |= 0x80;                      // 非末字元 → 設高位元
    r[i] = b;                                       // 末字元高位元為 0 → 終止
  }
}
}  // namespace

std::array<std::uint8_t, 512> DraftCharacter::serialize() const {
  std::array<std::uint8_t, 512> r{};               // 全 0 初始(空 skills/spells/inventory)

  write_name(r, name);

  // 屬性 cur=max(各 1B)。[12-19]
  r[0x0C] = attr[kStr]; r[0x0D] = attr[kStr];
  r[0x0E] = attr[kDex]; r[0x0F] = attr[kDex];
  r[0x10] = attr[kInt]; r[0x11] = attr[kInt];
  r[0x12] = attr[kSpi]; r[0x13] = attr[kSpi];

  // HP/STUN/PWR 衍生(cur=max,各 2B LE)。[20-31]
  int hp = derived_hp(), st = derived_stun(), pw = derived_power();
  wr16(r, 0x14, hp); wr16(r, 0x16, hp);            // Health cur,max
  wr16(r, 0x18, st); wr16(r, 0x1A, st);            // Stun cur,max
  wr16(r, 0x1C, pw); wr16(r, 0x1E, pw);            // Power cur,max

  // [32-58] skills 全 0;[59] AP=0;[60-67] spells 全 0(已由 {} 初始)。
  // [76] status=0;[78] gender;[79] level=1;[80] xp=0;[81] gold=0。
  r[0x4C] = 0;                                      // status
  r[0x4E] = gender;                                 // gender
  r[0x4F] = 1;                                      // level=1(起始)
  r[0x50] = 0;                                      // xp
  r[0x51] = 0;                                      // gold (fraterrisus [81])
  // [82] AV / [83] DV / [84] AC stored=0 → 對齊起始隊伍:runtime 由
  // CharacterRecord::effective_av/dv/ac 算(base = DEX/4)。
  r[0x52] = 0; r[0x53] = 0; r[0x54] = 0;
  return r;
}

}  // namespace dw::game
