// ui_pieces — 遊戲內 UI chrome(石磚邊框 + Dragon Wars logo + pillar)的原版資源層。
//
// 忠實 port 自 opendw src/lib/ui.c:
//   draw_ui_piece() @546:把一片 nibble bitmap blit 進 framebuffer。
//     x 起點 = offset_delta*4,y 起點 = y_pos,每 byte = 2 像素(hi=左,lo=右)。
//   ui_draw()       @744:counter 0..8 直接 draw_ui_piece(pieces[counter]);
//                          counter 9 = 右 pillar(UI_RIGHT_PILLAR);其餘為功能(viewport 等)。
//   ui_header_draw() @762:頂端石磚邊框 + 標題 logo 區。pieces[0x17 + i]。
//   draw_right_pillar() @720:draw_ui_piece(pieces[UI_RIGHT_PILLAR=9])。
//
// 資料自包含於 bundle:viewport/ui_pieces.bin(由 tools/extract/extract_ui_pieces 萃取,
// byte-for-byte 同 DRAGON.COM v1.1 com 0x6AE0)。執行期不依賴 DRAGON.COM。
//
// 與 viewport 解碼互不干擾:本層只畫 viewport(160×136 @ (16,8))外圍 + 右側面板區,
// 不碰 viewport_memory(render_sweep 154 case 鎖定的像素)。
#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <vector>

#include "render/framebuffer.hpp"

namespace dw::render {

class UiPieces {
public:
  static constexpr int kCount = 0x2B;               // UI_PIECE_COUNT = 43
  static constexpr int kRightPillar = 9;            // UI_RIGHT_PILLAR
  static constexpr int kBrickFirstPicture = 0x17;   // UI_BRICK_FIRST_PICTURE

  struct Piece {
    std::uint8_t w = 0, h = 0, dx = 0, y = 0;
    std::vector<std::uint8_t> data;  // w*h nibble bytes
  };

  // 載入 viewport/ui_pieces.bin(magic DWUIP)。
  static std::optional<UiPieces> load(const std::filesystem::path& bin);

  const Piece& piece(int idx) const { return pieces_[idx]; }

  // draw_ui_piece(opendw ui.c:546):把單片 blit 進 framebuffer。
  // 各片自帶 (x,y) 定位(x = dx*4, y = y_pos)。
  void draw_piece(Framebuffer& fb, int idx) const;

  // 完整 in-game chrome:ui_draw() 的靜態部分(石磚邊框 pieces 0..8)
  //   + 右側 pillar(piece 9)+ 頂端石磚邊框(pieces 0x17..)+ Dragon Wars logo(piece 5)。
  //   不畫 viewport / 面板「內部」像素。對應原版 S_GAME / S_COMBAT 外框。
  void draw_chrome(Framebuffer& fb) const;

private:
  std::array<Piece, kCount> pieces_{};
};

}  // namespace dw::render
