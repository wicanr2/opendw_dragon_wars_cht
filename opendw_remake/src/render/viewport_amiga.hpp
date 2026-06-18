#pragma once

#include <array>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "render/framebuffer.hpp"
#include "render/sprite.hpp"
#include "render/viewport_compose.hpp"
#include "resource/level.hpp"

// viewport_amiga — theme=Amiga 時的第一人稱 viewport 渲染(地牢牆面 Amiga 美術)。
//
// 設計(deep module / 不破 DOS golden):
//   • 元件「選擇 + 落點」沿用 DOS 既有 compose_draw_sequence(byte-for-byte 對拍 oracle 的
//     read-only 路徑;本模組只取其輸出 DrawCmd,不改一字)。
//   • 元件「像素」改用 Amiga data3 viewport 圖塊(4-bitplane 逆向 → .spr,見
//     tools_build/amiga_viewport_extract.py)。每個 DOS tag 在 Amiga 下有 N 個子圖塊
//     (近/中/遠 牆面 + 側牆 trapezoid)。
//   • DOS template 的 sprite_offset slot 對映到 Amiga 子圖塊:以 DOS template header
//     (runlen→寬、numruns→高)取目標尺寸,挑該 tag 尺寸最接近的 Amiga 子圖塊(維度匹配)。
//   • 全部 viewport 圖塊共用一份原生 viewport palette(內嵌於各 .spr;從 dw 主程式
//     @0x0fdf4/0x10d34 的 16-word CLUT 抽出,取代舊 stone 近似色)。呼叫端在 Amiga FP
//     期間 set_palette(該盤)。
//
// DOS theme 完全不經過本模組(走原 render_first_person + ComponentStore)→ golden 不破。

namespace dw::render {

// Amiga viewport 圖塊倉(每 tag 載 themes/amiga/components/<tag>_<idx>.spr;lazy)。
class AmigaComponentStore {
 public:
  explicit AmigaComponentStore(std::string dir) : dir_(std::move(dir)) {}

  // tag 對應的全部子圖塊(依 blockidx 排序);找不到回空 vector。
  const std::vector<Sprite>& blocks(int tag) const;

  // 任一圖塊的 palette(全 viewport 共用原生 dw CLUT 盤);無圖塊回 nullopt。
  std::optional<std::array<Rgb, 16>> viewport_palette() const;

 private:
  std::string dir_;
  mutable std::map<int, std::vector<Sprite>> cache_;
};

// 是否有 Amiga viewport 元件可用(至少載得到一個 tag 的圖塊)。
//   呼叫端據此決定:可用 → 走 Amiga FP;不可用 → 誠實回退 DOS render_first_person。
bool amiga_viewport_available(const AmigaComponentStore& store);

// theme=Amiga 第一人稱 viewport 渲染到 framebuffer(像素層,160×136 @ (ox,oy))。
//   dos_comps:DOS bundle/components(用來讀 DOS template 尺寸做維度匹配;不畫像素)。
//   am_store :Amiga viewport 圖塊倉(實際畫的 Amiga 美術)。
//   呼叫端應先 fb.clear(0) 並把 fb palette 設為 stone_palette()。
// 回傳 true = 有畫出 Amiga 牆面;false = 無可用圖塊(呼叫端回退 DOS)。
bool render_first_person_amiga(const res::Level& level, int x, int y, int facing,
                              Framebuffer& fb, const ComponentStore& dos_comps,
                              const AmigaComponentStore& am_store, int ox = 16,
                              int oy = 8);

}  // namespace dw::render
