// render_endgame — 端到端閉環:載 area 的 level bytecode(bundle,不碰 DATA1)→
//   跑指定 tile 的事件 script → VM emit 英文字串 → i18n(events.tsv)換繁中 →
//   24×24 CJK atlas 渲染進 320×200 framebuffer → 輸出 PPM。
//   證明「結局/quest gate 事件 → 跑 VM → 繁中顯示」。
//
// 用法: render_endgame <bundle> <dw8x8.bin> <cjk24.atlas> <events.tsv> <area> <tile> <out.ppm>
#include <cstdio>
#include <cstdlib>
#include <optional>
#include <string>
#include <vector>
#include "../../src/resource/level.hpp"
#include "../../src/resource/provider.hpp"
#include "../../src/vm/interpreter.hpp"
#include "../../src/render/font.hpp"
#include "../../src/render/cjk_font.hpp"
#include "../../src/render/framebuffer.hpp"
#include "../../src/i18n/strings.hpp"
using namespace dw;

// 把一段繁中(可含 ASCII)以 24×24 中文 / 8×8 ASCII 混排、自動換行畫進 fb。
static void draw_wrapped(render::Framebuffer& fb, const render::Font8x8& a8,
                         const render::CjkFont& cjk, int x0, int y0,
                         const std::string& s, std::uint8_t color) {
  int x = x0, y = y0;
  const int right = render::kW - 4, line_h = 26;
  const char* p = s.c_str();
  while (*p) {
    std::uint32_t cp = render::utf8_next(p);
    if (cp == '\n') { x = x0; y += line_h; continue; }
    if (cp < 0x80) {
      if (x + 8 > right) { x = x0; y += line_h; }
      a8.draw_char(fb, x, y + 12, (std::uint8_t)cp, color, 0); x += 8;
    } else {
      if (x + 24 > right) { x = x0; y += line_h; }
      cjk.draw(fb, x, y, cp, color); x += 24;
    }
    if (y + 24 > render::kH) break;
  }
}

int main(int argc, char** argv) {
  if (argc < 8) {
    std::fprintf(stderr,
      "usage: %s <bundle> <dw8x8.bin> <cjk24.atlas> <events.tsv> <area> <tile> <out.ppm>\n", argv[0]);
    return 2;
  }
  std::string bundle = argv[1];
  auto a8 = render::Font8x8::load_table(argv[2]);
  auto cjk = render::CjkFont::load(argv[3]);
  auto tr = i18n::Strings::load(argv[4]);
  if (!a8 || !cjk || !tr) { std::fprintf(stderr, "font/atlas/i18n load failed\n"); return 1; }
  int area = (int)std::strtol(argv[5], nullptr, 0);
  int tile = (int)std::strtol(argv[6], nullptr, 0);

  res::BundleProvider prov(bundle);
  auto lvl = res::Level::load_file(bundle + "/maps/" + std::to_string(area) + ".lvl");
  if (!lvl) { std::fprintf(stderr, "area %d load fail\n", area); return 1; }
  int level_res = area + 0x46;
  std::uint16_t pc = lvl->script_pc((std::uint8_t)tile);
  if (pc == 0 || pc >= lvl->data().size()) { std::fprintf(stderr, "tile 0x%02X bad pc\n", tile); return 1; }

  vm::VmState st;
  st.script = lvl->data(); st.data_bytes = lvl->data();
  st.script_res = level_res; st.data_res = level_res; st.pc = pc;
  st.game_state[2] = (std::uint8_t)area; st.game_state[0x57] = (std::uint8_t)area;
  st.resource_provider = [&](int tag) -> std::optional<std::vector<std::uint8_t>> {
    if (tag == level_res) return lvl->data();
    return prov.load(tag);
  };
  std::vector<std::string> msgs;
  vm::Interpreter ip(st);
  ip.set_message_sink([&](std::size_t, const std::string& s) { if (!s.empty()) msgs.push_back(s); });
  ip.run(200000);

  // 取第一條「實質敘述字串」(略過 number-sink / Begin-new-game 雜訊)。
  std::string en, zh;
  for (auto& m : msgs) {
    if (m == "?" || m.find("egin a new game") != std::string::npos) continue;
    en = m; zh = tr->tr(m); break;
  }
  if (en.empty()) { std::fprintf(stderr, "area %d tile 0x%02X: no narrative emit\n", area, tile); return 1; }

  render::Framebuffer fb; fb.clear(0);  // 黑底
  // 標題列:火龍之戰 + area/tile
  { int x = 8; for (std::uint32_t cp : {U'火', U'龍', U'之', U'戰'}) { cjk->draw(fb, x, 6, cp, 14); x += 24; } }
  char hdr[64]; std::snprintf(hdr, sizeof hdr, "  area %d  tile 0x%02X", area, tile);
  a8->draw_string(fb, 112, 14, hdr, 6, 0);
  // 訊息區:繁中譯文(白)
  draw_wrapped(fb, *a8, *cjk, 8, 44, zh, 15);

  std::FILE* f = std::fopen(argv[7], "wb");
  if (!f) { std::fprintf(stderr, "open %s fail\n", argv[7]); return 1; }
  fb.write_ppm(f); std::fclose(f);
  std::fprintf(stderr, "area %d tile 0x%02X\n  EN: %s\n  ZH: %s\n  -> %s\n",
               area, tile, en.c_str(), zh.c_str(), argv[7]);
  return 0;
}
