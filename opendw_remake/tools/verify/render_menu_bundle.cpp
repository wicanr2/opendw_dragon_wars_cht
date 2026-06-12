// render_menu_bundle — 文字徹底脫離 DATA1:從 bundle 字串表取選單(不碰 DATA1),
// 渲染英文;經 i18n 出中文。對照 render_menu_demo(後者從 DATA1 解碼)。
#include <cstdio>
#include <string>
#include <vector>
#include "../../src/render/font.hpp"
#include "../../src/render/cjk_font.hpp"
#include "../../src/i18n/strings.hpp"
#include "../../src/i18n/string_table.hpp"
using namespace dw;
static std::vector<std::string> lines_of(const std::string& s){
  std::vector<std::string> o; std::string c;
  for(char ch:s){ if(ch=='\r'||ch=='\n'){o.push_back(c);c.clear();} else c.push_back(ch);} o.push_back(c); return o;
}
int main(int argc,char**argv){
  if(argc<5){std::fprintf(stderr,"usage: %s <strings.tsv> <dragon.com> <atlas> <menu.tsv>\n",argv[0]);return 2;}
  auto tbl=i18n::StringTable::load(argv[1]);      // ← bundle,非 DATA1
  auto font=render::Font8x8::load(argv[2]);
  auto cjk=render::CjkFont::load(argv[3]);
  auto tr=i18n::Strings::load(argv[4]);
  if(!tbl||!font||!cjk||!tr){std::fprintf(stderr,"load failed\n");return 1;}
  std::string menu=tbl->at(21);                   // 主選單字串(section 0 offset 21)
  if(menu.empty()){std::fprintf(stderr,"menu string not in bundle\n");return 1;}
  auto lines=lines_of(menu);
  render::Framebuffer zh; zh.clear(1);
  { int x=8; for(std::uint32_t cp:{U'火',U'龍',U'之',U'戰'}){cjk->draw(zh,x,8,cp,14);x+=24;} }
  int y=44;
  for(auto&ln:lines){ if(ln.empty()){y+=12;continue;}
    std::string z=tr->tr(ln); const char*p=z.c_str(); int x=16;
    while(*p){ std::uint32_t cp=render::utf8_next(p);
      if(cp<0x80){font->draw_char(zh,x,y+8,(std::uint8_t)cp,15,1);x+=8;} else {cjk->draw(zh,x,y,cp,15);x+=24;} }
    y+=28; }
  std::FILE*f=std::fopen("menu_bundle_zh.ppm","wb"); zh.write_ppm(f); std::fclose(f);
  std::fprintf(stderr,"menu from bundle (no DATA1): \"%s\"\n",menu.c_str());
  return 0;
}
