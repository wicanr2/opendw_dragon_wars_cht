// party_panel — 隊伍右側狀態面板渲染(像素條 + 文字層名字)。
//
// 從 party.cpp 拆出:此 TU 相依 render::TextLayer(SDL2_ttf),故與「純資料/載入」
// 的 party.cpp 分離,讓不需渲染的測試(verify_combat 等)可只連 party.cpp.o,
// 不被 SDL 連結需求污染(Deep Modules:render 邊界內聚於此檔)。
//
// 對照 opendw:draw_player_status_panel(0x1A72)/draw_player_status(0x1ABD)/
//   draw_player_stat(6599)。
#include "party.hpp"

#include <cstdio>

namespace dw::game {

namespace {

// 狀態條色(對照 opendw color_data[] = {00,FF,CC,AA,99} & 0x0F)。
constexpr std::uint8_t kBarColorHp    = 0x0C;  // 亮紅
constexpr std::uint8_t kBarColorStun  = 0x0A;  // 亮綠
constexpr std::uint8_t kBarColorPower = 0x09;  // 亮藍
constexpr std::uint8_t kBgColor       = 0x00;  // 背景黑

constexpr int kRowStride = 0x10;      // 16 掃描線/角色
constexpr int kRowTop    = 0x20;      // 第一列頂(y=32)
constexpr int kBarX0px   = 0x36 * 4;  // 216
constexpr int kBarX1px   = 0x4E * 4;  // 312
constexpr int kBarUnitPx = 4;         // 每條長度單位 = 4px
constexpr int kNameXpx   = 0x1B * 8;  // x<<3, x=0x1B → 216

// 狀態 bitmask → 文字(檢查順序與 opendw 一致 si 3→0)。
const char* status_text(std::uint8_t st) {
  if (st & 0x01) return "dead";
  if (st & 0x80) return "stunned";
  if (st & 0x04) return "poisoned";
  if (st & 0x02) return "chained";
  return nullptr;
}

void hline(render::Framebuffer& fb, int x0px, int x1px, int y,
           std::uint8_t color) {
  for (int x = x0px; x < x1px; ++x) fb.put(x, y, color);
}

// 對照 draw_player_stat(6599):畫一條狀態條(亮色比例 + 黑色剩餘),雙掃描線。
void draw_stat_bar(render::Framebuffer& fb, int y, std::uint16_t cur,
                   std::uint16_t max_val, std::uint8_t bar_color) {
  int filled_units = 0;
  if (cur != 0 && max_val != 0) {
    filled_units = (cur * 23) / max_val + 1;
  } else {
    filled_units = cur ? cur : 0;
  }
  int split_px = kBarX0px + filled_units * kBarUnitPx;
  if (split_px > kBarX1px) split_px = kBarX1px;
  hline(fb, kBarX0px, split_px, y, bar_color);
  hline(fb, kBarX0px, split_px, y + 1, bar_color);
  if (split_px < kBarX1px) {
    hline(fb, split_px, kBarX1px, y, kBgColor);
    hline(fb, split_px, kBarX1px, y + 1, kBgColor);
  }
}

}  // namespace

void Party::draw_status_panel(render::Framebuffer& fb, render::TextLayer& tl,
                              int name_px) const {
  for (std::size_t i = 0; i < members_.size() && i < 7; ++i) {
    const CharacterRecord& c = members_.at(i);
    int row_top = static_cast<int>(i) * kRowStride + kRowTop;

    tl.add(kNameXpx, row_top, c.name.empty() ? "?" : c.name, 15, name_px);

    const char* st = status_text(c.status);
    if (st) {
      char buf[48];
      std::snprintf(buf, sizeof buf, "is %s", st);
      tl.add(kNameXpx, row_top + 8, buf, 12, name_px);
      continue;
    }

    draw_stat_bar(fb, row_top + 0x08, c.health, c.max_health, kBarColorHp);
    draw_stat_bar(fb, row_top + 0x0B, c.stun, c.max_stun, kBarColorStun);
    draw_stat_bar(fb, row_top + 0x0E, c.power, c.max_power, kBarColorPower);
  }
}

}  // namespace dw::game
