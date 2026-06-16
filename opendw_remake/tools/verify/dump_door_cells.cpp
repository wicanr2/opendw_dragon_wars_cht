// dump_door_cells — 找出「門牆格」(牆 nibble 的高半低 nibble hilo!=0)的座標,
//   並列其 tile 型(word_11C8)、四方向牆 nibble,佐證門語意(可走?事件格?)。
// 用法:dump_door_cells <bundle_dir> <area>
#include <cstdio>
#include <string>
#include "../../src/render/viewport_compose.hpp"
#include "../../src/resource/level.hpp"
using namespace dw;

int main(int argc, char** argv) {
  if (argc < 3) { std::fprintf(stderr, "usage: %s <bundle_dir> <area>\n", argv[0]); return 2; }
  std::string dir = argv[1]; int a = std::atoi(argv[2]);
  auto lv = res::Level::load_file(dir + "/maps/" + std::to_string(a) + ".lvl");
  if (!lv) { std::fprintf(stderr, "load fail\n"); return 1; }
  render::LevelComponents lc = render::parse_level_components(*lv);
  // 哪些 nibble 是門(hilo!=0)。
  bool door_nib[16] = {};
  std::printf("area %d \"%s\" %dx%d  door-nibbles(hilo!=0):", a, lv->name.c_str(), lv->w, lv->h);
  for (int n = 1; n <= 15; ++n)
    if (lc.a56C6[n + 0xF] & 0xF) { door_nib[n] = true; std::printf(" n%d(hilo%X)", n, lc.a56C6[n + 0xF] & 0xF); }
  std::printf("\n");
  int found = 0;
  for (int y = 0; y < lv->h; ++y)
    for (int x = 0; x < lv->w; ++x) {
      std::uint16_t w = lv->wall(x, y);
      int lo = w & 0xF, hi = (w >> 4) & 0xF;
      bool is_door = (lo && door_nib[lo]) || (hi && door_nib[hi]);
      if (!is_door) continue;
      ++found;
      std::printf("  (%2d,%2d) wall=%04X lo=%X hi=%X tile=%02X\n", x, y, w, lo, hi, lv->tile(x, y));
    }
  std::printf("  total door-wall cells: %d\n", found);
  return 0;
}
