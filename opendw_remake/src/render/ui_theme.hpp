// ui_theme — UI 主題抽象(theme-aware presentation)。
//
// 目的:把「介面外觀會隨平台版本(DOS / Amiga / X68000)不同」的選擇收斂到單一
//   narrow interface,讓 title art / per-theme 16 色 palette / 戰鬥 backdrop /
//   訊息框配色等可整批切換(F8 循環)。新增平台只需多一個 UiTheme 實例 + 對應資產,
//   呼叫端(main.cpp)不需改動。
//
// 主題完整度(誠實標示;見 partial / note):
//   DOS    — 完整:原生 dragon art(res29)+ DOS 16 色標準盤。
//   Amiga  — 完整:原生金龍標題 + 結局,各帶自己的 16 色 Amiga palette(themes/amiga/*.pic)。
//   X68000 — 部分:目前只有怪物/場景 contact sheet(未切),palette 為 DOS placeholder,
//            無原生標題 → title 回退 DOS res29。toast/註解標清楚。
//
// Deep module:對外只露 UiTheme(一組具名的呈現參數)+ default_theme()/theme_by_name();
//   呼叫端拿 const UiTheme& 取值,不需理解各平台差異從何而來。
#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "framebuffer.hpp"   // Rgb / kDosPalette(per-theme palette 預設)

namespace dw::render {

// 半透明覆蓋框(訊息框 / 段落框)在 indexed 像素層上的呈現參數。
//   indexed framebuffer 無原生 alpha → 用「棋盤式 dithering(底色 vs 框色交錯)」模擬半透明感:
//   dither=true 時,框內以 (x+y) 奇偶交錯填 base / 讓底下畫面像素透出,形成網點透視。
struct OverlayStyle {
  std::uint8_t base = 1;     // 框底主色(DOS 深藍)
  std::uint8_t border = 15;  // 邊框色(白)
  std::uint8_t border2 = 9;  // 內層雙線框色(亮藍;優雅雙框用)
  std::uint8_t accent = 14;  // 標題 / 提示強調色(黃)
  bool dither = true;        // true = 半透明網點(底下畫面透出);false = 實心底(舊行為)
};

// 戰鬥畫面 viewport 內的 backdrop(怪物立繪後方背景)。
//   DOS:上半天空(cyan)+ 下半地面(brick red 點綴),取代純黑。
struct CombatBackdrop {
  std::uint8_t sky = 3;       // viewport 上半(天空;DOS cyan=3)
  std::uint8_t ground = 4;    // viewport 下半(地面;DOS brick red=4)
  std::uint8_t ground_dot = 6;// 地面點綴色(棕,做石礫質感)
  int horizon = 70;           // 天空/地面分界(viewport 內相對 y,0..136)
};

// title art 載入來源。引擎依此決定用哪種 decode + 套哪個 palette。
enum class TitleSource {
  kDosScene,    // DOS res .pic:decode_fullscreen(title_adjust + nibble),套 theme.palette(= DOS 盤)
  kAmigaPic,    // Amiga .pic:decode_amiga_planar,palette 讀自檔頭(覆蓋 theme.palette)
};

// 一個完整 UI 主題:title art 來源 + per-theme 16 色 palette + 戰鬥 backdrop + 覆蓋框配色。
struct UiTheme {
  std::string name = "dos";
  // 開機 title splash 的全螢幕 art。kDosScene 時 title_ref = bundle/scenes/<title_ref>.pic 的
  //   場景名(DOS = "29",Dragon Wars dragon art);kAmigaPic 時 title_ref = themes/ 下相對路徑
  //   (如 "amiga/title.pic")。
  std::string title_ref = "29";
  TitleSource title_source = TitleSource::kDosScene;
  // 像素層 indexed→RGB 用的 16 色盤。預設 DOS 標準盤;Amiga theme 帶自己的 palette
  //   (亦可由 kAmigaPic 檔頭覆蓋)。SdlVideo 切 theme 時套此盤(per-theme palette 打通點)。
  std::array<Rgb, 16> palette = kDosPalette;
  CombatBackdrop combat;
  OverlayStyle overlay;
  // 誠實標示:partial=true 表示此主題未完整(palette placeholder / 無原生標題 / sprite 未切)。
  //   note 為簡短英文說明(toast 顯示用,經 tr() 在地化)。
  bool partial = false;
  std::string note;
};

// 所有可循環的 UI 主題(F8 依序:DOS → Amiga → X68000 → DOS)。
//   新增平台版本時在此追加一個 UiTheme(各自指定 title art / palette / backdrop / overlay),
//   F8 循環機制與呼叫端皆不需改(narrow interface 不變)。
inline const std::vector<UiTheme>& theme_list() {
  // Amiga 16 色 palette(themes/amiga/title.pic 檔頭,0x0RGB ×17 → 8-bit;見 amiga_pic_extract.py)。
  //   word[1]=白 word[2]=金龍。此處硬寫一份作為「載 art 前」的預設盤,實際載 kAmigaPic 時
  //   會用檔頭 palette 覆蓋(兩者一致),確保即使 art 載失敗,Amiga 主題仍呈現自己的色系。
  static const std::array<Rgb, 16> kAmigaPalette = {{
    {0x00,0x00,0x00},{0xFF,0xFF,0xFF},{0xFF,0x55,0x99},{0xFF,0x22,0x88},
    {0xDD,0x11,0x55},{0xBB,0x11,0x33},{0x99,0x11,0x22},{0x77,0x11,0x11},
    {0xFF,0xDD,0x44},{0xDD,0xBB,0x33},{0xBB,0x99,0x22},{0xAA,0x77,0x11},
    {0x88,0x55,0x00},{0x66,0x44,0x00},{0x66,0x66,0x88},{0x99,0x99,0xBB}}};

  static const std::vector<UiTheme> ts = [] {
    std::vector<UiTheme> v;
    // [0] DOS(預設;完整)。
    v.push_back(UiTheme{});

    // [1] Amiga(完整):原生金龍標題 + 自己的 Amiga 16 色 palette。
    UiTheme amiga;
    amiga.name = "amiga";
    amiga.title_ref = "amiga/title.pic";       // themes/ 下相對路徑(kAmigaPic)
    amiga.title_source = TitleSource::kAmigaPic;
    amiga.palette = kAmigaPalette;
    // Amiga 色系下的戰鬥 backdrop / overlay(沿用結構,色索引仍 0–15,套 Amiga 盤後呈現 Amiga 色)。
    amiga.combat = CombatBackdrop{};
    amiga.overlay = OverlayStyle{};
    v.push_back(amiga);

    // [2] X68000(部分):無原生標題 → 回退 DOS res29;palette 暫用 DOS placeholder;
    //   sprite 為 contact sheet 未切。誠實標示 partial。
    UiTheme x68000;
    x68000.name = "x68000";
    x68000.title_ref = "29";                   // 回退 DOS dragon art
    x68000.title_source = TitleSource::kDosScene;
    x68000.palette = kDosPalette;              // placeholder(原生 X68000 盤尚未萃取)
    x68000.partial = true;
    x68000.note = "partial: DOS-palette placeholder, DOS title fallback";
    v.push_back(x68000);
    return v;
  }();
  return ts;
}

// 主題總數(F8 循環用)。
inline int theme_count() { return (int)theme_list().size(); }

// 依索引取主題(自動 wrap 到合法範圍)。
inline const UiTheme& theme_by_index(int idx) {
  const auto& ts = theme_list();
  int n = (int)ts.size();
  if (n <= 0) { static const UiTheme fallback{}; return fallback; }
  return ts[((idx % n) + n) % n];
}

// 預設主題(DOS;索引 0)。
inline const UiTheme& default_theme() { return theme_by_index(0); }

// 依名稱取主題;未知名稱回退 default_theme()。
inline const UiTheme& theme_by_name(const std::string& name) {
  for (const auto& t : theme_list())
    if (t.name == name) return t;
  return default_theme();
}

}  // namespace dw::render
