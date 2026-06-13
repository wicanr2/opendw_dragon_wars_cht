#pragma once

#include <array>
#include <cstdint>

// 第一人稱 viewport 解碼器。
//
// 忠實 port 自 Devin Smith 的 opendw C 反組譯 (src/lib/ui.c) 的
// decode_viewport_data + 5 個分派函式 (process_quadrant /
// draw_viewport_word_mode / draw_viewport_neg_x / draw_viewport_neg_x_alt /
// draw_viewport_flip_y),以
// tools_build/viewport_golden/golden_decode.c (已抽成可編譯、邏輯一致) 為翻譯藍本。
//
// 目標:逐像素正確 — viewport_memory 與 golden .vpmem byte-for-byte 相同。
// 變數語意/順序刻意保留與 golden_decode.c 一致 (這是逐像素正確性的關鍵)。

namespace dw::render {

class ViewportDecoder {
public:
  // 0x4F11 viewport_memory:10880 bytes (136 × 80)。
  static constexpr int kViewportMemSize = 10880;

  std::array<std::uint8_t, kViewportMemSize> mem{};

  // 清空 viewport_memory (對應原版 memset)。
  void reset(std::uint8_t fill = 0);

  // 解碼一個 template buffer 到 mem。
  //
  // 參數對應 golden harness:
  //   tmpl       — template buffer (vp->data),前 4 bytes 為 header
  //                (runlength, numruns, dx-byte, dy-byte)。
  //   xpos/ypos  — viewport_data 的初始座標 (harness 預設 0/0)。
  //   word_1053  — 列 stride 基準 (harness 預設 0x50);
  //                make_offset_table(word_1053) 等價 opendw init_offsets。
  //   byte_104E  — quadrant dispatch flag (harness 預設 0)。
  void decode(const std::uint8_t* tmpl,
              int xpos = 0,
              int ypos = 0,
              std::uint16_t word_1053 = 0x50,
              std::uint8_t byte_104E = 0);
};

} // namespace dw::render
