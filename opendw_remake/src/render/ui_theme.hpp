// ui_theme — UI 主題抽象(theme-aware presentation)。
//
// 目的:把「介面外觀會隨平台版本(DOS / PC-98 / Amiga / X68000)不同」的選擇
//   收斂到單一 narrow interface,讓 title art / 戰鬥 backdrop / 訊息框配色等可整批切換。
//   目前只實作 DOS 主題(唯一有萃取資產者),但抽象先留好,未來新增平台只需多一個
//   UiTheme 實例 + 對應資產,呼叫端(main.cpp)不需改動。
//
// Deep module:對外只露 UiTheme(一組具名的呈現參數)+ default_theme()/theme_by_name();
//   呼叫端拿 const UiTheme& 取值,不需理解各平台差異從何而來。
#pragma once

#include <cstdint>
#include <string>

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

// 一個完整 UI 主題:title art 來源 + 戰鬥 backdrop + 覆蓋框配色。
struct UiTheme {
  std::string name = "dos";
  // 開機 title splash 的全螢幕 art 場景名(bundle/scenes/<scene>.pic)。
  //   DOS = res 29(Dragon Wars dragon art)。未來 PC-98/Amiga 各自指向不同 scene。
  std::string title_scene = "29";
  CombatBackdrop combat;
  OverlayStyle overlay;
};

// 預設主題(DOS;目前唯一有資產者)。
inline const UiTheme& default_theme() {
  static const UiTheme t{};
  return t;
}

// 依名稱取主題;未知名稱回退 default_theme()。
//   現只有 "dos";新增平台時在此擴充(narrow interface 不變)。
inline const UiTheme& theme_by_name(const std::string& name) {
  // 目前單一主題。保留 switch 點供未來擴充。
  (void)name;
  return default_theme();
}

}  // namespace dw::render
