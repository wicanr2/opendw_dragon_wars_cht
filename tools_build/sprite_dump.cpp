#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <resource.h>
#include <offsets.h>
#include <ui.h>

extern "C" void render_encounter_sprite_only(struct resource *r);

struct pal { unsigned char r,g,b; };
static const pal P[16]={{0,0,0},{0,0,0xAA},{0,0xAA,0},{0,0xAA,0xAA},{0xAA,0,0},{0xAA,0,0xAA},{0xAA,0x55,0},{0xAA,0xAA,0xAA},{0x55,0x55,0x55},{0x55,0x55,0xFF},{0x55,0xFF,0x55},{0x55,0xFF,0xFF},{0xFF,0x55,0x55},{0xFF,0x55,0xFF},{0xFF,0xFF,0x55},{0xFF,0xFF,0xFF}};

int main(int argc,char**argv){
  if(argc<3){fprintf(stderr,"usage: sprite_dump <res_index> <out.ppm>\n");return 1;}
  int idx=atoi(argv[1]);
  if(rm_init()!=0){fprintf(stderr,"rm_init failed\n");return 1;}
  init_offsets(0x50);
  init_viewport_memory();
  struct resource *r=resource_load((resource_section)idx);
  if(!r||!r->bytes){fprintf(stderr,"resource %d load failed\n",idx);return 1;}
  render_encounter_sprite_only(r);
  const unsigned char*vp=ui_get_viewport_mem();
  int rows=0x88, cols=0x50, W=cols*2, H=rows;
  FILE*f=fopen(argv[2],"wb");
  fprintf(f,"P6\n%d %d\n255\n",W,H);
  for(int y=0;y<rows;y++){
    const unsigned char*src=vp+y*cols;
    for(int x=0;x<cols;x++){
      unsigned char b=src[x];
      pal hi=P[(b>>4)&0xF], lo=P[b&0xF];
      fputc(hi.r,f);fputc(hi.g,f);fputc(hi.b,f);
      fputc(lo.r,f);fputc(lo.g,f);fputc(lo.b,f);
    }
  }
  fclose(f);
  printf("wrote %s (%dx%d) from resource %d\n",argv[2],W,H,idx);
  return 0;
}
