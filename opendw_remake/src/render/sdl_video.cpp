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

bool SdlVideo::poll() {
  SDL_Event e;
  while (SDL_PollEvent(&e)) {
    if (e.type == SDL_QUIT) return false;
    if (e.type == SDL_KEYDOWN &&
        (e.key.keysym.sym == SDLK_ESCAPE || e.key.keysym.sym == SDLK_q))
      return false;
  }
  return true;
}

void SdlVideo::close() {
  if (tex_) { SDL_DestroyTexture(tex_); tex_ = nullptr; }
  if (ren_) { SDL_DestroyRenderer(ren_); ren_ = nullptr; }
  if (win_) { SDL_DestroyWindow(win_); win_ = nullptr; }
  if (SDL_WasInit(SDL_INIT_VIDEO)) SDL_Quit();
}

}  // namespace dw::render
