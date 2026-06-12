// render_vm_text — 完整閉環:從 bundle 載 section 0 bytecode(不碰 DATA1)→ VM 執行 →
// op_78/77/7B emit 字串 → i18n 換中文 → 渲染。證明「跑 script → 顯示在地化文字」。
#include <cstdio>
#include <string>
#include <vector>
#include "../../src/resource/provider.hpp"
#include "../../src/vm/interpreter.hpp"
#include "../../src/render/font.hpp"
#include "../../src/render/cjk_font.hpp"
#include "../../src/i18n/strings.hpp"
using namespace dw;
static std::vector<std::string> lines_of(const std::string& s){
  std::vector<std::string> o; std::string c;
  for(char ch:s){ if(ch=='\r'||ch=='\n'){o.push_back(c);c.clear();} else c.push_back(ch);} o.push_back(c); return o;
}
int main(int argc,char**argv){
  if(argc<5){std::fprintf(stderr,"usage: %s <bundle_dir> <dragon.com> <atlas> <menu.tsv> [start_pc]\n",argv[0]);return 2;}
  res::BundleProvider bun(argv[1]);
  auto sec0=bun.load(0);              // ← bundle,非 DATA1
  if(!sec0){std::fprintf(stderr,"bundle load section 0 failed\n");return 1;}
  auto font=render::Font8x8::load(argv[2]);
  auto cjk=render::CjkFont::load(argv[3]);
  auto tr=i18n::Strings::load(argv[4]);
  if(!font||!cjk||!tr){std::fprintf(stderr,"font/i18n load failed\n");return 1;}

  std::vector<std::pair<std::size_t,std::string>> msgs;
  vm::VmState st; st.script=*sec0; st.pc = argc>5 ? (std::size_t)std::atoi(argv[5]) : 0;
  vm::Interpreter ip(st);
  ip.set_message_sink([&](std::size_t off,const std::string&s){ msgs.push_back({off,s}); });
  int steps=ip.run();
  std::fprintf(stderr,"VM ran %d steps, emitted %zu strings\n",steps,msgs.size());

  // 找含 "Begin a new game" 的選單字串渲染(英/中)
  render::Framebuffer fb; fb.clear(1);
  { int x=8; for(std::uint32_t cp:{U'火',U'龍',U'之',U'戰'}){cjk->draw(fb,x,8,cp,14);x+=24;} }
  font->draw_string(fb,112,16,"  (VM-driven)",6,1);
  int y=44; bool shown=false;
  for(auto&[off,s]:msgs){
    if(s.find("Begin a new game")==std::string::npos) continue;
    for(auto&ln:lines_of(s)){ if(ln.empty()){y+=12;continue;}
      std::string z=tr->tr(ln); const char*p=z.c_str(); int x=16;
      while(*p){ std::uint32_t cp=render::utf8_next(p);
        if(cp<0x80){font->draw_char(fb,x,y+8,(std::uint8_t)cp,15,1);x+=8;} else {cjk->draw(fb,x,y,cp,15);x+=24;} }
      y+=28; }
    shown=true; break;
  }
  std::fprintf(stderr, shown?"rendered localized menu (VM-driven, from bundle)\n":"menu not emitted\n");
  std::FILE*f=std::fopen("vm_text.ppm","wb"); fb.write_ppm(f); std::fclose(f);
  return 0;
}
