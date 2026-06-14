// damage_dice — 武器傷害骰編碼(fraterrisus 1-byte 編碼)。
//
// 自成一個小 header,讓 equipment / party / combat 共用同一份 DamageDice 定義
// 而不互相 include。來源:docs/44_DATA_FORMATS_AND_MECHANICS.md §2 傷害骰編碼。
//   高 3 bit = 骰面:000=d4 001=d6 010=d8 011=d10 100=d12 101=d20 110=d30 111=d100
//   低 3 bit = 骰數 − 1(000=1 顆 … 111=8 顆)。中間 2 bit 恆 0。
//   範例:0b101_00_001 = 2d20。
// raw==0 視為「無傷害骰」(空欄/無武器)。
#pragma once

#include <cstdint>

namespace dw::game {

struct DamageDice {
  int count = 0;   // 骰數;0 = 無
  int sides = 0;   // 骰面;0 = 無
  bool valid() const { return count > 0 && sides > 0; }
};

DamageDice decode_damage_dice(std::uint8_t encoded);

}  // namespace dw::game
