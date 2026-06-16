// dump_door_table — 逆向「門 vs 實心牆」:逐 area dump data_56C6 牆/門型表(兩半)
//   + 牆面 nibble 在地圖實際出現分佈 + 每個出現 nibble 選到的元件 tag(門 sprite 線索)。
//
// 依 opendw read_level_metadata(engine.c:5096-5110):data_56C6 每筆 2 byte:
//   低半 data_56C6[n]   = byte_lo & 0x7F   (元件型 index → data_59E4 sprite)
//   高半 data_56C6[n+F] = byte_hi (raw,含 0x80 旗標)  ← refresh_viewport 0x520C 用它
//                                                        寫 gs[0x26](疑似門/牆狀態)
//   refresh_viewport 0x52B2:other-component 用 data_56C6[nibble] → 牆/門 sprite。
//
// 用法:dump_door_table <bundle_dir> [area]
#include <cstdio>
#include <map>
#include <set>
#include <string>

#include "../../src/render/viewport_compose.hpp"
#include "../../src/resource/level.hpp"

using namespace dw;

int main(int argc, char** argv) {
  if (argc < 2) { std::fprintf(stderr, "usage: %s <bundle_dir> [area]\n", argv[0]); return 2; }
  std::string dir = argv[1];
  int only = (argc >= 3) ? std::atoi(argv[2]) : -1;

  for (int a = 0; a < 40; ++a) {
    if (only >= 0 && a != only) continue;
    auto lv = res::Level::load_file(dir + "/maps/" + std::to_string(a) + ".lvl");
    if (!lv) continue;
    render::LevelComponents lc = render::parse_level_components(*lv);

    // 地圖中實際出現的牆面 nibble(低 byte 高/低 nibble)。
    std::map<int, int> nib_hist;
    for (int y = 0; y < lv->h; ++y)
      for (int x = 0; x < lv->w; ++x) {
        std::uint16_t w = lv->wall(x, y);
        int lo = w & 0xF, hi = (w >> 4) & 0xF;
        if (lo) ++nib_hist[lo];
        if (hi) ++nib_hist[hi];
      }

    std::printf("=== area %2d \"%s\" %dx%d ===\n", a, lv->name.c_str(), lv->w, lv->h);
    // 56C6 兩半表(index 1..15)。
    std::printf("  56C6 lo[n] (type&0x7F): ");
    for (int n = 1; n <= 15; ++n) std::printf("%d:%02X ", n, lc.a56C6[n]);
    std::printf("\n  56C6 hi[n+F] (raw):     ");
    for (int n = 1; n <= 15; ++n) std::printf("%d:%02X ", n, lc.a56C6[n + 0xF]);
    // 高半 byte 低 nibble(refresh_viewport 0x520C 寫 gs[0x26] 的值;疑似門/牆語意位元)。
    std::printf("\n  hi-byte low-nibble:     ");
    for (int n = 1; n <= 15; ++n) std::printf("%d:%X ", n, lc.a56C6[n + 0xF] & 0xF);
    std::printf("\n  wall nibbles in map + tag:");
    for (auto& kv : nib_hist) {
      int n = kv.first;
      // 牆面元件選擇:al = data_56C6[n](型 index)→ data_59E4[al] tag。
      std::uint8_t al = lc.a56C6[n];
      int tag = (al <= 0x7F) ? lc.tags[al] : -1;
      int hilo = lc.a56C6[n + 0xF] & 0xF;   // 高半低 nibble
      std::printf(" n%d(x%d type%02X tag%d hilo%X%s)", n, kv.second, al, tag, hilo,
                  hilo != 0 ? "*" : "");
    }
    std::printf("\n");
  }
  return 0;
}
