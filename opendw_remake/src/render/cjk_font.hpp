// cjk_font — 24×24 中文點陣字(由 gen_cjk_atlas.py 產生的 atlas 載入,免 freetype)。
//
// 與 8×8 ASCII 字混排進同一 framebuffer:ASCII 用 Font8x8(8px 寬),中文用本模組(24px 寬)。
#pragma once
#include <cstdint>
#include <filesystem>
#include <optional>
#include <unordered_map>
#include "framebuffer.hpp"

namespace dw::render {

class CjkFont {
public:
  // 載入 cjk24.atlas(magic CJK1)。
  static std::optional<CjkFont> load(const std::filesystem::path& atlas);

  bool has(std::uint32_t codepoint) const { return glyphs_.count(codepoint) != 0; }
  // 在像素 (px,py) 畫一個中文字(24×24);set 像素=color。回傳是否有該字。
  bool draw(Framebuffer& fb, int px, int py, std::uint32_t codepoint, std::uint8_t color) const;

  // 半尺寸(12×12)畫一個中文字:24×24 點陣 2×2 區塊只要任一點亮即點亮(OR 降採樣,
  // 保留筆畫連通性)。供 320×200 訊息區排較長句子用(每行約 26 字)。
  bool draw_half(Framebuffer& fb, int px, int py, std::uint32_t codepoint, std::uint8_t color) const;

  // 混排畫一行 UTF-8:ASCII 用 8×8(需傳入 Font8x8),中文用 24×24。回傳結束 x。
  // (為避免循環相依,實際混排在 demo/呼叫端做;此處只提供單字。)

private:
  std::unordered_map<std::uint32_t, std::array<std::uint8_t, 72>> glyphs_;
};

// UTF-8 解碼:回傳 codepoint 並前進 p。簡化版(假設輸入合法)。
std::uint32_t utf8_next(const char*& p);

}  // namespace dw::render
