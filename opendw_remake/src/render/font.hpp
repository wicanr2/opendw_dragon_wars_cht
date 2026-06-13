// font — 遊戲原生 8×8 ASCII 點陣字(chr_table)。
//
// chr_table 嵌在 DRAGON.COM,opendw 以 com_extract(0xBF52,0x400) 取出(COM_ORG_START=0x100,
// 故檔案 offset = 0xBE52)。128 字 × 8 bytes;glyph 以 MSB-first 渲染(對照 ui.c draw_character)。
#pragma once
#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>
#include "framebuffer.hpp"

namespace dw::render {

class Font8x8 {
public:
  // 從 dragon.com 載入 chr_table(1024 bytes @ 檔案 0xBE52)。
  static std::optional<Font8x8> load(const std::filesystem::path& dragon_com);

  // 從已抽出的 raw 字模檔載入(1024 bytes @ 檔案 0;見 assets/fonts/dw8x8.bin),
  // 讓 app 自包含、不依賴 DRAGON.COM。
  static std::optional<Font8x8> load_table(const std::filesystem::path& raw);

  // 在像素座標 (px,py) 畫一個字(charcode & 0x7F);set 像素=fg,unset=bg。
  void draw_char(Framebuffer& fb, int px, int py, std::uint8_t ch,
                 std::uint8_t fg = 15, std::uint8_t bg = 0) const;
  // 畫 ASCII 字串(每字寬 8px)。
  void draw_string(Framebuffer& fb, int px, int py, const char* s,
                   std::uint8_t fg = 15, std::uint8_t bg = 0) const;

private:
  std::array<std::uint8_t, 1024> table_{};  // 128 字 × 8 bytes
};

}  // namespace dw::render
