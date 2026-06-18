// verify_theme — UI 主題接線(theme wiring)不變式驗證。
//
// 驗:
//   1) theme_list 三主題(dos / amiga / x68000),F8 循環索引正確 wrap。
//   2) 各主題 palette 正確(DOS = 標準盤;Amiga ≠ DOS;X68000 = DOS placeholder)。
//   3) 誠實標示:amiga partial=false、x68000 partial=true(+ note 非空)。
//   4) Amiga title.pic(themes/amiga/title.pic)read_amiga_palette + decode_amiga_planar:
//      檔頭 palette[1]=白、palette[2]=金龍金;解碼後 framebuffer 用到多個索引(非全 0)。
//
// 用法:verify_theme <bundle_dir>  (bundle = assets/bundle)
#include <array>
#include <cstdio>
#include <cstdlib>
#include <span>
#include <string>
#include <vector>

#include "render/framebuffer.hpp"
#include "render/picture.hpp"
#include "render/sprite.hpp"
#include "render/ui_theme.hpp"

using namespace dw::render;

static int fails = 0;
static void check(bool ok, const char* what) {
  std::fprintf(stderr, "[%s] %s\n", ok ? "PASS" : "FAIL", what);
  if (!ok) ++fails;
}

int main(int argc, char** argv) {
  const std::string bundle = (argc > 1) ? argv[1] : "assets/bundle";

  // 1) 四主題 + F8 循環(dos / amiga / x68000 / vga)。
  check(theme_count() == 4, "theme_count == 4 (dos/amiga/x68000/vga)");
  check(theme_by_index(0).name == "dos", "theme[0] == dos");
  check(theme_by_index(1).name == "amiga", "theme[1] == amiga");
  check(theme_by_index(2).name == "x68000", "theme[2] == x68000");
  check(theme_by_index(3).name == "vga", "theme[3] == vga");
  check(theme_by_index(4).name == "dos", "F8 wrap: index 4 -> dos");
  check(theme_by_index(-1).name == "vga", "negative wrap: index -1 -> vga");

  // 2) per-theme palette。
  check(theme_by_index(0).palette == kDosPalette, "dos palette == kDosPalette");
  check(theme_by_index(1).palette != kDosPalette, "amiga palette != DOS (own 16-colour set)");
  check(theme_by_index(2).palette == kDosPalette, "x68000 palette == DOS placeholder");
  check(theme_by_index(3).palette == kDosPalette, "vga 16-colour base == DOS (256 色由 enhance pass 生成)");

  // 3) 誠實標示 + VGA-256 旗標。
  check(theme_by_index(1).partial == false, "amiga is full (partial == false)");
  check(theme_by_index(2).partial == true, "x68000 is partial (partial == true)");
  check(!theme_by_index(2).note.empty(), "x68000 has honest note");
  check(theme_by_index(0).vga256 == false, "dos vga256 == false (16-colour path)");
  check(theme_by_index(1).vga256 == false, "amiga vga256 == false (16-colour path)");
  check(theme_by_index(2).vga256 == false, "x68000 vga256 == false (16-colour path)");
  check(theme_by_index(3).vga256 == true,  "vga vga256 == true (256-colour enhance path)");
  check(theme_by_index(3).partial == false, "vga not partial (deterministic algorithmic enhancement)");

  // 4) Amiga title.pic 解碼。
  const std::string tpath = bundle + "/themes/amiga/title.pic";
  std::FILE* f = std::fopen(tpath.c_str(), "rb");
  check(f != nullptr, "open themes/amiga/title.pic");
  if (f) {
    std::vector<std::uint8_t> data;
    std::uint8_t buf[4096];
    std::size_t n;
    while ((n = std::fread(buf, 1, sizeof buf, f)) > 0)
      data.insert(data.end(), buf, buf + n);
    std::fclose(f);
    check(data.size() >= 32 + 32000, "title.pic >= 32B palette + 32000B planar");

    auto pal = read_amiga_palette(data);
    // 檔頭 word[1]=0x0fff 白;word[2]=0x0f59 金(R=0xF*17=255,G=0x5*17=85,B=0x9*17=153)。
    check(pal[1].r == 255 && pal[1].g == 255 && pal[1].b == 255, "amiga pal[1] == white");
    check(pal[2].r == 255 && pal[2].g == 85 && pal[2].b == 153, "amiga pal[2] == dragon gold");
    check(pal[0].r == 0 && pal[0].g == 0 && pal[0].b == 0, "amiga pal[0] == black");

    Framebuffer fb;
    decode_amiga_planar(fb, data);
    // 解碼後應用到多個索引(金龍標題非空白):至少看到 0(底)、金龍/白等。
    std::array<bool, 16> seen{};
    for (std::uint8_t i : fb.idx) seen[i & 0x0F] = true;
    int used = 0;
    for (bool s : seen) if (s) ++used;
    check(used >= 4, "amiga title decode uses >= 4 colour indices (non-blank art)");
    check(seen[2], "amiga title uses index 2 (golden dragon present)");
  }

  // 5) Amiga theme 戰鬥 sprite 接線(theme-aware combat art)。
  const auto& am = theme_by_index(1);
  check(am.sprite_dir == "themes/amiga/sprites", "amiga sprite_dir == themes/amiga/sprites");
  check(am.sprite_own_palette == true, "amiga sprite_own_palette == true");
  check(am.sprite_transparent == 8, "amiga sprite transparent index == 8 (red bg)");
  check(theme_by_index(0).sprite_dir.empty(), "dos sprite_dir empty (= bundle/sprites)");
  check(theme_by_index(0).sprite_transparent == 6, "dos transparent index == 6 (brown bg)");

  // 6) Amiga 怪物 sprite(逆向 data4 4-bitplane → .spr,各帶自帶 16 色 Amiga palette)。
  //    驗:可載、寬高合理、自帶 16 色盤(且 ≠ DOS,= 各怪物自有色系)。
  //    red_bg=true 的 5 隻為標準紅底 sprite(pal[8]=紅=透明色 8);200_innocent_man 為
  //    例外(自帶盤 bg=黑、自動裁切只取到單一動畫格,誠實標示 partial,不驗 pal[8])。
  struct { const char* file; int w; int h; bool red_bg; } amspr[] = {
      {"196_spider", 94, 63, true}, {"168_wolf", 83, 112, true},
      {"222_fanatic", 67, 132, true}, {"152_guard", 88, 112, true},
      {"210_pikeman", 93, 137, true}, {"200_innocent_man", 33, 104, false},
  };
  for (auto& s : amspr) {
    const std::string p = bundle + "/themes/amiga/sprites/" + s.file + ".spr";
    auto sp = Sprite::load(p);
    char msg[128];
    std::snprintf(msg, sizeof msg, "amiga sprite %s loads", s.file);
    check(sp.has_value(), msg);
    if (sp) {
      std::snprintf(msg, sizeof msg, "amiga sprite %s dims %dx%d", s.file, s.w, s.h);
      check(sp->w == s.w && sp->h == s.h, msg);
      std::snprintf(msg, sizeof msg, "amiga sprite %s has 16-colour self palette", s.file);
      check(sp->palette.size() == 16, msg);
      if (s.red_bg) {  // 標準紅底:index 8 = 紅(透明色)。
        std::snprintf(msg, sizeof msg, "amiga sprite %s pal[8] == red (0xF00)", s.file);
        check(sp->palette[8].r == 255 && sp->palette[8].g == 0 && sp->palette[8].b == 0, msg);
      }
    }
  }

  std::fprintf(stderr, "verify_theme: %s (%d failure(s))\n", fails ? "FAIL" : "PASS", fails);
  return fails ? 1 : 0;
}
