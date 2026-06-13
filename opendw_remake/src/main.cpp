// main — OpenDW Remake app 進入點(R2 SDL2 開窗,第 1 小段)。
//
// 目前最小自包含版:用 CJK atlas 在 320×200 framebuffer 畫「火龍之戰」+ 16 色盤條,
// 經 SdlVideo 放大顯示在視窗。證明開窗 / 調色盤 / 放大顯示的管線通了。
// 後續小段再接 bundle bytecode → VM → i18n 文字。
//
// 用法:opendw_remake [atlas] [--frames N]
//   atlas      CJK 24×24 atlas(預設 assets/fonts/cjk24.atlas)
//   --frames N  跑 N 幀後自動結束(headless smoke test;省略則開窗到關閉/ESC/Q)
#include <cstdio>
#include <cstring>
#include <string>
#include "render/cjk_font.hpp"
#include "render/framebuffer.hpp"
#include "render/sdl_video.hpp"
using namespace dw;

int main(int argc, char** argv) {
  std::string atlas = "assets/fonts/cjk24.atlas";
  int max_frames = -1;
  for (int i = 1; i < argc; ++i) {
    if (!std::strcmp(argv[i], "--frames") && i + 1 < argc) max_frames = std::atoi(argv[++i]);
    else if (argv[i][0] != '-') atlas = argv[i];
  }

  auto cjk = render::CjkFont::load(atlas);
  if (!cjk) { std::fprintf(stderr, "CJK atlas load failed: %s\n", atlas.c_str()); return 1; }

  // 畫面:藍底 + 標題「火龍之戰」+ 底部 16 色盤條(驗證調色盤對應)
  render::Framebuffer fb;
  fb.clear(1);                                  // DOS 藍
  int x = 100;
  for (std::uint32_t cp : {U'火', U'龍', U'之', U'戰'}) { cjk->draw(fb, x, 80, cp, 14); x += 28; }
  for (int c = 0; c < 16; ++c)                  // 底部色盤條
    for (int yy = 180; yy < 196; ++yy)
      for (int xx = 0; xx < 20; ++xx) fb.put(c * 20 + xx, yy, (std::uint8_t)c);

  render::SdlVideo vid;
  if (!vid.open(3)) { std::fprintf(stderr, "SDL open failed\n"); return 1; }
  int frames = 0;
  for (;;) {
    vid.present(fb);
    if (!vid.poll()) break;
    if (max_frames >= 0 && ++frames >= max_frames) break;
  }
  vid.close();
  std::fprintf(stderr, "ok (frames=%d)\n", frames);
  return 0;
}
