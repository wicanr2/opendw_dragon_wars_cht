#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include "resource/level.hpp"

// 第一人稱 viewport「組景」第一步:FOV tile 取樣 + 牆面 nibble 推導。
//
// 忠實 port 自 Devin Smith 的 opendw C 反組譯 (src/lib/engine.c):
//   - refresh_viewport 的 FOV 取樣迴圈 (engine.c:5632..5661)
//   - move_player_on_map (engine.c:5332) 的 nibble 旋轉
//   - get_map_tile_data (engine.c:5206) + check_map_boundary_x/y
//
// 對拍基準:tools_build/viewport_compose_golden/golden_compose.c。
// 變數語意/順序刻意保留與 opendw 一致 (這是 byte-for-byte 正確性的關鍵)。
//
// 本層只做「資料正確性」(取樣 (x,y) 序列 + 牆/地面 nibble),不畫像素。

namespace dw::render {

// 12 槽 FOV 取樣結果。對應 opendw 的:
//   wall[di]   = data_5A56[di]       (word_11CA 低 byte;牆/門面 nibble 來源)
//   ground[di] = data_5A56[di + 0xC] (word_11CA 高 byte & 0xF7;地面 nibble 來源)
// 另外保留每槽的取樣 (x,y) 與 primary tile 的 word_11C6 供診斷對拍。
struct FovSample {
  std::array<std::uint8_t, 12> wall{};    // data_5A56[0..11]
  std::array<std::uint8_t, 12> ground{};  // data_5A56[12..23]
  std::array<int, 12> sx{};               // 取樣 x (col)
  std::array<int, 12> sy{};               // 取樣 y (row)
  std::array<std::uint16_t, 12> raw{};    // primary tile word_11C6 (診斷)
};

// 對 (level, x, y, facing) 跑 refresh_viewport 的 FOV 取樣段。
// facing: 0=N 1=E 2=S 3=W (game_state[3])。
FovSample sample_fov(const res::Level& level, int x, int y, int facing);

// ---------------------------------------------------------------------------
// Step 2:元件選擇 + 繪製指令序列 (refresh_viewport 的 draw 呼叫段)。
//
// 從 .lvl 解析的關卡元件表 (data_56C6 牆/門型、data_56E5 元件資源索引、
// data_5897 元件型/旗標),忠實 port 自 opendw read_level_metadata /
// cache_level_components / cache_resources。selection 用的「resource tag」=
// (data_5897[idx] & 0x7F) + 0x6E,即 DATA1 section,不需載入 sprite 像素。
struct LevelComponents {
  std::array<std::uint8_t, 256> a5897{};  // data_5897 元件型/旗標 (+0xF 後為 resource index)
  std::array<std::uint8_t, 128> a56C6{};  // data_56C6 牆/門型表
  std::array<std::uint8_t, 128> a56E5{};  // data_56E5 元件資源索引 by type
  std::array<int, 128> tags{};            // data_59E4[i]->tag (computed; -1 = 未載入)

  // 由元件選擇用的 al (data_59E4 index) → resource tag (= DATA1 section)。
  int tag_for(std::uint8_t al) const { return tags[al]; }
};

// 解析 .lvl 的元件表 (read_level_metadata + cache_level_components +
// cache_resources 的選擇相依部分;不讀 DATA1 像素)。
LevelComponents parse_level_components(const res::Level& level);

// 單筆繪製指令 (對應一次 draw_sprite_to_viewport 呼叫)。
// batch: 0=SKY 1=GROUND 2=OTHER。對拍 opendw 的
// `Drawing component %d (tag: %d)` (OTHER 批) + sky/ground 的 draw 呼叫參數。
struct DrawCmd {
  int batch;            // 0=SKY 1=GND 2=OTH
  int counter;          // 迴圈 counter (sky = -1)
  int tag;              // word_1051->tag (= DATA1 section)
  int sprite_offset;    // draw_sprite_to_viewport 的 sprite_offset 參數
  int xpos;             // vp.xpos (有號)
  int ypos;             // vp.ypos
  std::uint8_t byte_104E;  // 象限旗標
};

// 跑 refresh_viewport 的選擇 + draw 呼叫段 (sky → ground 8..0 → other 23..0),
// 產出繪製指令序列。先不畫像素;每筆只記「選了哪個元件 (tag) + 畫在哪」。
std::vector<DrawCmd> compose_draw_sequence(const res::Level& level, int x, int y,
                                           int facing);

}  // namespace dw::render
