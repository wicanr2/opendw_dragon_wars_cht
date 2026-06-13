// main — OpenDW Remake app(R2 SDL2 開窗)。
//
// 第 2 小段(B):完整閉環在視窗呈現 ——
//   asset bundle 的 section 0 bytecode(不碰 DATA1)→ VM 執行 → op_78 emit 選單字串
//   → i18n 換繁體中文 → 畫進 320×200 framebuffer(CJK 24×24 + 8×8 ASCII)
//   → SdlVideo 放大顯示。字型全部來自 assets(自包含:dw8x8.bin + cjk24.atlas)。
//
// 用法:opendw_remake [--bundle DIR] [--font RAW] [--atlas ATLAS] [--menu TSV]
//                     [--pc N] [--frames N]
//   預設指向 repo 的 assets/;--frames N 跑 N 幀後結束(headless smoke test)。
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include "resource/provider.hpp"
#include "vm/interpreter.hpp"
#include "render/font.hpp"
#include "render/cjk_font.hpp"
#include "render/framebuffer.hpp"
#include "render/sprite.hpp"
#include "render/sdl_video.hpp"
#include "i18n/strings.hpp"
using namespace dw;

static std::vector<std::string> lines_of(const std::string& s) {
  std::vector<std::string> o; std::string c;
  for (char ch : s) { if (ch == '\r' || ch == '\n') { o.push_back(c); c.clear(); } else c.push_back(ch); }
  o.push_back(c); return o;
}

int main(int argc, char** argv) {
  std::string bundle = "assets/bundle";
  std::string font_raw = "assets/fonts/dw8x8.bin";
  std::string atlas = "assets/fonts/cjk24.atlas";
  std::string menu_tsv = "assets/i18n/zh-TW/menu.tsv";
  int start_pc = 20;       // section 0 的選單 op_78 觸發點(見專案 lore)
  int max_frames = -1;
  std::string dump;        // --dump <ppm>:把渲染的一幀寫出(headless 視覺驗證)
  std::string sprite_name; // --sprite NAME:改顯示 bundle 內的 sprite(NAME 或 .spr 路徑)
  for (int i = 1; i < argc; ++i) {
    auto eq = [&](const char* f) { return !std::strcmp(argv[i], f); };
    if (eq("--bundle") && i + 1 < argc) bundle = argv[++i];
    else if (eq("--font") && i + 1 < argc) font_raw = argv[++i];
    else if (eq("--atlas") && i + 1 < argc) atlas = argv[++i];
    else if (eq("--menu") && i + 1 < argc) menu_tsv = argv[++i];
    else if (eq("--pc") && i + 1 < argc) start_pc = std::atoi(argv[++i]);
    else if (eq("--frames") && i + 1 < argc) max_frames = std::atoi(argv[++i]);
    else if (eq("--dump") && i + 1 < argc) dump = argv[++i];
    else if (eq("--sprite") && i + 1 < argc) sprite_name = argv[++i];
  }

  auto font = render::Font8x8::load_table(font_raw);
  if (!font) { std::fprintf(stderr, "font load failed: %s\n", font_raw.c_str()); return 1; }
  render::Framebuffer fb;

  if (!sprite_name.empty()) {
    // ── A:sprite 檢視模式(從 bundle 載 .spr,不碰 DATA1)──
    std::string path = sprite_name.find('/') != std::string::npos
                         ? sprite_name : bundle + "/sprites/" + sprite_name + ".spr";
    auto sp = render::Sprite::load(path);
    if (!sp) { std::fprintf(stderr, "sprite load failed: %s\n", path.c_str()); return 1; }
    fb.clear(0);
    sp->blit(fb, (render::kW - sp->w) / 2, (render::kH - sp->h) / 2, 6);  // 置中,棕底(6)透明
    font->draw_string(fb, 8, 8, sprite_name.c_str(), 15, 0);
    std::fprintf(stderr, "sprite %s %dx%d rendered from bundle (no DATA1)\n",
                 sprite_name.c_str(), sp->w, sp->h);
  } else {
    // ── B:VM 驅動的在地化選單 ──
    res::BundleProvider bun(bundle);
    auto sec0 = bun.load(0);
    auto cjk = render::CjkFont::load(atlas);
    auto tr = i18n::Strings::load(menu_tsv);
    if (!sec0) { std::fprintf(stderr, "bundle section 0 load failed: %s\n", bundle.c_str()); return 1; }
    if (!cjk || !tr) { std::fprintf(stderr, "atlas/i18n load failed\n"); return 1; }

    std::vector<std::string> msgs;
    { vm::VmState st; st.script = *sec0; st.pc = (std::size_t)start_pc;
      vm::Interpreter ip(st);
      ip.set_message_sink([&](std::size_t, const std::string& s) { msgs.push_back(s); });
      int steps = ip.run();
      std::fprintf(stderr, "VM ran %d steps, emitted %zu strings\n", steps, msgs.size()); }

    std::string menu;
    for (auto& s : msgs) if (s.find("Begin a new game") != std::string::npos) { menu = s; break; }
    if (menu.empty()) for (auto& s : msgs) if (s.size() > menu.size()) menu = s;

    fb.clear(1);
    { int x = 8; for (std::uint32_t cp : {U'火', U'龍', U'之', U'戰'}) { cjk->draw(fb, x, 8, cp, 14); x += 24; } }
    int y = 48;
    for (auto& ln : lines_of(menu)) {
      if (ln.empty()) { y += 12; continue; }
      std::string z = tr->tr(ln); const char* p = z.c_str(); int x = 16;
      while (*p) { std::uint32_t cp = render::utf8_next(p);
        if (cp < 0x80) { font->draw_char(fb, x, y + 8, (std::uint8_t)cp, 15, 1); x += 8; }
        else { cjk->draw(fb, x, y, cp, 15); x += 24; } }
      y += 28;
    }
    std::fprintf(stderr, "menu rendered (%zu chars)\n", menu.size());
  }

  if (!dump.empty()) {  // headless 視覺驗證:把這一幀存成 PPM
    std::FILE* d = std::fopen(dump.c_str(), "wb");
    if (d) { fb.write_ppm(d); std::fclose(d); std::fprintf(stderr, "dumped frame to %s\n", dump.c_str()); }
  }

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
