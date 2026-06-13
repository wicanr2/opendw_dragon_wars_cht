#include "sdl_video.hpp"

#include <SDL.h>

namespace dw::render {

std::vector<std::uint8_t> SdlVideo::to_rgb(const Framebuffer& fb) {
  std::vector<std::uint8_t> out(static_cast<std::size_t>(kW) * kH * 3);
  std::size_t o = 0;
  for (std::uint8_t i : fb.idx) {
    const Rgb& c = kDosPalette[i & 0x0F];
    out[o++] = c.r; out[o++] = c.g; out[o++] = c.b;
  }
  return out;
}

SdlVideo::~SdlVideo() { close(); }

bool SdlVideo::open(int scale, const char* title) {
  scale_ = scale;
  if (SDL_Init(SDL_INIT_VIDEO) != 0) return false;
  win_ = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                          kW * scale, kH * scale, SDL_WINDOW_SHOWN);
  if (!win_) return false;
  ren_ = SDL_CreateRenderer(win_, -1, SDL_RENDERER_SOFTWARE);
  if (!ren_) return false;
  tex_ = SDL_CreateTexture(ren_, SDL_PIXELFORMAT_RGB24,
                           SDL_TEXTUREACCESS_STREAMING, kW, kH);
  return tex_ != nullptr;
}

void SdlVideo::present(const Framebuffer& fb) {
  if (!tex_) return;
  auto rgb = to_rgb(fb);
  SDL_UpdateTexture(tex_, nullptr, rgb.data(), kW * 3);
  SDL_RenderClear(ren_);
  SDL_RenderCopy(ren_, tex_, nullptr, nullptr);  // 縮放到視窗大小
  SDL_RenderPresent(ren_);
}

Input SdlVideo::poll() {
  Input in;
  SDL_Event e;
  while (SDL_PollEvent(&e)) {
    if (e.type == SDL_QUIT) { in.quit = true; continue; }
    if (e.type != SDL_KEYDOWN) continue;
    SDL_Keycode k = e.key.keysym.sym;
    switch (k) {
      case SDLK_q:      in.quit = true; break;     // Q = 離開遊戲(手冊)
      case SDLK_ESCAPE: in.back = true; break;     // Esc = 離開子畫面 / 繼續訊息
      case SDLK_UP:     in.up = true; break;
      case SDLK_DOWN:   in.down = true; break;
      case SDLK_LEFT:   in.left = true; break;
      case SDLK_RIGHT:  in.right = true; break;
      case SDLK_RETURN: case SDLK_SPACE: in.select = true; break;
      default:
        if (k >= SDLK_a && k <= SDLK_z) in.key = 'A' + (k - SDLK_a);  // 字母→大寫快捷鍵
        else if (k >= SDLK_0 && k <= SDLK_9) in.key = '0' + (k - SDLK_0);
        break;
    }
  }
  return in;
}

void SdlVideo::close() {
  if (tex_) { SDL_DestroyTexture(tex_); tex_ = nullptr; }
  if (ren_) { SDL_DestroyRenderer(ren_); ren_ = nullptr; }
  if (win_) { SDL_DestroyWindow(win_); win_ = nullptr; }
  if (SDL_WasInit(SDL_INIT_VIDEO)) SDL_Quit();
}

}  // namespace dw::render
