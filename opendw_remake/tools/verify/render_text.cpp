// render_text — 用遊戲 8×8 字渲染樣本字串,輸出 PPM 供視覺驗證。
#include <cstdio>
#include "../../src/render/font.hpp"
int main(int argc, char** argv){
  if(argc<3){ std::fprintf(stderr,"usage: %s <dragon.com> <out.ppm>\n",argv[0]); return 2; }
  auto font = dw::render::Font8x8::load(argv[1]);
  if(!font){ std::fprintf(stderr,"load font failed\n"); return 1; }
  dw::render::Framebuffer fb; fb.clear(1);  // 藍底
  font->draw_string(fb, 8, 8,  "DRAGON WARS", 14, 1);
  font->draw_string(fb, 8, 24, "Read paragraph 137.", 15, 1);
  font->draw_string(fb, 8, 40, "ABCDEFGHIJKLMNOPQRSTUVWXYZ", 11, 1);
  font->draw_string(fb, 8, 56, "abcdefghijklmnopqrstuvwxyz", 10, 1);
  font->draw_string(fb, 8, 72, "0123456789  !?.,'-()", 12, 1);
  std::FILE* f=std::fopen(argv[2],"wb"); fb.write_ppm(f); std::fclose(f);
  return 0;
}
