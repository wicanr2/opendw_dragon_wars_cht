// dump_wall_attr — 逐 area dump 牆屬性 word_11C6,統計門/密門位元 pattern(grounding 用,非回歸)。
//
// 目的:逆向判定門/密門/陷阱在 .lvl 牆屬性層(word_11C6)的編碼。
//   依 opendw engine.c draw_minimap_row(0x187A)/move_player_on_map(0x536B):
//     • 高 byte bit3(0x08)= 該格「有牆/門結構」旗標(test byte[11C7],08)。
//     • 高 byte bit4-5((>>4)&3)→ data_56E5[bx+4] 天花板/地面資源。
//     • 高 byte (>>4)&0xF 與低 byte &0xF → data_56C6[bl] 牆/門 sprite(side/front)。
//     • 高 byte &0x7 → data_56E5[bx+7] 額外結構(門框/密門?)。
//   本工具列出每張圖 word_11C6 的分佈,找門 nibble 值。
//
// 用法:dump_wall_attr <bundle_dir> [area]
#include <cstdio>
#include <map>
#include <string>
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
    std::map<std::uint16_t, int> hist;          // word_11C6 -> count
    std::map<int, int> hi_nib, lo_nib, lo7;     // nibble 分佈(僅 0x08 旗標格)
    std::map<int, int> tile_hist;               // word_11C8 tile 型分佈
    int doored = 0, total = 0;
    for (int y = 0; y < lv->h; ++y)
      for (int x = 0; x < lv->w; ++x) {
        std::uint16_t w = lv->wall(x, y);
        ++total;
        ++hist[w];
        ++tile_hist[lv->tile(x, y)];
        std::uint8_t hb = (w >> 8) & 0xFF;
        if (hb & 0x08) {
          ++doored;
          ++hi_nib[(w >> 4) & 0xF];
          ++lo_nib[w & 0xF];
          ++lo7[hb & 0x7];
        }
      }
    std::printf("=== area %2d \"%s\"  %dx%d  flags=0x%X  cells=%d  hasStruct(bit3)=%d ===\n",
                a, lv->name.c_str(), lv->w, lv->h, lv->flags, total, doored);
    std::printf("  hi_nib((>>4)&F):");
    for (auto& kv : hi_nib) std::printf(" %X:%d", kv.first, kv.second);
    std::printf("\n  lo_nib(&F)     :");
    for (auto& kv : lo_nib) std::printf(" %X:%d", kv.first, kv.second);
    std::printf("\n  lo7(hb&7)      :");
    for (auto& kv : lo7) std::printf(" %X:%d", kv.first, kv.second);
    std::printf("\n  tile(11C8)     :");
    for (auto& kv : tile_hist) std::printf(" %02X:%d", kv.first, kv.second);
    std::printf("\n");
    // 列前幾個最常見 word 值
    std::printf("  top word_11C6  :");
    int shown = 0;
    // 簡單找出現次數最多的幾個
    for (int pass = 0; pass < 8; ++pass) {
      std::uint16_t best = 0; int bc = 0;
      for (auto& kv : hist) if (kv.second > bc) { bc = kv.second; best = kv.first; }
      if (bc == 0) break;
      std::printf(" %04X:%d", best, bc);
      hist[best] = 0; ++shown;
    }
    std::printf("\n");
  }
  return 0;
}
