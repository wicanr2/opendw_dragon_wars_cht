#include "render/ui_pieces.hpp"

#include <cstdio>

namespace dw::render {

std::optional<UiPieces> UiPieces::load(const std::filesystem::path& bin) {
  std::FILE* f = std::fopen(bin.string().c_str(), "rb");
  if (!f) return std::nullopt;
  std::fseek(f, 0, SEEK_END);
  long n = std::ftell(f);
  std::fseek(f, 0, SEEK_SET);
  std::vector<std::uint8_t> buf(n > 0 ? static_cast<std::size_t>(n) : 0);
  if (!buf.empty()) {
    if (std::fread(buf.data(), 1, buf.size(), f) != buf.size()) {
      std::fclose(f);
      return std::nullopt;
    }
  }
  std::fclose(f);

  // header: magic "DWUIP\0"(6) + version u16 + count u16
  if (buf.size() < 10) return std::nullopt;
  if (!(buf[0] == 'D' && buf[1] == 'W' && buf[2] == 'U' && buf[3] == 'I' &&
        buf[4] == 'P' && buf[5] == 0)) {
    return std::nullopt;
  }
  std::uint16_t count = static_cast<std::uint16_t>(buf[8] | (buf[9] << 8));
  if (count != kCount) return std::nullopt;

  UiPieces ui;
  std::size_t off = 10;
  for (int i = 0; i < kCount; ++i) {
    if (off + 6 > buf.size()) return std::nullopt;
    Piece& p = ui.pieces_[i];
    p.w = buf[off + 0];
    p.h = buf[off + 1];
    p.dx = buf[off + 2];
    p.y = buf[off + 3];
    std::uint16_t data_len = static_cast<std::uint16_t>(buf[off + 4] | (buf[off + 5] << 8));
    off += 6;
    if (off + data_len > buf.size()) return std::nullopt;
    p.data.assign(buf.begin() + off, buf.begin() + off + data_len);
    off += data_len;
  }
  return ui;
}

// 忠實 port draw_ui_piece(opendw ui.c:546)。
//   x 起點 = dx*4(像素);y 起點 = y_pos。每 byte = 2 像素:hi nibble = 左,lo = 右。
//   每片自帶定位;framebuffer stride 由 Framebuffer::put 處理(自動裁切出界)。
void UiPieces::draw_piece(Framebuffer& fb, int idx) const {
  if (idx < 0 || idx >= kCount) return;
  const Piece& p = pieces_[idx];
  if (p.data.empty()) return;
  const std::uint8_t* src = p.data.data();
  int x0 = static_cast<int>(p.dx) * 4;
  for (int row = 0; row < p.h; ++row) {
    int py = static_cast<int>(p.y) + row;
    int px = x0;
    for (int col = 0; col < p.w; ++col) {
      std::uint8_t al = *src++;
      fb.put(px++, py, (al >> 4) & 0x0F);  // hi = 左像素
      fb.put(px++, py, al & 0x0F);         // lo = 右像素
    }
  }
}

// in-game chrome:對應原版 ui_draw() 靜態邊框 + 右 pillar + 頂端石磚邊框 + logo。
//   pieces 0..8 = 主邊框(含 logo piece 5 @ x=216,y=0,48×32 立繪 + 角落磚)。
//   piece 9     = 右 pillar(UI_RIGHT_PILLAR;viewport 與面板間的分隔柱)。
//   pieces 0x17+4 .. 0x17+0x13 = 頂端橫向石磚邊框(ui_header_draw 無標題文字時的全幅)。
void UiPieces::draw_chrome(Framebuffer& fb) const {
  // 主邊框 + logo + 角落磚。
  for (int i = 0; i < 9; ++i) draw_piece(fb, i);
  // 右 pillar(分隔柱)。
  draw_piece(fb, kRightPillar);
  // 頂端石磚橫條(對應 ui_header_draw:i ∈ [4, 0x14) → pieces[0x17 + i])。
  for (int i = 4; i < 0x14; ++i) draw_piece(fb, i + kBrickFirstPicture);
}

}  // namespace dw::render
