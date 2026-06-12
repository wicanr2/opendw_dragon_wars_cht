// render_sprite_bundle — 從 asset bundle 載入 .spr sprite 渲染(不碰 DATA1)。
#include <cstdio>
#include "../../src/render/sprite.hpp"
int main(int argc,char**argv){
  if(argc<3){std::fprintf(stderr,"usage: %s <sprite.spr> <out.ppm>\n",argv[0]);return 2;}
  auto sp=dw::render::Sprite::load(argv[1]);
  if(!sp){std::fprintf(stderr,"load .spr failed\n");return 1;}
  dw::render::Framebuffer fb; fb.clear(0);
  sp->blit(fb,0,0);  // 全畫(對拍 DATA1 sprite_dump 輸出)
  std::FILE*f=std::fopen(argv[2],"wb"); fb.write_ppm(f); std::fclose(f);
  std::fprintf(stderr,"sprite %dx%d rendered from bundle (no DATA1)\n",sp->w,sp->h);
  return 0;
}
