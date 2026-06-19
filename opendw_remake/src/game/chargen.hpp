// chargen — 新遊戲建角流程(手冊選單 B → 建立人物)。
//
// Deep module:對外只露「一份建角草稿(DraftCharacter)+ 點數配置介面 + 序列化成
// 合法 512B CharacterRecord」。內部隱藏:點數池守恆、屬性合法範圍夾制、衍生值
// (HP/STUN/PWR/AV/DV)公式、名字高位元終止編碼、fraterrisus 512B 佈局寫入。
//
// 純資料 + 規則,無 SDL/render 相依 → 可獨立進 ctest(見 tools/verify/verify_chargen.cpp)。
// SDL 建角畫面(S_CREATE 狀態、文字輸入、↑↓/+− 配點)留在 main.cpp,只呼叫本模組。
//
// ── grounded 來源 vs remake 設計 ──────────────────────────────────────────
// 記錄格式:docs/reverse-engineering/44 §1(fraterrisus 512B);名字編碼:docs/reverse-engineering/44 §4。
// 建角規則(手冊 33 第 6/7/11/12 頁):
//   - [手冊] 每名角色建立時有「50 個點數」可分配在各項特性(力量/敏捷/智力/精神…)。
//   - [手冊] 屬性畫面:A)力量 B)敏捷 C)智力 D)精神,+ 加 1 / − 減 1,Esc 離開。
//   - [手冊] STR↑→傷害↑;DEX↑→戰鬥先攻 + 命中;INT→高級咒語威力 + 命中率;
//            SPI→法力恢復 + 部分魔法效果;Power=施法消耗的法力池;
//            HP 降到一定程度死亡;STUN 高 = 耐打;性別 男/女。
//   - [docs/reverse-engineering/44 / SDA, DOS 實機] 基礎 AV = DV = DEX ÷ 4。
//   - [手冊] 建議保留部分點數(升級時可再提升)→ 允許 leftover>0 完成建角。
// remake 設計(手冊未明列確切數值處,明確標示;非 opendw byte-for-byte——
//   opendw 建角流程未逆向):
//   - 屬性起始值(配點前的內定值)= 9;合法範圍 [3, 18]+(由剩餘點數決定上限)。
//     手冊只說「系統內定各項數值供你做最初的設定」未列數字 → 取 D&D 風中庸值 9。
//   - 每 +1 屬性成本 = 1 點(手冊 Cost 欄存在但未列數值)。
//   - 衍生值:HP = 10 + STR + SPI;STUN = HP(docs/reverse-engineering/44「HP=Stun」);
//             PWR = SPI + INT/2(SPI/INT→施法/法力)。皆 cur=max。
//   - 起始技能:全 0(手冊「有些技能須先學會」;升級/事件習得)。可選擇給徒手 1。
//   - 點數池語意:50 點是「在內定起始值之上」可額外分配的預算([手冊]內定值與
//     可分配點數分開計;spent() 由起始值起算,故起始 leftover()==50)。
//     4 屬性各封頂 +9(18−9)→ 最多吸 36 點 < 50;[手冊] 本就建議保留部分點數
//     供升級再提升,故 leftover>0 為正常、可完成建角。
#pragma once

#include <array>
#include <cstdint>
#include <string>

#include "game/party.hpp"

namespace dw::game {

// ── 建角規則常數(grounded / remake 設計見檔頭)──────────────────────────
namespace chargen {
inline constexpr int kPointPool   = 50;   // [手冊] 每名角色 50 點
inline constexpr int kAttrStart   = 9;    // [remake] 屬性內定起始值
inline constexpr int kAttrMin     = 3;    // [remake] 屬性下限(不可低於起始-…;夾到 3)
inline constexpr int kAttrMax     = 18;   // [remake] 屬性上限(單項封頂)
inline constexpr int kCostPerPlus = 1;    // [remake] 每 +1 成本 1 點
inline constexpr int kNameMaxLen  = 11;   // 名字最長(record [00-11] 12B,留 1B 終止編碼餘裕)

// 四個可配點屬性的索引(配點 UI 用)。
enum Attr { kStr = 0, kDex = 1, kInt = 2, kSpi = 3, kAttrCount = 4 };
}  // namespace chargen

// DraftCharacter — 一名建角中的草稿(純資料 + 配點規則)。
//
// 不變量(invariant,save 守恆):
//   spent() + leftover() == kPointPool 恆成立;
//   每項屬性 ∈ [kAttrMin, kAttrMax]。
struct DraftCharacter {
  std::string name;                              // 角色名(ASCII;序列化時做高位元編碼)
  std::uint8_t gender = 0;                       // [78] 0=男 1=女([手冊] Sex)
  std::array<std::uint8_t, chargen::kAttrCount> attr{};  // STR/DEX/INT/SPI 當前值

  DraftCharacter() {
    for (auto& a : attr) a = chargen::kAttrStart;  // 內定起始值(remake)
  }

  // 已花用的點數 = Σ(各屬性 − 起始值)×成本。起始配置 spent=0。
  int spent() const;
  // 剩餘可分配點數 = 池 − 已花用([手冊] 允許保留 → 可 >0 完成)。
  int leftover() const;

  // 嘗試對第 i 個屬性 +1(成本足夠且未封頂才生效)。回傳是否變動。
  bool inc(int attr_index);
  // 嘗試對第 i 個屬性 −1(不可低於下限,且不可低於起始值——起始值是內定保底,
  // 退點只退回「玩家自己加上去的」)。回傳是否變動。
  bool dec(int attr_index);

  // ── 衍生值(由屬性算;cur=max)。grounded/remake 標記見檔頭 ──
  int derived_hp()   const;   // [remake] 10 + STR + SPI
  int derived_stun() const;   // [docs/reverse-engineering/44] = HP
  int derived_power()const;   // [remake] SPI + INT/2
  int base_av()      const;   // [docs/reverse-engineering/44/SDA] DEX/4
  int base_dv()      const;   // [docs/reverse-engineering/44/SDA] DEX/4

  // 名字是否合法(非空、長度 ≤ kNameMaxLen、皆可列印 ASCII)。
  bool name_valid() const;

  // 序列化成合法 512B CharacterRecord(fraterrisus 佈局)。
  //   名字高位元終止編碼;屬性 cur=max;HP/STUN/PWR 衍生(cur=max,2B LE);
  //   skills/spells/inventory 全 0;status=0;gender;level=1;xp=0;gold=0;
  //   AV/DV/AC stored=0(對齊起始隊伍:runtime 由 effective_*() 算 DEX/4)。
  // 確定性:同 DraftCharacter → 同 512B(無 RNG)。
  std::array<std::uint8_t, 512> serialize() const;
};

}  // namespace dw::game
