#pragma once

#include <array>
#include <cstdint>

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

}  // namespace dw::render
