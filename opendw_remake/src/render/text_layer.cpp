#include "text_layer.hpp"

#include <SDL.h>
#include <SDL_ttf.h>

#include <cstdio>
#include <functional>

namespace dw::render {

// codepoint UTF-8 解碼(沿用 cjk_font 的實作,避免相依;前進 p)。
std::uint32_t utf8_next(const char*& p);

TextLayer::~TextLayer() { close(); }

bool TextLayer::open(SDL_Renderer* renderer, const std::string& ttf_path, int scale) {
  ren_ = renderer;
  scale_ = scale;
  ttf_path_ = ttf_path;
  if (TTF_WasInit() == 0 && TTF_Init() != 0) {
    std::fprintf(stderr, "TTF_Init failed: %s\n", TTF_GetError());
    return false;
  }
  // 預先開預設字級;失敗即視為文字層不可用(呼叫端可回退)。
  return font_for(default_px_) != nullptr;
}

TTF_Font* TextLayer::font_for(int px) const {
  if (px <= 0) px = default_px_;
  auto it = fonts_.find(px);
  if (it != fonts_.end()) return it->second;
  // 原生字級 px(視窗像素)→ 高解析原生繪製,永不被縮放。
  TTF_Font* f = TTF_OpenFont(ttf_path_.c_str(), px);
  if (!f) {
    std::fprintf(stderr, "TTF_OpenFont(%s, %d) failed: %s\n", ttf_path_.c_str(), px, TTF_GetError());
  } else {
    TTF_SetFontHinting(f, TTF_HINTING_LIGHT);
  }
  fonts_[px] = f;
  return f;
}

void TextLayer::close() {
  for (auto& kv : cache_) if (kv.second.tex) SDL_DestroyTexture(kv.second.tex);
  cache_.clear();
  for (auto& kv : fonts_) if (kv.second) TTF_CloseFont(kv.second);
  fonts_.clear();
  if (TTF_WasInit()) TTF_Quit();
  ren_ = nullptr;
}

void TextLayer::add(int vx, int vy, const std::string& utf8, std::uint8_t color, int px) {
  if (utf8.empty()) return;
  cmds_.push_back({vx, vy, utf8, color, px > 0 ? px : default_px_});
}

TextLayer::CachedTex* TextLayer::get_texture(const std::string& utf8, std::uint8_t color, int px) {
  // cache key:px(16b) | color(8b) | text hash。texture 隨 renderer 生命週期持有。
  std::uint64_t h = std::hash<std::string>{}(utf8);
  std::uint64_t key = (std::uint64_t)(px & 0xFFFF) << 48 |
                      (std::uint64_t)(color & 0xFF) << 40 | (h & 0xFFFFFFFFFFULL);
  auto it = cache_.find(key);
  if (it != cache_.end()) return &it->second;

  TTF_Font* font = font_for(px);
  if (!font) return nullptr;
  const Rgb& rgb = kDosPalette[color & 0x0F];
  SDL_Color c{rgb.r, rgb.g, rgb.b, 255};
  // Blended:抗鋸齒、alpha 邊緣 → 疊在像素層上銳利且邊緣乾淨。
  SDL_Surface* surf = TTF_RenderUTF8_Blended(font, utf8.c_str(), c);
  if (!surf) return nullptr;
  SDL_Texture* tex = SDL_CreateTextureFromSurface(ren_, surf);
  CachedTex ct{tex, surf->w, surf->h};
  SDL_FreeSurface(surf);
  if (!tex) return nullptr;
  auto res = cache_.emplace(key, ct);
  return &res.first->second;
}

void TextLayer::flush() {
  if (!ren_) return;
  for (const TextCmd& cmd : cmds_) {
    CachedTex* ct = get_texture(cmd.utf8, cmd.color, cmd.px);
    if (!ct || !ct->tex) continue;
    // 虛擬座標 ×scale → 視窗定位;texture 已是原生字級(視窗 px),1:1 blit(不縮放)。
    SDL_Rect dst{cmd.vx * scale_, cmd.vy * scale_, ct->w, ct->h};
    SDL_RenderCopy(ren_, ct->tex, nullptr, &dst);
  }
}

int TextLayer::measure_vwidth(const std::string& utf8, int px) const {
  TTF_Font* font = font_for(px);
  if (!font || utf8.empty()) return 0;
  int w = 0, h = 0;
  if (TTF_SizeUTF8(font, utf8.c_str(), &w, &h) != 0) return 0;
  // 視窗寬 → 虛擬寬(/scale,四捨五入)。
  return (w + scale_ / 2) / (scale_ > 0 ? scale_ : 1);
}

std::vector<std::string> TextLayer::wrap(const std::string& utf8, int max_vw, int px) const {
  std::vector<std::string> lines;
  if (utf8.empty()) { return lines; }
  std::string cur;          // 當前行(UTF-8)
  std::string word;         // 當前未斷詞的 ASCII 單字
  auto flush_word = [&]() {
    if (word.empty()) return;
    std::string trial = cur + word;
    if (!cur.empty() && measure_vwidth(trial, px) > max_vw) {
      lines.push_back(cur); cur = word;
    } else {
      cur = trial;
    }
    word.clear();
  };
  auto push_cjk = [&](const std::string& ch) {
    std::string trial = cur + ch;
    if (!cur.empty() && measure_vwidth(trial, px) > max_vw) {
      lines.push_back(cur); cur = ch;
    } else {
      cur = trial;
    }
  };
  const char* p = utf8.c_str();
  while (*p) {
    const char* start = p;
    std::uint32_t cp = utf8_next(p);
    std::string ch(start, (std::size_t)(p - start));
    if (cp == '\n') { flush_word(); lines.push_back(cur); cur.clear(); continue; }
    if (cp < 0x80) {
      if (cp == ' ') { flush_word(); if (!cur.empty()) cur += ' '; }   // 空白 = 詞界
      else word += ch;                                                 // 累積 ASCII 單字
    } else {                                                           // CJK:可逐字斷
      flush_word();
      push_cjk(ch);
    }
  }
  flush_word();
  if (!cur.empty()) lines.push_back(cur);
  return lines;
}

}  // namespace dw::render
