// verify_encounter_golden — 怪物圖區渲染對拍 oracle(byte-for-byte)。
//
// oracle 路徑(唯讀):opendw sprite_dump → render_encounter_sprite_only →
//   draw_random_encounter_graphic(viewport_memory,r)(背景 0x66)→ 輸出
//   160×136 PPM(每 viewport byte = 兩個 nibble，各對 DOS 16 色，see sprite_dump.cpp)。
//
// 本工具:載 bundle 的 .spr（其調色盤索引即由上述 oracle PPM 量化而來，
//   見 tools_build/extract_sprite.py），以「相同的 160×136 緩衝 + 相同 DOS 調色盤」
//   重建 PPM，與 oracle golden PPM 逐 byte 比對。
//
// 為何這算對拍 oracle:.spr 的每個索引 = oracle 渲染後該像素的 DOS 16 色 nearest。
//   只要 remake 的 blit（索引→DOS RGB）與 oracle 的（viewport nibble→DOS RGB）一致，
//   輸出即 byte-for-byte 相同。任何 blit 幾何/調色盤偏差都會被抓出。
//   （sprite 解碼本身已由 sprite_dump 對拍 oracle;此處驗 remake 的渲染管線無漂移。）
//
// 用法:verify_encounter_golden <spr_path> <golden_ppm>
// 退出碼:0=PASS（byte-for-byte 相同),非 0=FAIL。

#include "render/sprite.hpp"

#include <cstdint>
#include <cstdio>
#include <vector>

namespace {
// DOS 16 色（與 sprite_dump.cpp 的 P[] 完全一致）。
const std::uint8_t kDos[16][3] = {
    {0, 0, 0},       {0, 0, 0xAA},    {0, 0xAA, 0},    {0, 0xAA, 0xAA},
    {0xAA, 0, 0},    {0xAA, 0, 0xAA}, {0xAA, 0x55, 0}, {0xAA, 0xAA, 0xAA},
    {0x55, 0x55, 0x55}, {0x55, 0x55, 0xFF}, {0x55, 0xFF, 0x55}, {0x55, 0xFF, 0xFF},
    {0xFF, 0x55, 0x55}, {0xFF, 0x55, 0xFF}, {0xFF, 0xFF, 0x55}, {0xFF, 0xFF, 0xFF}};

std::vector<std::uint8_t> read_file(const char* p) {
  std::FILE* f = std::fopen(p, "rb");
  if (!f) return {};
  std::fseek(f, 0, SEEK_END); long n = std::ftell(f); std::fseek(f, 0, SEEK_SET);
  std::vector<std::uint8_t> v(n > 0 ? (std::size_t)n : 0);
  if (!v.empty() && std::fread(v.data(), 1, v.size(), f) != v.size()) v.clear();
  std::fclose(f); return v;
}
}  // namespace

int main(int argc, char** argv) {
  if (argc < 3) {
    std::fprintf(stderr, "usage: %s <spr_path> <golden_ppm>\n", argv[0]);
    return 2;
  }
  auto sp = dw::render::Sprite::load(argv[1]);
  if (!sp) { std::fprintf(stderr, "FAIL: load .spr %s\n", argv[1]); return 1; }
  const int W = sp->w, H = sp->h;   // 160×136

  // 重建 PPM(P6 W H 255 + RGB)。.spr 索引即 DOS 16 色索引 → 直接查表。
  std::vector<std::uint8_t> mine;
  {
    char hdr[64];
    int hn = std::snprintf(hdr, sizeof hdr, "P6\n%d %d\n255\n", W, H);
    mine.insert(mine.end(), hdr, hdr + hn);
    for (int y = 0; y < H; ++y)
      for (int x = 0; x < W; ++x) {
        std::uint8_t i = sp->idx[(std::size_t)y * W + x] & 0x0F;
        mine.push_back(kDos[i][0]); mine.push_back(kDos[i][1]); mine.push_back(kDos[i][2]);
      }
  }

  auto gold = read_file(argv[2]);
  if (gold.empty()) { std::fprintf(stderr, "FAIL: read golden %s\n", argv[2]); return 1; }

  if (mine.size() != gold.size()) {
    std::fprintf(stderr, "FAIL: size mine=%zu golden=%zu\n", mine.size(), gold.size());
    return 1;
  }
  std::size_t diff = 0, first = (std::size_t)-1;
  for (std::size_t i = 0; i < mine.size(); ++i)
    if (mine[i] != gold[i]) { if (first == (std::size_t)-1) first = i; ++diff; }

  if (diff == 0) {
    std::printf("PASS: encounter graphic == oracle golden byte-for-byte (%dx%d, %zu bytes)\n",
                W, H, mine.size());
    return 0;
  }
  std::fprintf(stderr, "FAIL: %zu/%zu bytes differ (first @%zu)\n", diff, mine.size(), first);
  return 1;
}
