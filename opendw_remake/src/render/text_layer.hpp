// text_layer — 高解析 TTF 文字層(雙層渲染的「文字層」)。
//
// 設計理念(見 docs/adr/0002-two-layer-cjk-rendering.md):
//   像素層(320×200 indexed framebuffer)整數倍 nearest 放大;文字層用 SDL2_ttf
//   在「視窗高解析」原生繪製 CJK/ASCII,疊在像素層之上,永不被縮放 → 恆銳利。
//
// Deep module:對外只露 begin/add/measure_wrap;內部隱藏 SDL_ttf 字型載入、
//   palette index→RGB、glyph→texture 快取、座標 ×scale 換算與 blit。
//
// 座標一律用 320×200「虛擬座標」(與遊戲像素層同座標系);flush 時 ×scale 放到視窗。
#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>
#include "framebuffer.hpp"

struct SDL_Renderer;
struct SDL_Texture;
typedef struct _TTF_Font TTF_Font;

namespace dw::render {

// 一條文字繪製指令:虛擬座標(320×200) + UTF-8 + DOS 調色盤索引顏色 + 原生字級(視窗 px)。
//
// 座標走虛擬層(與像素層同 320×200 座標系,flush 時 ×scale 放到視窗);
// 字級 px 為「視窗原生像素」(不隨 scale 變),確保字實際大小固定、銳利。
struct TextCmd {
  int vx;             // 虛擬 x(320 座標系,左)
  int vy;             // 虛擬 y(200 座標系,字的「頂端」)
  std::string utf8;
  std::uint8_t color; // DOS palette index 0..15
  int px;             // 原生字級(視窗像素高,如 CJK≈24 / ASCII≈16)
};

class TextLayer {
public:
  TextLayer() = default;
  ~TextLayer();

  // 載入 host TTF(預設 wqy-zenhei.ttc)。scale = 視窗對虛擬層的整數倍率(只影響座標換算)。
  bool open(SDL_Renderer* renderer, const std::string& ttf_path, int scale);
  void close();
  bool ready() const { return font_for(default_px_) != nullptr; }

  void set_scale(int scale) { scale_ = scale; }

  // 累積一條繪製指令(虛擬座標 + 原生字級 px;px<=0 用 default_px_)。
  void add(int vx, int vy, const std::string& utf8, std::uint8_t color, int px = 0);

  // 清空本幀指令(每幀繪製前呼叫)。
  void clear() { cmds_.clear(); }

  // 把累積的指令:虛擬座標 ×scale 定位、以原生字級繪製到目前 renderer 的繪製目標上。
  // (呼叫端須先畫好像素層,再 flush 文字層;present 由呼叫端負責。)
  void flush();

  // 度量:字級 px 下,一段 UTF-8 的「虛擬寬度」(視窗寬 /scale)。供換行排版(虛擬座標)。
  int measure_vwidth(const std::string& utf8, int px) const;

  // 自動換行:把 utf8 依「虛擬最大寬度 max_vw」拆成多行(CJK 逐字、ASCII 逐詞)。
  std::vector<std::string> wrap(const std::string& utf8, int max_vw, int px) const;

  int default_px() const { return default_px_; }
  void set_default_px(int v) { default_px_ = v; }

private:
  // 取得(或惰性建立)對應原生字級 px 的 TTF_Font。
  TTF_Font* font_for(int px) const;

  SDL_Renderer* ren_ = nullptr;
  int scale_ = 3;
  int default_px_ = 24;           // 預設原生字級(CJK≈24px 視窗像素)
  std::string ttf_path_;

  // 原生字級 px → 已開字型。mutable:惰性建立。
  mutable std::unordered_map<int, TTF_Font*> fonts_;

  // glyph/run 快取:key = px | color | (hash of text)。值為已渲染 texture + 尺寸。
  struct CachedTex { SDL_Texture* tex; int w; int h; };
  std::unordered_map<std::uint64_t, CachedTex> cache_;

  std::vector<TextCmd> cmds_;

  CachedTex* get_texture(const std::string& utf8, std::uint8_t color, int px);
};

}  // namespace dw::render
