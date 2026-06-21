// sdl_video — 雙層合成顯示(像素層 + 文字層,內外解析度解耦)。
//
// 雙層架構(見 docs/adr/0002-two-layer-cjk-rendering.md):
//   像素層:320×200 indexed framebuffer → 整數倍 nearest 放大到視窗(維持像素正確性)。
//   文字層:TextLayer 用 SDL2_ttf 在視窗高解析原生繪製 CJK/ASCII,疊在像素層之上,永不縮放。
//
// Deep module:對外 open/present/dump/poll/close + 取 TextLayer&;
//   內部隱藏 SDL window/renderer/texture、palette→RGB、整數放大、文字層合成、headless 讀回。
#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <vector>
#include "framebuffer.hpp"
#include "text_layer.hpp"

struct SDL_Window;
struct SDL_Renderer;
struct SDL_Texture;

namespace dw::render {

// 一幀內收到的輸入事件(互動骨架用;鍵位對齊說明書,見 docs/engine/CONTROLS.md)。
struct Input {
  bool quit = false;     // 關窗 / Q(離開遊戲)
  bool back = false;     // Esc(離開子畫面 / 繼續訊息)
  bool up = false;       // ↑
  bool down = false;     // ↓
  bool left = false;     // ←
  bool right = false;    // →
  bool select = false;   // Enter / Space(輔助選取)
  bool pgup = false;     // PgUp(段落檢視器上翻一頁)
  bool pgdown = false;   // PgDn(段落檢視器下翻一頁)
  bool cycle_lang = false; // F4:即時循環切換語系(zh-TW → en → ja → …)
  bool help = false;       // F1:開 Help 覆蓋層(列操作鍵)
  bool cycle_theme = false;// F8:循環切換 UI 主題(UiTheme)
  bool request_quit = false;// F10:請求離開(觸發離開確認流程;非立即 quit)
  int  key = 0;          // 本幀按下的字母鍵(大寫 ASCII,如 'B'/'C'/'I'/'U'…),供快捷字母
  // ── 文字輸入(建角命名等)。一般狀態不讀這兩欄,行為不變。──
  int  text_char = 0;    // 本幀輸入的可列印字元(含小寫 / 空白,保留原始大小寫)
  bool backspace = false;// 退格(刪除一字元)
};

class SdlVideo {
public:
  ~SdlVideo();
  // headless = true:用 dummy video driver,只供 dump(不開實體視窗)。
  // ttf_path:文字層 host 字型(預設 wqy-zenhei.ttc)。空字串 = 不啟用文字層(回退)。
  bool open(int scale = 3,
            const char* title = "OpenDW Remake — 火龍之戰",
            const std::string& ttf_path = "/usr/share/fonts/truetype/wqy/wqy-zenhei.ttc",
            bool headless = false);

  // 640×480 視窗模式(方案 3,見 docs/assessment/47):視窗固定 640×480;像素層維持 320×200
  //   (byte-for-byte 不動),整數 ×2 放大成 640×400 並「垂直置中」於 640×480
  //   (上下各 40px 黑邊 letterbox,保持像素完美 + 原始 16:10 比例,絕不拉伸)。
  //   文字層字級與 scale 解綁(由呼叫端 add(...) 指定固定 px:CJK 24 / UI 16 / 標題 48);
  //   座標換算固定 ×2 並加 letterbox y 偏移(文字隨遊戲內容置中)。
  // 與一般 open 互斥:呼叫此函式即進 640×480 模式(scale 概念變成固定 2 + letterbox)。
  bool open_640x480(const char* title = "OpenDW Remake — 火龍之戰",
                    const std::string& ttf_path = "/usr/share/fonts/truetype/wqy/wqy-zenhei.ttc",
                    bool headless = false);

  TextLayer& text() { return text_; }   // 每幀:text().clear() → add(...) → present()

  // per-theme palette:切 UI 主題(F8)時呼叫,之後 present/dump 用此盤把 indexed→RGB。
  //   預設 DOS 盤;DOS 主題 set DOS 盤(回歸不變)、Amiga 主題 set Amiga 盤。
  //   set_palette() 同時關閉 256 色增強(切回 16 色路徑),確保 DOS/Amiga/X68000
  //   永遠走原本 16 色 to_rgb(golden 對拍不破)。
  void set_palette(const std::array<Rgb, 16>& pal) { active_palette_ = pal; vga256_ = false; }
  const std::array<Rgb, 16>& palette() const { return active_palette_; }

  // 「真 VGA」256 色增強模式(VGA-256 theme 專用;remake 加值)。開啟後 present/dump
  //   先把 16 色 framebuffer 經 enhance_to_256(垂直漸層 + 邊緣壓暗)轉 256 色索引,
  //   再用 vga_palette() 256 色盤 to_rgb。DOS/Amiga/X68000 不呼叫此函式 → 16 色路徑不變。
  void set_vga256(bool on) { vga256_ = on; }
  bool is_vga256() const { return vga256_; }

  // 區域 RGB 覆寫(remake 加值,「打破單盤隔閡」):set 後,下一次 present/dump 時把該矩形
  //   (320×200 像素座標)的像素以提供的 RGB 直接覆蓋(隨 texture 一起整數放大),然後自動清除。
  //   用途:Amiga 戰鬥 viewport —— 牆/UI 用主題盤、怪物用自帶盤,各自轉 RGB 後在此合成,
  //   讓「鮮豔怪物 + 土黃地牢牆」同框(原版受 16 色單盤所限,remake 不必)。rgb 長度需 = w*h。
  void set_region_rgb(std::vector<Rgb> rgb, int x, int y, int w, int h) {
    region_rgb_ = std::move(rgb);
    region_x_ = x; region_y_ = y; region_w_ = w; region_h_ = h;
  }

  void present(const Framebuffer& fb);   // 像素層放大 + 文字層合成 → 顯示
  void set_title(const std::string& title);  // 動態改視窗標題(語言 / tileset 切換時)
  Input poll();                          // 收集本幀事件(in.quit=true 表示要結束)
  void close();

  // headless 合成 dump:像素層放大 + 文字層,讀回 scale 倍高解析 RGB → 寫 PPM。
  // 回傳是否成功。(需 open(headless=true 或一般皆可))。
  bool dump_ppm(const Framebuffer& fb, const std::string& path);

  // 視窗實際輸出尺寸。640×480 模式固定 640×480;一般模式 = 320*scale × 200*scale。
  int out_w() const { return mode640_ ? kWin640W : kW * scale_; }
  int out_h() const { return mode640_ ? kWin640H : kH * scale_; }
  bool is_640x480() const { return mode640_; }

  // 驗證用:把 320×200 framebuffer→RGB(不含文字層,純像素層)讀回。
  //   不帶 palette 的多載維持 DOS 盤(既有 golden 對拍 / 呼叫端不變);帶 palette 多載供 per-theme。
  static std::vector<std::uint8_t> to_rgb(const Framebuffer& fb);
  static std::vector<std::uint8_t> to_rgb(const Framebuffer& fb,
                                          const std::array<Rgb, 16>& pal);

private:
  // 把像素層(framebuffer)+ 文字層合成到目前 renderer 目標(不 present)。
  void compose(const Framebuffer& fb);

  // 640×480 模式常數(方案 3:像素層 320×200 ×2 = 640×400,垂直置中於 640×480)。
  static constexpr int kWin640W = 640;
  static constexpr int kWin640H = 480;
  static constexpr int kMode640Scale = 2;        // 像素層整數放大倍率
  static constexpr int kMode640LetterY = (kWin640H - kH * kMode640Scale) / 2;  // = 40(上下黑邊)

  // 共用初始化(一般 / 640):建立 window/renderer/texture/文字層。
  bool init_common(int win_w, int win_h, const char* title,
                   const std::string& ttf_path, bool headless);

  SDL_Window* win_ = nullptr;
  SDL_Renderer* ren_ = nullptr;
  SDL_Texture* tex_ = nullptr;   // 320×200 streaming(像素層)
  int scale_ = 3;
  bool headless_ = false;
  bool mode640_ = false;         // true = 640×480 letterbox 模式
  std::array<Rgb, 16> active_palette_ = kDosPalette;  // 目前生效的 16 色盤(per-theme;set_palette)
  bool vga256_ = false;                              // true = 256 色增強路徑(VGA-256 theme)
  // 區域 RGB 覆寫(set_region_rgb;compose 套用後清除)。region_w_==0 = 無覆寫。
  std::vector<Rgb> region_rgb_;
  int region_x_ = 0, region_y_ = 0, region_w_ = 0, region_h_ = 0;
  TextLayer text_;
};

}  // namespace dw::render
