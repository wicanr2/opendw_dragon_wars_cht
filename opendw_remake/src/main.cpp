// main — OpenDW Remake app(R2 SDL2 開窗)。
//
// 已完成小段:
//   B  bundle bytecode → VM op_78 → i18n 繁中 → CJK+8×8 渲染 → SDL 顯示(在地化選單)
//   A  --sprite NAME:從 bundle 載 .spr 美術顯示在視窗(不碰 DATA1)
//   C  互動骨架:poll() → Input 事件
//   D  快捷字母選單 + 狀態分支,操作與說明書一致(見 docs/CONTROLS.md):
//      B=開始新遊戲、C=繼續舊遊戲;↑↓/Enter 為輔助;Esc 返回 / Q 離開。
//
// 字型全部來自 assets(自包含:dw8x8.bin + cjk24.atlas),執行期不依賴原始遊戲檔。
//
// 用法:opendw_remake [--bundle DIR] [--font RAW] [--atlas ATLAS] [--menu TSV]
//                     [--pc N] [--sprite NAME] [--frames N] [--dump PPM] [--press CH]
#include <cstdio>
#include <cstring>
#include <cctype>
#include <optional>
#include <string>
#include <vector>
#include "resource/provider.hpp"
#include "vm/interpreter.hpp"
#include "render/font.hpp"
#include "render/cjk_font.hpp"
#include "render/framebuffer.hpp"
#include "render/sprite.hpp"
#include "render/picture.hpp"
#include "render/sdl_video.hpp"
#include "i18n/strings.hpp"
using namespace dw;

static std::vector<std::string> lines_of(const std::string& s) {
  std::vector<std::string> o; std::string c;
  for (char ch : s) { if (ch == '\r' || ch == '\n') { o.push_back(c); c.clear(); } else c.push_back(ch); }
  o.push_back(c); return o;
}

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

struct Opt { char hot; std::string label; };   // 快捷字母 + 在地化文字

int main(int argc, char** argv) {
  std::string bundle = "assets/bundle";
  std::string font_raw = "assets/fonts/dw8x8.bin";
  std::string atlas = "assets/fonts/cjk24.atlas";
  std::string menu_tsv = "assets/i18n/zh-TW/menu.tsv";
  int start_pc = 20, max_frames = -1, press = 0;
  std::string dump, sprite_name, scene_name;
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
    else if (eq("--scene") && i + 1 < argc) scene_name = argv[++i];
    else if (eq("--press") && i + 1 < argc) press = std::toupper((unsigned char)argv[++i][0]);  // 模擬按鍵(測試)
  }

  auto font = render::Font8x8::load_table(font_raw);
  if (!font) { std::fprintf(stderr, "font load failed: %s\n", font_raw.c_str()); return 1; }
  const bool scene_mode = !scene_name.empty();
  const bool sprite_mode = !sprite_name.empty();
  const bool menu_mode = !scene_mode && !sprite_mode;
  render::Framebuffer fb;

  std::optional<render::CjkFont> cjk;
  std::string header;
  std::vector<Opt> opts;
  int sel = 0;
  enum { S_MENU, S_BRANCH } state = S_MENU;
  std::string branch_label;

  if (scene_mode) {
    // ── E:全螢幕場景圖(從 bundle .pic 載解壓資料,title_adjust 去交錯)──
    std::string path = scene_name.find('/') != std::string::npos
                         ? scene_name : bundle + "/scenes/" + scene_name + ".pic";
    std::FILE* sf = std::fopen(path.c_str(), "rb");
    if (!sf) { std::fprintf(stderr, "scene open failed: %s\n", path.c_str()); return 1; }
    std::vector<std::uint8_t> data(32000);
    std::size_t n = std::fread(data.data(), 1, data.size(), sf); std::fclose(sf);
    if (n != data.size()) { std::fprintf(stderr, "scene size %zu != 32000: %s\n", n, path.c_str()); return 1; }
    render::decode_fullscreen(fb, data);
    std::fprintf(stderr, "scene %s rendered (bundle, no DATA1)\n", scene_name.c_str());
  } else if (sprite_mode) {
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
    // ── B:VM 在地化選單 → D:快捷字母選項 ──
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
      ip.run(); }
    std::string menu;
    for (auto& s : msgs) if (s.find("Begin a new game") != std::string::npos) { menu = s; break; }
    if (menu.empty()) for (auto& s : msgs) if (s.size() > menu.size()) menu = s;

    // 英文行取快捷字母(highlighted letter)+ i18n 在地化
    std::vector<std::string> en;
    for (auto& ln : lines_of(menu)) if (!ln.empty()) en.push_back(ln);
    std::size_t first_opt = 0;
    if (en.size() > 1) { header = tr->tr(en[0]); first_opt = 1; }   // 第一行為提示
    for (std::size_t i = first_opt; i < en.size(); ++i) {
      char hot = 0;
      for (char ch : en[i]) if (std::isalpha((unsigned char)ch)) { hot = std::toupper((unsigned char)ch); break; }
      opts.push_back({hot, tr->tr(en[i])});
    }
    std::fprintf(stderr, "menu: header=\"%s\" options=%zu (hotkeys:", header.c_str(), opts.size());
    for (auto& o : opts) std::fprintf(stderr, " %c", o.hot ? o.hot : '?');
    std::fprintf(stderr, ")\n");

    // --press 模擬:直接觸發對應快捷字母(headless 驗證分支)
    if (press) for (std::size_t i = 0; i < opts.size(); ++i)
      if (opts[i].hot == press) { sel = (int)i; state = S_BRANCH; branch_label = opts[i].label; }
  }

  auto draw_menu = [&]() {
    fb.clear(1);
    { int x = 8; for (std::uint32_t cp : {U'火', U'龍', U'之', U'戰'}) { cjk->draw(fb, x, 8, cp, 14); x += 24; } }
    int y = 48;
    if (!header.empty()) { draw_mixed(fb, *font, *cjk, 16, y, header, 7, 1); y += 28; }
    for (std::size_t i = 0; i < opts.size(); ++i) {
      bool cur = (int)i == sel;
      std::uint8_t col = cur ? 14 : 15;
      int x = 16;
      if (cur) { font->draw_char(fb, x, y + 8, (std::uint8_t)'>', 14, 1); }
      x = 36;
      if (opts[i].hot) {                       // 快捷字母,如 "B)"
        font->draw_char(fb, x, y + 8, (std::uint8_t)opts[i].hot, 11, 1); x += 8;
        font->draw_char(fb, x, y + 8, (std::uint8_t)')', col, 1); x += 12;
      }
      draw_mixed(fb, *font, *cjk, x, y, opts[i].label, col, 1);
      y += 28;
    }
  };
  auto draw_branch = [&]() {
    fb.clear(1);
    { int x = 8; for (std::uint32_t cp : {U'火', U'龍', U'之', U'戰'}) { cjk->draw(fb, x, 8, cp, 14); x += 24; } }
    draw_mixed(fb, *font, *cjk, 16, 70, branch_label, 14, 1);
    // 提示用 ASCII(CJK atlas 僅含遊戲在地化字,避免缺字)
    font->draw_string(fb, 16, 120, "(game screen - to be implemented)", 7, 1);
    font->draw_string(fb, 16, 156, "Esc: back   Q: quit", 8, 1);
  };
  auto render_now = [&]() { if (!menu_mode) return; (state == S_MENU) ? draw_menu() : draw_branch(); };
  render_now();

  if (!dump.empty()) {
    std::FILE* d = std::fopen(dump.c_str(), "wb");
    if (d) { fb.write_ppm(d); std::fclose(d); std::fprintf(stderr, "dumped frame to %s\n", dump.c_str()); }
  }

  render::SdlVideo vid;
  if (!vid.open(3)) { std::fprintf(stderr, "SDL open failed\n"); return 1; }
  int frames = 0;
  for (;;) {
    render_now();
    vid.present(fb);
    render::Input in = vid.poll();
    if (in.quit) break;
    if (!menu_mode) { if (in.back) break; }              // sprite 檢視:Esc/Q 離開
    else if (state == S_MENU) {
      if (in.back) break;                                // 選單按 Esc = 離開
      int n = (int)opts.size();
      if (n) {
        if (in.up) sel = (sel - 1 + n) % n;
        if (in.down) sel = (sel + 1) % n;
        int trig = in.select ? sel : -1;
        if (in.key) for (int i = 0; i < n; ++i) if (opts[i].hot == in.key) trig = i;  // 快捷字母
        if (trig >= 0) { sel = trig; state = S_BRANCH; branch_label = opts[trig].label;
                         std::fprintf(stderr, "selected [%c] %s\n", opts[trig].hot, opts[trig].label.c_str()); }
      }
    } else {                                             // S_BRANCH
      if (in.back || in.select) state = S_MENU;
    }
    if (max_frames >= 0 && ++frames >= max_frames) break;
  }
  vid.close();
  std::fprintf(stderr, "ok (frames=%d, state=%d, sel=%d)\n", frames, (int)state, sel);
  return 0;
}
