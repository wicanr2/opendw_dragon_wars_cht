// render_worldmap — 把 area 0 美化世界地圖渲染成 PPM(像素層;繁中標籤走文字層,
//   headless 不出字,僅印標籤錨點到 stderr 供目視對齊)。非 ctest gate。
//
// 用法:render_worldmap <0.lvl> <out.ppm> [px py]
#include <cstdio>
#include "render/framebuffer.hpp"
#include "render/worldmap.hpp"
#include "resource/level.hpp"

int main(int argc, char** argv) {
  if (argc < 3) { std::fprintf(stderr, "usage: %s <0.lvl> <out.ppm> [px py]\n", argv[0]); return 2; }
  auto L = dw::res::Level::load_file(argv[1]);
  if (!L) { std::fprintf(stderr, "load fail %s\n", argv[1]); return 1; }
  int px = argc > 4 ? std::atoi(argv[3]) : -1;
  int py = argc > 4 ? std::atoi(argv[4]) : -1;

  dw::render::Framebuffer fb;
  dw::render::WorldMap wm;
  auto labels = wm.render(fb, *L, px, py);

  std::FILE* f = std::fopen(argv[2], "wb");
  if (!f) { std::fprintf(stderr, "open out fail\n"); return 1; }
  fb.write_ppm(f);
  std::fclose(f);

  std::fprintf(stderr, "labels (%zu):\n", labels.size());
  for (auto& lb : labels)
    std::fprintf(stderr, "  (%3d,%3d) %s%s\n", lb.x, lb.y,
                 lb.right_align ? "<-" : "", lb.name.c_str());
  std::fprintf(stderr, "wrote %s\n", argv[2]);
  return 0;
}
