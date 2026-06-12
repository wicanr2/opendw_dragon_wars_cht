// render_title — 用 remake 的 archive + render 解碼 res29(標題畫面)為 PPM。
// 與 golden(scene_render.py 的驗證輸出)對拍 = R2 render golden 測試。
//
// 用法: render_title <data_dir> <out.ppm>
#include <cstdio>
#include "../../src/resource/archive.hpp"
#include "../../src/render/picture.hpp"

int main(int argc, char** argv) {
  if (argc < 3) { std::fprintf(stderr, "usage: %s <data_dir> <out.ppm>\n", argv[0]); return 2; }
  auto arc = dw::res::Archive::open(argv[1]);
  if (!arc) { std::fprintf(stderr, "open data1 failed\n"); return 1; }
  auto pic = arc->load(29);  // RESOURCE_TITLE3,解壓後 32000B
  if (!pic) { std::fprintf(stderr, "load res29 failed\n"); return 1; }
  std::fprintf(stderr, "res29 decompressed: %zu bytes\n", pic->size());
  dw::render::Framebuffer fb;
  dw::render::decode_fullscreen(fb, *pic);
  std::FILE* f = std::fopen(argv[2], "wb");
  if (!f) { std::fprintf(stderr, "open out failed\n"); return 1; }
  fb.write_ppm(f);
  std::fclose(f);
  return 0;
}
