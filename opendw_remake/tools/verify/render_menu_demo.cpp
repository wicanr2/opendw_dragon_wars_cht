// render_menu_demo — 端到端:remake 用自身 text_codec 從 DATA1 解出 section 0 選單字串,
// 渲染英文版;再經 i18n 換成中文渲染(CJK)。輸出 en/zh 兩張 PPM。
#include <cstdio>
#include <string>
#include <vector>
#include "../../src/resource/archive.hpp"
#include "../../src/resource/text_codec.hpp"
#include "../../src/render/font.hpp"
#include "../../src/render/cjk_font.hpp"
#include "../../src/i18n/strings.hpp"
using namespace dw;

static std::vector<std::string> split_lines(const std::string& s) {
  std::vector<std::string> out; std::string cur;
  for (char c : s) { if (c == '\r' || c == '\n') { out.push_back(cur); cur.clear(); } else cur.push_back(c); }
  out.push_back(cur);
  return out;
}

int main(int argc, char** argv) {
  if (argc < 5) { std::fprintf(stderr, "usage: %s <data_dir> <dragon.com> <atlas> <menu.tsv>\n", argv[0]); return 2; }
  auto arc = res::Archive::open(argv[1]);
  auto font = render::Font8x8::load(argv[2]);
  auto cjk = render::CjkFont::load(argv[3]);
  auto i18n = i18n::Strings::load(argv[4]);
  if (!arc || !font || !cjk || !i18n) { std::fprintf(stderr, "load failed\n"); return 1; }

  auto sec0 = arc->load(0);
  if (!sec0) { std::fprintf(stderr, "load section 0 failed\n"); return 1; }
  // 主選單字串在 section 0 op_78 @0x14 之後(0x15)。
  auto [menu, next] = text::decode(*sec0, 0x15);
  (void)next;
  auto lines = split_lines(menu);

  // 英文版(8×8)
  render::Framebuffer en; en.clear(1);
  font->draw_string(en, 8, 8, "= Dragon Wars =", 14, 1);
  int y = 32;
  for (auto& ln : lines) { if (!ln.empty()) font->draw_string(en, 16, y, ln.c_str(), 15, 1); y += 12; }
  { std::FILE* f = std::fopen("menu_en.ppm", "wb"); en.write_ppm(f); std::fclose(f); }

  // 中文版(i18n → CJK 24×24)
  render::Framebuffer zh; zh.clear(1);
  zh.put(0, 0, 0);
  { int x = 8; for (std::uint32_t cp : {U'火', U'龍', U'之', U'戰'}) { cjk->draw(zh, x, 8, cp, 14); x += 24; } }
  y = 44;
  for (auto& ln : lines) {
    if (ln.empty()) { y += 12; continue; }
    std::string zhs = i18n->tr(ln);
    const char* p = zhs.c_str();
    int x = 16;
    while (*p) {
      std::uint32_t cp = render::utf8_next(p);
      if (cp < 0x80) { font->draw_char(zh, x, y + 8, (std::uint8_t)cp, 15, 1); x += 8; }
      else { cjk->draw(zh, x, y, cp, 15); x += 24; }
    }
    y += 28;
  }
  { std::FILE* f = std::fopen("menu_zh.ppm", "wb"); zh.write_ppm(f); std::fclose(f); }

  std::fprintf(stderr, "menu decoded: \"%s\"\n", menu.c_str());
  return 0;
}
