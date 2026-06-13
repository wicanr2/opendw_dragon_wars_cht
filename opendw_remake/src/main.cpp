// main — OpenDW Remake app(R2 SDL2 開窗)。
//
// 已完成小段:
//   B  bundle bytecode → VM op_78 → i18n 繁中 → CJK+8×8 渲染 → SDL 顯示(在地化選單)
//   A  --sprite NAME:從 bundle 載 .spr 美術顯示在視窗(不碰 DATA1)
//   C  互動骨架:↑/↓(或 K/J)移動選單游標,Enter/Space 選取,ESC/Q 離開
//
// 字型全部來自 assets(自包含:dw8x8.bin + cjk24.atlas),執行期不依賴原始遊戲檔。
//
// 用法:opendw_remake [--bundle DIR] [--font RAW] [--atlas ATLAS] [--menu TSV]
//                     [--pc N] [--sprite NAME] [--frames N] [--dump PPM]
#include <cstdio>
#include <cstring>
#include <optional>
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

// 畫一行(8×8 ASCII + 24×24 CJK 混排),回傳結束 x。
static void draw_mixed(render::Framebuffer& fb, const render::Font8x8& font,
                       const render::CjkFont& cjk, int x, int y, const std::string& z,
                       std::uint8_t col, std::uint8_t bg) {
  const char* p = z.c_str();
  while (*p) {
    std::uint32_t cp = render::utf8_next(p);
    if (cp < 0x80) { font.draw_char(fb, x, y + 8, (std::uint8_t)cp, col, bg); x += 8; }
    else { cjk.draw(fb, x, y, cp, col); x += 24; }
  }
}

int main(int argc, char** argv) {
  std::string bundle = "assets/bundle";
  std::string font_raw = "assets/fonts/dw8x8.bin";
  std::string atlas = "assets/fonts/cjk24.atlas";
  std::string menu_tsv = "assets/i18n/zh-TW/menu.tsv";
  int start_pc = 20;
  int max_frames = -1;
  std::string dump, sprite_name;
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
  const bool menu_mode = sprite_name.empty();
  render::Framebuffer fb;

  // 選單模式需要的資料(VM 跑出的在地化選項)
  std::optional<render::CjkFont> cjk;
  std::string header;                 // 提示行(如「您希望‥」)
  std::vector<std::string> options;   // 可選項(已在地化)
  int sel = 0;

  if (!menu_mode) {
    // ── A:sprite 檢視 ──
    std::string path = sprite_name.find('/') != std::string::npos
                         ? sprite_name : bundle + "/sprites/" + sprite_name + ".spr";
    auto sp = render::Sprite::load(path);
    if (!sp) { std::fprintf(stderr, "sprite load failed: %s\n", path.c_str()); return 1; }
    fb.clear(0);
    sp->blit(fb, (render::kW - sp->w) / 2, (render::kH - sp->h) / 2, 6);
    font->draw_string(fb, 8, 8, sprite_name.c_str(), 15, 0);
    std::fprintf(stderr, "sprite %s %dx%d (bundle, no DATA1)\n", sprite_name.c_str(), sp->w, sp->h);
  } else {
    // ── B:VM 在地化選單 ──
    cjk = render::CjkFont::load(atlas);
    auto tr = i18n::Strings::load(menu_tsv);
    res::BundleProvider bun(bundle);
    auto sec0 = bun.load(0);
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

    std::vector<std::string> ls;
    for (auto& ln : lines_of(menu)) if (!ln.empty()) ls.push_back(tr->tr(ln));
    if (!ls.empty()) { header = ls.front(); options.assign(ls.begin() + 1, ls.end()); }
    if (options.empty() && !ls.empty()) { options = ls; header.clear(); }  // 沒 header 就全當選項
    std::fprintf(stderr, "menu: header=\"%s\" options=%zu\n", header.c_str(), options.size());
  }

  // 選單模式:依目前選擇把整幀畫出來(含游標)
  auto draw_menu = [&]() {
    fb.clear(1);
    { int x = 8; for (std::uint32_t cp : {U'火', U'龍', U'之', U'戰'}) { cjk->draw(fb, x, 8, cp, 14); x += 24; } }
    int y = 48;
    if (!header.empty()) { draw_mixed(fb, *font, *cjk, 16, y, header, 7, 1); y += 28; }
    for (std::size_t i = 0; i < options.size(); ++i) {
      bool cur = (int)i == sel;
      if (cur) font->draw_char(fb, 16, y + 8, (std::uint8_t)'>', 14, 1);  // 游標
      draw_mixed(fb, *font, *cjk, 40, y, options[i], cur ? 14 : 15, 1);   // 選中=黃,其餘=白
      y += 28;
    }
  };
  if (menu_mode) draw_menu();

  if (!dump.empty()) {
    std::FILE* d = std::fopen(dump.c_str(), "wb");
    if (d) { fb.write_ppm(d); std::fclose(d); std::fprintf(stderr, "dumped frame to %s\n", dump.c_str()); }
  }

  render::SdlVideo vid;
  if (!vid.open(3)) { std::fprintf(stderr, "SDL open failed\n"); return 1; }
  int frames = 0;
  for (;;) {
    vid.present(fb);
    render::Input in = vid.poll();
    if (in.quit) break;
    if (menu_mode && !options.empty()) {
      int n = (int)options.size();
      if (in.up)   { sel = (sel - 1 + n) % n; draw_menu(); }
      if (in.down) { sel = (sel + 1) % n; draw_menu(); }
      if (in.select) std::fprintf(stderr, "selected: %s\n", options[sel].c_str());
    }
    if (max_frames >= 0 && ++frames >= max_frames) break;
  }
  vid.close();
  std::fprintf(stderr, "ok (frames=%d, sel=%d)\n", frames, sel);
  return 0;
}
