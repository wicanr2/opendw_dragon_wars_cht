// party — 隊伍角色資料 + 右側狀態面板渲染。
//
// Deep module:對外只露「載入預設 4 人隊伍」與「把面板畫進畫面」兩個窄介面;
// 內部隱藏 opendw player_record 512-byte 佈局、名字高位元終止編碼、
// 以及原版 draw_player_status_panel / draw_player_stat 的 VGA 座標移植。
//
// 來源對照(opendw,唯讀 oracle):
//   - player.c struct player_record(欄位 offset 見下)
//   - engine.c draw_player_status_panel(0x1A72)/draw_player_status(0x1ABD)/
//     draw_player_stat(0x1C0F? 實作於 6599)/write_character_name(0x1A40)
//   - game_state:gs[0x0A+i]=角色 record 選擇子(=record_index*2)、
//     gs[0x18+i]=在隊旗標(0=在)、gs[0x1F]=隊伍人數
//
// 像素層 vs 文字層分工(遵循專案雙層渲染規則):
//   - 角色列底框 / 三條 HP/暈眩/法力 狀態條 → 走 Framebuffer(像素層)
//   - 角色名字 → 走 TextLayer(TTF,可 i18n;角色名通常英文)
#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "../render/framebuffer.hpp"
#include "../render/text_layer.hpp"

namespace dw::game {

// opendw player_record 關鍵欄位 offset(player.c)。
struct CharacterRecord {
  std::string name;          // name[0..]:除末位元組外皆設高位元,末位元組高位元清除
  std::uint8_t strength = 0, max_strength = 0;       // 0x0C,0x0D
  std::uint8_t dexterity = 0, max_dexterity = 0;     // 0x0E,0x0F
  std::uint8_t intel = 0, max_intel = 0;             // 0x10,0x11
  std::uint8_t spirit = 0, max_spirit = 0;           // 0x12,0x13
  std::uint16_t health = 0, max_health = 0;          // 0x14,0x16
  std::uint16_t stun = 0, max_stun = 0;              // 0x18,0x1A
  std::uint16_t power = 0, max_power = 0;             // 0x1C,0x1E
  std::uint8_t status = 0;   // 0x4C bitfield:0x01 dead / 0x02 chained / 0x04 poisoned / 0x80 stunned
  std::uint8_t gender = 0;   // 0x4E
  std::uint16_t level = 0;   // 0x4F
  std::uint32_t gold = 0;    // 0x55
  std::array<std::uint8_t, 512> raw{};  // 完整原始 record(供未來欄位擴充)
};

class Party {
public:
  // 從 bundle 載入預設隊伍(assets/bundle/party/default_party.bin,自包含,
  // 抽自 DATA1 @ 0x2E26 起 4×512B:Muskels / Theb / Elendil / Cheetah)。
  // 失敗回傳空隊伍(size()==0)。
  static Party load_default(const std::filesystem::path& bundle_dir);

  // 從原始位元組(N×512)建立(供測試)。
  static Party from_records(const std::vector<std::uint8_t>& bytes);

  // 從一組 512B 原始 record 建立(供讀檔還原;不過濾空槽 → 與存檔內容一一對應)。
  static Party from_raw_records(
      const std::vector<std::array<std::uint8_t, 512>>& records);

  // 取每名角色的完整 512B 原始 record(供存檔;與 at(i) 同順序)。
  std::vector<std::array<std::uint8_t, 512>> raw_records() const;

  std::size_t size() const { return members_.size(); }
  const CharacterRecord& at(std::size_t i) const { return members_.at(i); }

  // status bitfield → 英文狀態鍵(供 i18n tr() 在地化);正常回 "normal"。
  // 檢查順序與 opendw 一致(dead > stunned > poisoned > chained)。
  static const char* status_key(std::uint8_t status);

  // 在畫面右側畫隊伍狀態面板(對拍 opendw draw_player_status_panel)。
  //   - 像素層:每名角色一列底框 + HP(亮紅)/暈眩(亮綠)/法力(亮藍)狀態條
  //   - 文字層:角色名字(置中於列頂),異常狀態以文字標示("is dead" 等)
  // name_px:名字字級(視窗 px),0 用預設。
  void draw_status_panel(render::Framebuffer& fb, render::TextLayer& tl,
                         int name_px = 0) const;

private:
  std::vector<CharacterRecord> members_;
  static CharacterRecord parse_record(const std::uint8_t* p);
};

}  // namespace dw::game
