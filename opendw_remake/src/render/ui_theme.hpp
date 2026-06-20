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

// 結局過場場景一張(全螢幕 .pic + 對應在地化敘事字)。
//   - ref / source:同 title art 的載入規則(kDosScene = bundle/scenes/<ref>.pic、nibble 解碼套
//     theme.palette;kAmigaPic = bundle/themes/<ref>、planar 解碼 + 檔頭 palette)。
//   - narrative_en:疊在場景底部襯底條的英文敘事鍵(經 tr() 在地化為繁中/日)。空字串 =
//     此場景無疊字(如「The End」logo 自帶字,只需收尾標題)。
struct EndingScene {
  std::string ref;          // kDosScene:場景名("24");kAmigaPic:themes/ 下相對路徑
  TitleSource source = TitleSource::kDosScene;
  std::string narrative_en; // 疊字英文鍵(tr 在地化);空 = 不疊敘事
  // 「英文擦除框」(viewport 320×200 座標):原版把英文敘事烤進此塊黑底區。
  //   非英文語系時:把此框實心填黑(徹底擦掉原版英文)→ 銳利文字層在框內畫在地化敘事
  //   (對齊原版英文的位置 = 沿用 scene_localize.py 「換字不換版」的作法,取代底部字幕條)。
  //   ew==0 → 無擦除框 → 回退舊版「底部襯底條疊字幕」(Amiga 單張結局 / 末張 The End 用)。
  //   英文語系永遠走 art 原樣(英文已烤進圖,不疊任何敘事)。
  int ex = 0, ey = 0, ew = 0, eh = 0;
};

// 一個完整 UI 主題:title art 來源 + per-theme 16 色 palette + 戰鬥 backdrop + 覆蓋框配色。
struct UiTheme {
  std::string name = "dos";
  // 開機 title splash 的全螢幕 art。kDosScene 時 title_ref = bundle/scenes/<title_ref>.pic 的
  //   場景名(DOS = "29",Dragon Wars dragon art);kAmigaPic 時 title_ref = themes/ 下相對路徑
  //   (如 "amiga/title.pic")。
  std::string title_ref = "29";
  TitleSource title_source = TitleSource::kDosScene;
  // 結局過場場景序列(依序顯示;每張全螢幕 art + 在地化敘事)。空 = 此主題無原生結局過場
  //   → 呼叫端誠實回退 DOS 序列(見 ending_scenes() helper)。DOS = res 24..28(Namtar 墜淵/
  //   慘叫/焚城/和平新時代/The End);Amiga = 單張 themes/amiga/scenes/endgame.pic(planar)。
  std::vector<EndingScene> ending = {};
  // 像素層 indexed→RGB 用的 16 色盤。預設 DOS 標準盤;Amiga theme 帶自己的 palette
  //   (亦可由 kAmigaPic 檔頭覆蓋)。SdlVideo 切 theme 時套此盤(per-theme palette 打通點)。
  std::array<Rgb, 16> palette = kDosPalette;
  CombatBackdrop combat;
  OverlayStyle overlay;
  // 戰鬥怪物 sprite 來源(theme-aware combat art)。空 = 用預設 DOS bundle/sprites;
  //   非空 = bundle 下相對路徑(如 "themes/amiga/sprites"),Amiga 怪物立繪由此載。
  //   檔名沿用 DOS 命名(152_guard / 196_spider …),呼叫端只換目錄不換名。
  std::string sprite_dir;
  // Amiga sprite 自帶 16 色 palette(各怪物不同色系)→ 戰鬥載此 sprite 時須套其自帶盤
  //   (Sprite::palette),而非 theme.palette。true 時 combat 渲染前 set_palette(sprite.palette)。
  bool sprite_own_palette = false;
  // 此 sprite 來源的透明色索引(blit 時跳過)。DOS=6(encounter 棕);Amiga=8(紅底)。
  int sprite_transparent = 6;
  // 第一人稱 viewport 牆面元件來源(theme-aware dungeon art)。空 = 用 DOS bundle/components
  //   (走 render_first_person + golden 對拍路徑);非空 = bundle 下相對路徑(如
  //   "themes/amiga/components"),Amiga 地牢牆面由此載(走 render_first_person_amiga;
  //   DOS golden 不經此路徑、不破)。各 viewport 圖塊共用內嵌 stone palette。
  std::string component_dir;
  // 誠實標示:partial=true 表示此主題未完整(palette placeholder / 無原生標題 / sprite 未切)。
  //   note 為簡短英文說明(toast 顯示用,經 tr() 在地化)。
  bool partial = false;
  std::string note;
  // 「真 VGA」256 色增強(VGA-256 theme;remake 加值,原版無此版本)。
  //   true 時呼叫端(main.cpp / SdlVideo)走 256 色路徑:framebuffer 仍由各渲染器以
  //   16 色繪製,但 present/dump 前經 enhance_to_256(垂直漸層 + 邊緣壓暗)轉 256 色,
  //   套 vga_palette() 256 色盤。DOS/Amiga/X68000 此旗標為 false → 16 色路徑不變
  //   (golden 對拍不破)。palette 欄位仍保留 16 色(載 title art / set_palette 前置用)。
  bool vga256 = false;
};

// Amiga 第一人稱 viewport 配色盤(校準自真機官方截圖,誠實標示):
//   原生 Amiga viewport 圖塊的「重組落點」需逆出 Amiga 引擎 blit 錨點演算法,且官方截圖
//   無「長走廊」可作側牆遞遠的對位真值 → 原生圖塊組裝暫擱置(見 viewport_amiga.hpp / docs/61)。
//   改採有界方案:沿用 **DOS golden 精確透視幾何**(byte-for-byte 對拍那條),調色盤**校準自
//   真機 Amiga「Purgatory」起始地牢 FP 截圖**(amiga_screenshot_official/dragon-wars_5/9.png,
//   直方圖實測:土黃磚牆 136,102,0、金黃高光 203,187,0、棕縫 102,68,0、青柱)→ 透視 100%
//   收斂且配色忠於真 Amiga 起始區(玩家第一印象的土黃磚牆,非 Mines 灰)。
//   索引語意對齊 DOS viewport 實際用色(idx3/7/8/11=磚牆、idx4/12=地板天花)。
inline constexpr std::array<Rgb, 16> kAmigaViewportPalette = {{
  {0x00,0x00,0x00},  // 0 黑(遠景開口/門凹/陰影)
  {0x00,0x00,0xAA},  // 1
  {0x00,0x88,0x77},  // 2 青(柱/裝飾;真機青 0,136,119)
  {0x88,0x66,0x00},  // 3 土黃磚牆主面(原 DOS teal → 真機 136,102,0)
  {0x66,0x44,0x00},  // 4 棕地板/天花(原 DOS 紅 → 真機磚縫棕 102,68,0)
  {0xAA,0x00,0xAA},  // 5
  {0x66,0x44,0x00},  // 6 棕門/縫
  {0x99,0x77,0x00},  // 7 亮土黃磚(原 DOS 亮灰 → 真機 153,119,0)
  {0x66,0x44,0x00},  // 8 暗棕磚縫(原 DOS 暗灰 → 真機 102,68,0)
  {0x55,0x55,0xFF},  // 9
  {0x00,0x88,0x77},  // 10 青
  {0xCB,0xBB,0x00},  // 11 金黃磚高光(原 DOS 亮青 → 真機 203,187,0)
  {0x99,0x77,0x00},  // 12 亮土黃地板(原 DOS 亮紅 → 真機 153,119,0)
  {0xFF,0x55,0xFF},  // 13
  {0xCB,0xBB,0x00},  // 14 金黃高光細節(真機 204,188,0)
  {0xFF,0xFF,0xFF}}};// 15 白(邊緣高光)

// 結局過場各場景的英文敘事鍵(經 tr() 在地化 → 繁中/日;查無回退英文)。
//   鍵文字與原版場景烤進點陣圖的英文一致,已於 i18n events.tsv 提供繁中譯文。
//   集中為具名常數供 theme 結局序列與測試共用,避免散落多處。
inline constexpr const char* kEndingNarr24 =
    "And with a mighty heave, Namtar is hurled back into the pit from whence he came...";
inline constexpr const char* kEndingNarr25 =
    "Namtar lets out a horrid scream as he falls into the deepest depths of the Magan "
    "Underworld. His reign of terror is at an end!";
inline constexpr const char* kEndingNarr26 =
    "News of Namtar's destruction brings untold joy to the prisoners of the city of "
    "Purgatory. The evil city is overrun and burned to the ground by its own citizens "
    "as they once again possess the freedom that was taken from them by Namtar.";
inline constexpr const char* kEndingNarr27 =
    "A new age of peace begins, the land is reborn with life and beauty. The dragons "
    "leave the cities and return to their mystic valley to await a time that they may be "
    "needed again...";

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
    // [0] DOS(預設;完整)。結局過場 = res 24..28 五張 DOS 全螢幕場景,各配在地化敘事。
    //   敘事鍵與場景烤進的原版英文一致(已於 i18n events.tsv 在地化),底部襯底條疊繁中。
    UiTheme dos;
    // 擦除框 = 原版英文敘事烤進的黑底區(座標沿用 tools_build/scene_localize.py 實測):
    //   24 英文在右側、25/26/27 在下方。非英文語系填黑該框 + 框內畫在地化敘事(取代字幕條)。
    dos.ending = {
      {"24", TitleSource::kDosScene, kEndingNarr24, 196, 30, 124, 150},
      {"25", TitleSource::kDosScene, kEndingNarr25,   0, 140, 320,  60},
      {"26", TitleSource::kDosScene, kEndingNarr26,   0, 128, 320,  72},
      {"27", TitleSource::kDosScene, kEndingNarr27,   0, 116, 320,  84},
      // The End logo 自帶英文立繪;narrative 空 → 只疊在地化收尾標題(全劇終)。
      {"28", TitleSource::kDosScene, ""},
    };
    v.push_back(dos);

    // [1] Amiga(完整):原生金龍標題 + 自己的 Amiga 16 色 palette。
    UiTheme amiga;
    amiga.name = "amiga";
    amiga.title_ref = "amiga/title.pic";       // themes/ 下相對路徑(kAmigaPic)
    amiga.title_source = TitleSource::kAmigaPic;
    amiga.palette = kAmigaPalette;
    // Amiga 色系下的戰鬥 backdrop / overlay(沿用結構,色索引仍 0–15,套 Amiga 盤後呈現 Amiga 色)。
    amiga.combat = CombatBackdrop{};
    amiga.overlay = OverlayStyle{};
    // Amiga 結局過場:原生單張全螢幕 endgame(planar,自帶 palette)。Amiga 版把整段
    //   結局繪成一張圖,故 ending 只一張;敘事字仍疊在地化收尾標題(narrative 沿用 24 鍵
    //   給可讀的繁中導言,The End 感由 art 本身呈現)。
    amiga.ending = {
      {"amiga/scenes/endgame.pic", TitleSource::kAmigaPic, kEndingNarr24},
    };
    // Amiga 原生怪物 sprite(data4 4-bitplane 逆向 → .spr,各帶自帶 Amiga palette)。
    //   戰鬥載 themes/amiga/sprites/<name>.spr,套 sprite 自帶盤,透明色 = 8(紅底)。
    amiga.sprite_dir = "themes/amiga/sprites";
    amiga.sprite_own_palette = true;
    amiga.sprite_transparent = 8;
    // Amiga 第一人稱地牢牆面(data3 viewport 圖塊逆向 → .spr;見 viewport_amiga.hpp）。
    //   F8 切 Amiga 走 render_first_person_amiga;DOS 仍走 golden 路徑(不破對拍)。
    amiga.component_dir = "themes/amiga/components";
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

    // [3] VGA-256(remake 加值;原版無此版本)。256 色「真 VGA」增強主題:
    //   以 DOS EGA 16 色畫面為來源,present/dump 前經 enhance_to_256(垂直漸層 +
    //   邊緣壓暗)演算法化擴成 256 色更精緻版本(見 render/vga256.hpp)。
    //   - 16 色 base palette 仍用 DOS 盤(title art / set_palette 前置;實際畫面套 256 色盤)。
    //   - title 沿用 DOS dragon art(無原生 VGA 標題,誠實回退)。
    //   - 結局過場沿用 DOS 序列(theme_ending_scenes 回退),同樣經 256 色增強呈現。
    //   - partial=false:此主題的「256 色增強」是完整、確定性的演算法 pass(非未完成移植);
    //     但誠實標示為「演算法增強(漸層/色深擴展/柔邊),非逐像素手繪重畫」(見 vga256.hpp)。
    UiTheme vga;
    vga.name = "vga";
    vga.title_ref = "29";                      // 回退 DOS dragon art(無原生 VGA 標題)
    vga.title_source = TitleSource::kDosScene;
    vga.palette = kDosPalette;                 // 16 色 base(實際畫面由 enhance_to_256 → 256 色)
    vga.vga256 = true;                         // 開啟 256 色增強路徑
    vga.combat = CombatBackdrop{};
    vga.overlay = OverlayStyle{};
    v.push_back(vga);
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

// 取一個主題的結局過場序列;此主題無原生結局(ending 空,如未來新平台)時
//   誠實回退 DOS 完整 5 張序列(default_theme().ending)。narrow interface:呼叫端
//   (main.cpp)只問「這主題的結局該放哪幾張」,不需理解回退從何而來。
inline const std::vector<EndingScene>& theme_ending_scenes(const UiTheme& t) {
  if (!t.ending.empty()) return t.ending;
  return default_theme().ending;   // DOS 24..28 完整序列(回退)
}

}  // namespace dw::render
