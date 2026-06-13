#include "sdl_video.hpp"

#include <SDL.h>

#include <cstdlib>

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

bool SdlVideo::open(int scale, const char* title, const std::string& ttf_path, bool headless) {
  scale_ = scale;
  headless_ = headless;
  if (headless) {
    // headless 合成:dummy video driver(不開實體視窗),供 --dump 讀回高解析畫面。
    // SDL 2.0.x 無 SDL_HINT_VIDEODRIVER,改用環境變數(SDL_Init 前設定)。
    setenv("SDL_VIDEODRIVER", "dummy", 1);
  }
  if (SDL_Init(SDL_INIT_VIDEO) != 0) return false;
  Uint32 wflags = headless ? SDL_WINDOW_HIDDEN : SDL_WINDOW_SHOWN;
  win_ = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                          kW * scale, kH * scale, wflags);
  if (!win_) return false;
  // software renderer:headless dummy driver 下 render-to-texture + ReadPixels 穩定。
  ren_ = SDL_CreateRenderer(win_, -1, SDL_RENDERER_SOFTWARE);
  if (!ren_) return false;
  // 像素層 320×200 texture;nearest 放大維持像素正確性(整數倍)。
  SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");  // nearest
  tex_ = SDL_CreateTexture(ren_, SDL_PIXELFORMAT_RGB24,
                           SDL_TEXTUREACCESS_STREAMING, kW, kH);
  if (!tex_) return false;
  // 文字層:host TTF;失敗則文字層不可用,但不阻擋程式(回退到只有像素層)。
  if (!ttf_path.empty()) {
    if (!text_.open(ren_, ttf_path, scale_))
      SDL_Log("text layer disabled (TTF open failed): %s", ttf_path.c_str());
  }
  return true;
}

void SdlVideo::compose(const Framebuffer& fb) {
  // 1) 像素層:framebuffer → 320×200 texture → nearest 整數放大至全視窗。
  auto rgb = to_rgb(fb);
  SDL_UpdateTexture(tex_, nullptr, rgb.data(), kW * 3);
  SDL_RenderClear(ren_);
  SDL_RenderCopy(ren_, tex_, nullptr, nullptr);   // 放大到 kW*scale × kH*scale
  // 2) 文字層:TextLayer 在視窗高解析原生繪製,疊在像素層之上(永不縮放)。
  text_.flush();
}

void SdlVideo::present(const Framebuffer& fb) {
  if (!tex_) return;
  compose(fb);
  SDL_RenderPresent(ren_);
}

bool SdlVideo::dump_ppm(const Framebuffer& fb, const std::string& path) {
  if (!ren_ || !tex_) return false;
  int w = kW * scale_, h = kH * scale_;
  // 合成到 default render target,再 ReadPixels 取回高解析 RGB。
  compose(fb);
  std::vector<std::uint8_t> px(static_cast<std::size_t>(w) * h * 3);
  if (SDL_RenderReadPixels(ren_, nullptr, SDL_PIXELFORMAT_RGB24, px.data(), w * 3) != 0) {
    SDL_Log("RenderReadPixels failed: %s", SDL_GetError());
    return false;
  }
  std::FILE* f = std::fopen(path.c_str(), "wb");
  if (!f) return false;
  std::fprintf(f, "P6\n%d %d\n255\n", w, h);
  std::fwrite(px.data(), 1, px.size(), f);
  std::fclose(f);
  return true;
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
      case SDLK_F4:     in.cycle_lang = true; break;  // F4 = 循環切換語系
      default:
        if (k >= SDLK_a && k <= SDLK_z) in.key = 'A' + (k - SDLK_a);  // 字母→大寫快捷鍵
        else if (k >= SDLK_0 && k <= SDLK_9) in.key = '0' + (k - SDLK_0);
        break;
    }
  }
  return in;
}

void SdlVideo::close() {
  text_.close();
  if (tex_) { SDL_DestroyTexture(tex_); tex_ = nullptr; }
  if (ren_) { SDL_DestroyRenderer(ren_); ren_ = nullptr; }
  if (win_) { SDL_DestroyWindow(win_); win_ = nullptr; }
  if (SDL_WasInit(SDL_INIT_VIDEO)) SDL_Quit();
}

}  // namespace dw::render
