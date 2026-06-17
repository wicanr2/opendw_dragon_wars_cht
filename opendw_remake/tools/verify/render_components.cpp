// render_components — 渲染 assets/bundle/components/<tag>.bin 的元件 sprite frame,
//   把每個 tag 的「最近距 frame」(sprite_offset=0)畫進 viewport 並輸出 PPM,供
//   視覺分類哪些元件是「門」(門框/門板)vs 牆/岩(路徑 A 逆向)。
//
// 每個 .bin 是 [size:2 LE][payload] 串接的多 frame 元件資源。frame 0 = 最近距全尺寸。
// 用法:render_components <components_dir> <tag> <out.ppm> [frame_index]
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include "../../src/render/viewport.hpp"
#include "../../src/render/framebuffer.hpp"

using namespace dw;

int main(int argc, char** argv) {
  if (argc < 4) {
    std::fprintf(stderr, "usage: %s <components_dir> <tag> <out.ppm> [frame]\n", argv[0]);
    return 2;
  }
  std::string dir = argv[1];
  int tag = std::atoi(argv[2]);
  std::string out = argv[3];
  int want_frame = (argc >= 5) ? std::atoi(argv[4]) : 0;

  std::string path = dir + "/" + std::to_string(tag) + ".bin";
  std::FILE* f = std::fopen(path.c_str(), "rb");
  if (!f) { std::fprintf(stderr, "open fail %s\n", path.c_str()); return 1; }
  std::fseek(f, 0, SEEK_END); long sz = std::ftell(f); std::fseek(f, 0, SEEK_SET);
  std::vector<std::uint8_t> comp((std::size_t)sz);
  if (std::fread(comp.data(), 1, (std::size_t)sz, f) != (std::size_t)sz) { std::fclose(f); return 1; }
  std::fclose(f);

  // 列出每個 frame 的 size (供觀察 frame 數)。
  std::fprintf(stderr, "tag %d: %ld bytes; frames:", tag, sz);
  {
    std::size_t off = 0; int n = 0;
    while (off + 2 <= comp.size()) {
      std::uint16_t s = comp[off] | (comp[off + 1] << 8);
      if (s == 0) break;
      std::fprintf(stderr, " [%d]=%u", n, s);
      off += s; ++n;
    }
    std::fprintf(stderr, " (total %d)\n", n);
  }

  render::ViewportDecoder dec;
  dec.reset(0);
  // draw_sprite 以 word_104F 累進:每呼叫一次畫一個 frame 並把 word_104F 推進。
  // 要看 frame N,連畫 N+1 次(前 N 次當 skip,最後一次落在乾淨 reset 後重畫)。
  // 簡化:reset → 連畫到 want_frame(中間 frame 也會疊上,但 want_frame=0 即純 frame0)。
  std::uint16_t word_104F = 0;
  for (int i = 0; i <= want_frame; ++i) {
    if (i == want_frame) dec.reset(0);  // 只保留目標 frame 的像素
    if (!dec.draw_sprite(comp.data(), comp.size(), word_104F, 0, 0, 0, 0)) break;
  }

  render::Framebuffer fb;
  fb.clear(8);  // 灰底,凸顯 sprite
  dec.to_framebuffer(fb, 16, 8);
  std::FILE* o = std::fopen(out.c_str(), "wb");
  fb.write_ppm(o); std::fclose(o);
  std::fprintf(stderr, "rendered tag %d frame %d -> %s\n", tag, want_frame, out.c_str());
  return 0;
}
