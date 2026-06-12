// render_cjk_demo — 在 320×200 framebuffer 混排 8×8 ASCII + 24×24 中文,
// demo 在地化文字渲染。輸出 PPM。
#include <cstdio>
#include "../../src/render/font.hpp"
#include "../../src/render/cjk_font.hpp"
using namespace dw::render;

// 混排一行 UTF-8:ASCII 用 8×8,中文用 24×24(中文 24px 高,ASCII 對齊其下緣)。
static void mixed_line(Framebuffer& fb, const Font8x8& a, const CjkFont& c,
                       int px, int py, const char* s, std::uint8_t fg) {
  int x = px;
  while (*s) {
    const char* before = s;
    std::uint32_t cp = utf8_next(s);
    if (cp < 0x80) { a.draw_char(fb, x, py + 8, static_cast<std::uint8_t>(cp), fg, 0); x += 8; }
    else { c.draw(fb, x, py, cp, fg); x += 24; (void)before; }
  }
}

int main(int argc, char** argv) {
  if (argc < 3) { std::fprintf(stderr, "usage: %s <dragon.com> <cjk24.atlas> [out.ppm]\n", argv[0]); return 2; }
  auto a = Font8x8::load(argv[1]);
  auto c = CjkFont::load(argv[2]);
  if (!a || !c) { std::fprintf(stderr, "load font/atlas failed\n"); return 1; }
  Framebuffer fb; fb.clear(0);  // 黑底

  // 標題(中文 24×24,黃)
  c->draw(fb, 8,   8, U'火', 14); c->draw(fb, 32,  8, U'龍', 14);
  c->draw(fb, 56,  8, U'之', 14); c->draw(fb, 80,  8, U'戰', 14);
  a->draw_string(fb, 112, 16, "  Dragon Wars", 6, 0);

  // 對話(中文,白)
  const char* dlg = "你正站在競技場大門前";
  { int x = 8; for (const char* s = dlg; *s; ) { std::uint32_t cp = utf8_next(s); c->draw(fb, x, 44, cp, 15); x += 24; } }

  // 地名(中文,亮青)
  const char* place = "波卡城 罪惡之城";
  { int x = 8; for (const char* s = place; *s; ) { std::uint32_t cp = utf8_next(s); if (cp >= 0x80) { c->draw(fb, x, 80, cp, 11); x += 24; } else x += 12; } }

  // 混排:Read Paragraph(ASCII 8×8 紅 + 中文 24×24 綠)
  a->draw_string(fb, 8, 120, "Read paragraph 137:", 12, 0);
  { int x = 8; const char* p = "段落一三七 瑪根地底世界"; for (const char* s = p; *s; ) { std::uint32_t cp = utf8_next(s); if (cp >= 0x80) { c->draw(fb, x, 136, cp, 10); x += 24; } else x += 12; } }

  // UI 術語(中文,亮灰)
  const char* ui = "生命 法力 等級";
  { int x = 8; for (const char* s = ui; *s; ) { std::uint32_t cp = utf8_next(s); if (cp >= 0x80) { c->draw(fb, x, 168, cp, 7); x += 24; } else x += 12; } }

  const char* out = argc > 3 ? argv[3] : "cjk_demo.ppm";
  std::FILE* f = std::fopen(out, "wb"); fb.write_ppm(f); std::fclose(f);
  (void)mixed_line;
  return 0;
}
