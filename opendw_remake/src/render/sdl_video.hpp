// sdl_video — 雙層合成顯示(像素層 + 文字層,內外解析度解耦)。
//
// 雙層架構(見 docs/adr/0002-two-layer-cjk-rendering.md):
//   像素層:320×200 indexed framebuffer → 整數倍 nearest 放大到視窗(維持像素正確性)。
//   文字層:TextLayer 用 SDL2_ttf 在視窗高解析原生繪製 CJK/ASCII,疊在像素層之上,永不縮放。
//
// Deep module:對外 open/present/dump/poll/close + 取 TextLayer&;
//   內部隱藏 SDL window/renderer/texture、palette→RGB、整數放大、文字層合成、headless 讀回。
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "framebuffer.hpp"
#include "text_layer.hpp"

struct SDL_Window;
struct SDL_Renderer;
struct SDL_Texture;

namespace dw::render {

// 一幀內收到的輸入事件(互動骨架用;鍵位對齊說明書,見 docs/CONTROLS.md)。
struct Input {
  bool quit = false;     // 關窗 / Q(離開遊戲)
  bool back = false;     // Esc(離開子畫面 / 繼續訊息)
  bool up = false;       // ↑
  bool down = false;     // ↓
  bool left = false;     // ←
  bool right = false;    // →
  bool select = false;   // Enter / Space(輔助選取)
  bool cycle_lang = false; // F4:即時循環切換語系(zh-TW → en → ja → …)
  int  key = 0;          // 本幀按下的字母鍵(大寫 ASCII,如 'B'/'C'/'I'/'U'…),供快捷字母
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

  TextLayer& text() { return text_; }   // 每幀:text().clear() → add(...) → present()

  void present(const Framebuffer& fb);   // 像素層放大 + 文字層合成 → 顯示
  Input poll();                          // 收集本幀事件(in.quit=true 表示要結束)
  void close();

  // headless 合成 dump:像素層放大 + 文字層,讀回 scale 倍高解析 RGB → 寫 PPM。
  // 回傳是否成功。(需 open(headless=true 或一般皆可))。
  bool dump_ppm(const Framebuffer& fb, const std::string& path);

  int out_w() const { return kW * scale_; }
  int out_h() const { return kH * scale_; }

  // 驗證用:把 320×200 framebuffer→RGB(不含文字層,純像素層)讀回。
  static std::vector<std::uint8_t> to_rgb(const Framebuffer& fb);

private:
  // 把像素層(framebuffer)+ 文字層合成到目前 renderer 目標(不 present)。
  void compose(const Framebuffer& fb);

  SDL_Window* win_ = nullptr;
  SDL_Renderer* ren_ = nullptr;
  SDL_Texture* tex_ = nullptr;   // 320×200 streaming(像素層)
  int scale_ = 3;
  bool headless_ = false;
  TextLayer text_;
};

}  // namespace dw::render
