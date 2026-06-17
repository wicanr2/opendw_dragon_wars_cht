// render_fp_cell — 組第一人稱 viewport 並輸出 PPM(路徑 A:目視比對「門牆候選」
//   (hilo!=0)vs 一般牆 是否視覺有別)。用法:
//   render_fp_cell <bundle_dir> <area> <x> <y> <facing 0N/1E/2S/3W> <out.ppm>
#include <cstdio>
#include <cstdlib>
#include <string>
#include "../../src/render/viewport_compose.hpp"
#include "../../src/render/framebuffer.hpp"
using namespace dw;
int main(int argc, char** argv) {
  if (argc < 7) { std::fprintf(stderr, "usage: %s <bundle> <area> <x> <y> <facing> <out.ppm>\n", argv[0]); return 2; }
  std::string dir = argv[1];
  int area = std::atoi(argv[2]), x = std::atoi(argv[3]), y = std::atoi(argv[4]), f = std::atoi(argv[5]);
  auto lv = res::Level::load_file(dir + "/maps/" + std::to_string(area) + ".lvl");
  if (!lv) { std::fprintf(stderr, "load fail\n"); return 1; }
  render::ComponentStore comps(dir + "/components");
  render::ViewportDecoder dec;
  render::render_first_person(*lv, x, y, f, dec, comps);
  render::Framebuffer fb; fb.clear(0);
  dec.to_framebuffer(fb, 16, 8);
  std::FILE* o = std::fopen(argv[6], "wb"); fb.write_ppm(o); std::fclose(o);
  std::fprintf(stderr, "rendered area %d (%d,%d) f%d -> %s\n", area, x, y, f, argv[6]);
  return 0;
}
