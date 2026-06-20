#include "sdl_video.hpp"

#include <SDL.h>

#include <cstdlib>

#include "vga256.hpp"

namespace dw::render {

std::vector<std::uint8_t> SdlVideo::to_rgb(const Framebuffer& fb,
                                           const std::array<Rgb, 16>& pal) {
  std::vector<std::uint8_t> out(static_cast<std::size_t>(kW) * kH * 3);
  std::size_t o = 0;
  for (std::uint8_t i : fb.idx) {
    const Rgb& c = pal[i & 0x0F];
    out[o++] = c.r; out[o++] = c.g; out[o++] = c.b;
  }
  return out;
}

std::vector<std::uint8_t> SdlVideo::to_rgb(const Framebuffer& fb) {
  return to_rgb(fb, kDosPalette);   // 不帶 palette:維持 DOS 盤(golden 對拍 / 既有呼叫端不變)
}

SdlVideo::~SdlVideo() { close(); }

bool SdlVideo::init_common(int win_w, int win_h, const char* title,
                           const std::string& ttf_path, bool headless) {
  headless_ = headless;
  if (headless) {
    // headless 合成:dummy video driver(不開實體視窗),供 --dump 讀回高解析畫面。
    // SDL 2.0.x 無 SDL_HINT_VIDEODRIVER,改用環境變數(SDL_Init 前設定)。
    // 用 SDL_setenv(跨平台;Windows mingw 無 POSIX setenv)。
    SDL_setenv("SDL_VIDEODRIVER", "dummy", 1);
  }
  if (SDL_Init(SDL_INIT_VIDEO) != 0) return false;
  // 一般視窗可自由縮放(RESIZABLE);headless 維持隱藏固定。
  Uint32 wflags = headless ? SDL_WINDOW_HIDDEN : (SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
  win_ = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                          win_w, win_h, wflags);
  if (!win_) return false;
  // software renderer:headless dummy driver 下 render-to-texture + ReadPixels 穩定。
  ren_ = SDL_CreateRenderer(win_, -1, SDL_RENDERER_SOFTWARE);
  if (!ren_) return false;
  // 像素層 320×200 texture;nearest 放大維持像素正確性(整數倍)。
  SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");  // nearest
  // ── 動態縮放:固定 logical 解析度,SDL 自動等比 letterbox 到任意視窗大小 ──────────
  //   像素層與文字層都在 (win_w × win_h) 的 logical 座標繪製;使用者拉伸視窗時 SDL 每幀
  //   依當前視窗大小重算縮放(維持比例、自動黑邊),無需手動處理 resize 事件。
  //   nearest(SCALE_QUALITY=0)→ 像素藝術放大不糊;非整數倍時文字為 nearest 放大(仍可讀)。
  //   headless 視窗 = logical(無縮放)→ ReadPixels dump 不受影響。
  if (!headless) {
    SDL_RenderSetLogicalSize(ren_, win_w, win_h);
    SDL_SetWindowMinimumSize(win_, kW, kH);   // 最小 320×200,避免縮到 0
  }
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

bool SdlVideo::open(int scale, const char* title, const std::string& ttf_path, bool headless) {
  scale_ = scale;
  mode640_ = false;
  return init_common(kW * scale, kH * scale, title, ttf_path, headless);
}

bool SdlVideo::open_640x480(const char* title, const std::string& ttf_path, bool headless) {
  // 640×480 模式:像素層整數 ×2(= 640×400),垂直置中於 640×480(letterbox)。
  scale_ = kMode640Scale;   // 像素層 / 文字層座標換算固定 ×2
  mode640_ = true;
  if (!init_common(kWin640W, kWin640H, title, ttf_path, headless)) return false;
  // 文字層:座標 ×2 + letterbox y 偏移(文字疊在已置中的遊戲內容上 → 同步偏移)。
  //   字級不再與 scale 等比,由呼叫端 add(...) 傳固定 px(CJK 24 / UI 16 / 標題 48)。
  text_.set_scale(kMode640Scale);
  text_.set_y_offset(kMode640LetterY);
  return true;
}

void SdlVideo::compose(const Framebuffer& fb) {
  // 1) 像素層:framebuffer → 320×200 texture → nearest 整數放大。
  //    一般模式:放大填滿全視窗(320*scale × 200*scale)。
  //    640 模式:×2 = 640×400,垂直置中於 640×480(上下各 40px 黑邊;絕不拉伸)。
  // 16 色路徑(DOS/Amiga/X68000):per-theme 16 色盤直接 to_rgb(golden 對拍不破)。
  // 256 色路徑(VGA-256 theme):先 enhance_to_256(16 色→256 色索引),再用 256 色盤 to_rgb。
  std::vector<std::uint8_t> rgb;
  if (vga256_) {
    auto idx256 = enhance_to_256(fb);
    const auto& pal256 = vga_palette();
    rgb.resize(static_cast<std::size_t>(kW) * kH * 3);
    std::size_t o = 0;
    for (std::uint8_t i : idx256) {
      const Rgb& c = pal256[i];   // i 為完整 byte(0–255),不遮罩
      rgb[o++] = c.r; rgb[o++] = c.g; rgb[o++] = c.b;
    }
  } else {
    rgb = to_rgb(fb, active_palette_);   // per-theme 16 色盤(set_palette;預設 DOS 盤)
  }
  // 區域 RGB 覆寫(remake 加值):把 region 矩形的像素直接換成提供的 RGB(突破單盤限制),
  //   用後清除(只影響本幀)。裁切到 320×200 內。
  if (region_w_ > 0 && region_rgb_.size() == (std::size_t)region_w_ * region_h_) {
    for (int ry = 0; ry < region_h_; ++ry) {
      int y = region_y_ + ry;
      if (y < 0 || y >= kH) continue;
      for (int rx = 0; rx < region_w_; ++rx) {
        int x = region_x_ + rx;
        if (x < 0 || x >= kW) continue;
        const Rgb& c = region_rgb_[(std::size_t)ry * region_w_ + rx];
        std::size_t o = ((std::size_t)y * kW + x) * 3;
        rgb[o] = c.r; rgb[o + 1] = c.g; rgb[o + 2] = c.b;
      }
    }
    region_w_ = 0;  // 清除(僅本幀有效)
  }
  SDL_UpdateTexture(tex_, nullptr, rgb.data(), kW * 3);
  SDL_RenderClear(ren_);   // 預設黑底 → letterbox 黑邊
  if (mode640_) {
    SDL_Rect dst{0, kMode640LetterY, kW * kMode640Scale, kH * kMode640Scale};
    SDL_RenderCopy(ren_, tex_, nullptr, &dst);   // 整數 ×2 置中(像素完美)
  } else {
    SDL_RenderCopy(ren_, tex_, nullptr, nullptr);   // 放大到 kW*scale × kH*scale
  }
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
  int w = out_w(), h = out_h();   // 640 模式 = 640×480(含 letterbox);一般 = kW*scale × kH*scale
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
    const bool shift = (e.key.keysym.mod & KMOD_SHIFT) != 0;
    // 文字輸入(建角命名):可列印字元(字母含大小寫 / 數字 / 空白)+ 退格。
    //   一般狀態不讀 in.text_char/in.backspace,故不影響既有行為。
    if (k >= SDLK_a && k <= SDLK_z)
      in.text_char = (shift ? 'A' : 'a') + (k - SDLK_a);
    else if (k >= SDLK_0 && k <= SDLK_9 && !shift)
      in.text_char = '0' + (k - SDLK_0);
    else if (k == SDLK_SPACE)
      in.text_char = ' ';
    if (k == SDLK_BACKSPACE) in.backspace = true;
    switch (k) {
      case SDLK_q:      in.quit = true; break;     // Q = 離開遊戲(手冊)
      case SDLK_ESCAPE: in.back = true; break;     // Esc = 離開子畫面 / 繼續訊息
      case SDLK_UP:     in.up = true; break;
      case SDLK_DOWN:   in.down = true; break;
      case SDLK_LEFT:   in.left = true; break;
      case SDLK_RIGHT:  in.right = true; break;
      case SDLK_RETURN: case SDLK_SPACE: in.select = true; break;
      case SDLK_PAGEUP:   in.pgup = true; break;     // PgUp = 段落上翻一頁
      case SDLK_PAGEDOWN: in.pgdown = true; break;   // PgDn = 段落下翻一頁
      case SDLK_F4:     in.cycle_lang = true; break;  // F4 = 循環切換語系
      case SDLK_F1:     in.help = true; break;        // F1 = Help 覆蓋層
      case SDLK_F8:     in.cycle_theme = true; break; // F8 = 循環切換 UI 主題
      case SDLK_F10:    in.request_quit = true; break;// F10 = 請求離開(確認流程)
      default:
        // `?`(Shift+/ 或 SDLK_QUESTION)= 顯示俯視平面地圖(對齊原版手冊)。
        if (k == SDLK_QUESTION ||
            (k == SDLK_SLASH && (e.key.keysym.mod & KMOD_SHIFT))) in.key = '?';
        else if (k >= SDLK_a && k <= SDLK_z) in.key = 'A' + (k - SDLK_a);  // 字母→大寫快捷鍵
        else if (k >= SDLK_0 && k <= SDLK_9) in.key = '0' + (k - SDLK_0);
        // 建角配點:+/= 增、-/_ 減(含數字鍵盤 KP_PLUS/KP_MINUS)。
        else if (k == SDLK_EQUALS || k == SDLK_PLUS || k == SDLK_KP_PLUS) in.key = '=';
        else if (k == SDLK_MINUS || k == SDLK_KP_MINUS) in.key = '-';
        else if (k == SDLK_TAB) in.key = '\t';  // Tab:商店買/賣分頁切換
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
