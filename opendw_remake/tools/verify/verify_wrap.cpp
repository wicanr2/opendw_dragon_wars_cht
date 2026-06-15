// verify_wrap — wrap 邊界關卡(.lvl flags bit1=0x2)的載入 + 環繞移動 + 連通驗證。
//
// 背景:opendw `check_map_boundary_x/y`(engine.c:5147/5176)在「座標越界 && flag&2」
// 走 `exit(1)`(未實作),故 wrap 關卡(area 0/18/19/22/27/34/35/39)在 oracle 無
// byte-for-byte 真值。remake 以「標準 modular 環繞慣例」(走出東緣→西緣、南緣→北緣)
// 補上。本工具:
//   1. 確認 8 張 wrap 關卡全部能載入且 flag&2 成立(其餘 32 張 flag&2==0)。
//   2. 對每張 wrap 關卡做「環繞 flood-fill」:從第一個可走格出發,4 向移動且座標
//      modular 環繞,統計可走連通分量大小;對拍「非環繞 flood-fill」看環繞是否
//      真的把跨邊緣的格子接上(連通分量 ≥ 非環繞版本)。
//   3. 斷言:wrap 關卡環繞 flood-fill 不崩、連通分量 > 0;非 wrap 關卡 walkable_wrap
//      == walkable(環繞邏輯不污染非 wrap 路徑)。
//
// 注意:本工具不對拍 opendw(wrap 在 oracle 為 exit(1) 未實作)—— 純自洽 + 不破壞
// 不變式的驗證。連通數字是 remake 環繞慣例下的可達範圍,非原版真值。
//
// 用法:verify_wrap <bundle_dir>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <queue>
#include <string>
#include <vector>

#include "../../src/resource/level.hpp"

using namespace dw;

namespace {

// flood-fill 可走連通分量大小。wrap=true 時 4 向座標 modular 環繞。
int flood_component(const res::Level& lvl, bool wrap, int& start_x, int& start_y) {
  const int W = lvl.w, H = lvl.h;
  if (W <= 0 || H <= 0) return 0;
  // 找第一個可走格當起點。
  int sx = -1, sy = -1;
  for (int y = 0; y < H && sx < 0; ++y)
    for (int x = 0; x < W; ++x)
      if (lvl.tile(x, y) != 0) { sx = x; sy = y; break; }
  if (sx < 0) return 0;
  start_x = sx; start_y = sy;

  std::vector<std::uint8_t> seen((std::size_t)W * H, 0);
  std::queue<std::pair<int, int>> q;
  auto idx = [&](int x, int y) { return (std::size_t)y * W + x; };
  seen[idx(sx, sy)] = 1;
  q.push({sx, sy});
  int count = 0;
  const int dx4[4] = {0, 1, 0, -1}, dy4[4] = {-1, 0, 1, 0};
  while (!q.empty()) {
    auto [cx, cy] = q.front();
    q.pop();
    ++count;
    for (int d = 0; d < 4; ++d) {
      int nx = cx + dx4[d], ny = cy + dy4[d];
      if (wrap) { nx = lvl.wrap_x(nx); ny = lvl.wrap_y(ny); }
      if (nx < 0 || ny < 0 || nx >= W || ny >= H) continue;
      if (lvl.tile(nx, ny) == 0) continue;
      if (seen[idx(nx, ny)]) continue;
      seen[idx(nx, ny)] = 1;
      q.push({nx, ny});
    }
  }
  return count;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 2) {
    std::fprintf(stderr, "usage: %s <bundle_dir>\n", argv[0]);
    return 2;
  }
  const std::string bundle = argv[1];
  const std::array<int, 8> wrap_areas = {0, 18, 19, 22, 27, 34, 35, 39};

  int fails = 0;

  // 1) 載入 + flag&2 不變式:wrap_areas 應 wrap=1,其餘應 wrap=0。
  std::printf("== wrap 旗標 / 載入檢查(全 40 關)==\n");
  for (int a = 0; a < 40; ++a) {
    auto lvl = res::Level::load_file(bundle + "/maps/" + std::to_string(a) + ".lvl");
    if (!lvl) { std::printf("  area %2d: LOAD FAIL\n", a); ++fails; continue; }
    bool expect_wrap = false;
    for (int w : wrap_areas) if (w == a) expect_wrap = true;
    bool got = lvl->wraps();
    if (got != expect_wrap) {
      std::printf("  area %2d: FAIL wrap=%d expect=%d (flags=0x%02X)\n", a, got,
                  expect_wrap, lvl->flags);
      ++fails;
    }
  }

  // 2) 環繞 flood-fill 連通(wrap 關卡)。
  std::printf("== wrap 關卡環繞 flood-fill 連通 ==\n");
  for (int a : wrap_areas) {
    auto lvl = res::Level::load_file(bundle + "/maps/" + std::to_string(a) + ".lvl");
    if (!lvl) { ++fails; continue; }
    int sx = 0, sy = 0;
    int nowrap = flood_component(*lvl, false, sx, sy);
    int withwrap = flood_component(*lvl, true, sx, sy);
    int total_walk = 0;
    for (int y = 0; y < lvl->h; ++y)
      for (int x = 0; x < lvl->w; ++x)
        if (lvl->tile(x, y) != 0) ++total_walk;
    std::printf(
        "  area %2d \"%s\" %dx%d: walkable=%d  comp(no-wrap)=%d  comp(wrap)=%d  start=(%d,%d)\n",
        a, lvl->name.c_str(), lvl->w, lvl->h, total_walk, nowrap, withwrap, sx, sy);
    if (withwrap <= 0) { std::printf("    FAIL: wrap component empty\n"); ++fails; }
    if (withwrap < nowrap) {
      std::printf("    FAIL: wrap component shrank below no-wrap (邏輯錯誤)\n");
      ++fails;
    }
  }

  // 3) 不變式:非 wrap 關卡 walkable_wrap == walkable(環繞不污染非 wrap)。
  std::printf("== 非 wrap 關卡:walkable_wrap == walkable(含越界)==\n");
  {
    int checked = 0, mism = 0;
    for (int a = 0; a < 40; ++a) {
      bool is_wrap = false;
      for (int w : wrap_areas) if (w == a) is_wrap = true;
      if (is_wrap) continue;
      auto lvl = res::Level::load_file(bundle + "/maps/" + std::to_string(a) + ".lvl");
      if (!lvl) continue;
      // 掃界內 + 一圈界外座標。
      for (int y = -1; y <= lvl->h; ++y)
        for (int x = -1; x <= lvl->w; ++x) {
          ++checked;
          if (lvl->walkable_wrap(x, y) != lvl->walkable(x, y)) ++mism;
        }
    }
    std::printf("  checked=%d mismatch=%d\n", checked, mism);
    if (mism != 0) { std::printf("  FAIL: 非 wrap 路徑被污染\n"); ++fails; }
  }

  std::printf("\n%s (fails=%d)\n", fails == 0 ? "PASS" : "FAIL", fails);
  return fails == 0 ? 0 : 1;
}
