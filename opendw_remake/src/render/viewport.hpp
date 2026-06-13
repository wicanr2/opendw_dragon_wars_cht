#pragma once

#include <array>
#include <cstdint>

#include "render/framebuffer.hpp"

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

  // 把 viewport_memory blit 到 framebuffer。
  //
  // port 自 opendw ui_update_viewport (ui.c:519)。viewport_memory 為
  // 136 列 × 80 byte,每 byte 含 2 像素 (hi nibble = 左、lo nibble = 右)。
  // 視窗左上角 = (ox, oy),原版預設 (16, 8) — 對應
  //   line_num = viewport_initial_offset(0) + 8;framebuffer offset =
  //   get_line_offset(line_num) + 0x10 = line_num*320 + 16。
  // 視窗大小 = 160 × 136 像素 (80 byte × 2 px/byte × 136 列)。
  void to_framebuffer(render::Framebuffer& fb, int ox = 16, int oy = 8) const;

  // 組合靜態框架 (透視框線 / 地板網格) 到 viewport_memory。
  //
  // port 自 opendw update_viewport (ui.c:1081):
  //   1. 清左右邊界 nibble (每列 mem[di] &= 0x0F、mem[di+0x4F] &= 0xF0,di += 0x50)。
  //   2. byte_104E = 0;vidx 3→0,各 decode 對應象限模板,
  //      xpos/ypos 取自 viewport_metadata[vidx*4 + 2/3]。
  //
  // 四個象限模板 (vp0..vp3) 由呼叫端載入後傳入。本函式不負責 reset;
  // 呼叫端應先 reset(0)。
  void compose_frame(const std::uint8_t* vp0,
                     const std::uint8_t* vp1,
                     const std::uint8_t* vp2,
                     const std::uint8_t* vp3);

  // draw_viewport_sky 的 al!=1 分支:天空/地板兩色交替填色。
  // port 自 opendw engine.c:5576。dx 由 88 遞減到 0 (共 89 列),每列 40 個
  // word (= 80 byte) 寫 data_575C[bx] 的 hi/lo byte;dx<0x28 時 bx|=2,
  // 每列尾 bx^=1。data_575C = {0x4040,0x0404,0,0}。
  void fill_sky_flat();

  // draw_sprite_to_viewport (engine.c:5512) 的像素 blit:
  //   ds = comp + word_104F + sprite_offset;size = *(u16)ds;
  //   if size==0 return (不動 word_104F);word_104F += size;
  //   payload = comp + word_104F;decode(payload, xpos, ypos, 0x50, byte_104E)。
  // word_104F 由呼叫端維護 (每個元件畫前 reset 0,refresh_viewport 慣例)。
  // 回傳 true 表示有畫 (size!=0)。
  bool draw_sprite(const std::uint8_t* comp, std::size_t comp_len,
                   std::uint16_t& word_104F, int sprite_offset,
                   int xpos, int ypos, std::uint8_t byte_104E);
};

} // namespace dw::render
