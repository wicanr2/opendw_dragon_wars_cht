// sdl_video — 把 320×200 indexed framebuffer 放大顯示在 SDL2 視窗。
//
// Deep module:對外 open/present/poll/close;內部隱藏 SDL window/renderer/texture +
// 調色盤→RGB + 整數放大。framebuffer 仍是遊戲原生 320×200,SDL 只負責「放大顯示」。
#pragma once
#include <cstdint>
#include <vector>
#include "framebuffer.hpp"

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
  bool select = false;   // Enter / Space(輔助選取)
  int  key = 0;          // 本幀按下的字母鍵(大寫 ASCII,如 'B'/'C'/'I'/'U'…),供快捷字母
};

class SdlVideo {
public:
  ~SdlVideo();
  bool open(int scale = 3, const char* title = "OpenDW Remake — 火龍之戰");
  void present(const Framebuffer& fb);   // 上傳 + 放大顯示
  Input poll();                          // 收集本幀事件(in.quit=true 表示要結束)
  void close();

  // 驗證用:把目前 framebuffer→RGB 的結果讀回(headless 對拍 PPM)。
  static std::vector<std::uint8_t> to_rgb(const Framebuffer& fb);

private:
  SDL_Window* win_ = nullptr;
  SDL_Renderer* ren_ = nullptr;
  SDL_Texture* tex_ = nullptr;
  int scale_ = 3;
};

}  // namespace dw::render
