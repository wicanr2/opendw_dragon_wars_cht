#include "render/viewport_amiga.hpp"

#include <algorithm>
#include <climits>

namespace dw::render {

const std::vector<Sprite>& AmigaComponentStore::blocks(int tag) const {
  auto it = cache_.find(tag);
  if (it != cache_.end()) return it->second;
  std::vector<Sprite> v;
  // 連續 blockidx 載入(<tag>_0.spr、<tag>_1.spr…),遇缺即止(達 32 上限保險)。
  for (int bi = 0; bi < 32; ++bi) {
    std::string p = dir_ + "/" + std::to_string(tag) + "_" + std::to_string(bi) + ".spr";
    auto s = Sprite::load(p);
    if (!s) break;
    v.push_back(std::move(*s));
  }
  auto& slot = cache_[tag];
  slot = std::move(v);
  return slot;
}

std::optional<std::array<Rgb, 16>> AmigaComponentStore::viewport_palette() const {
  // 任一 tag 的第一個圖塊即帶共用原生 viewport 盤(dw CLUT);110 = Castle wall 最穩定。
  for (int tag : {110, 115, 122, 125, 126, 112, 116}) {
    const auto& b = blocks(tag);
    if (!b.empty() && b[0].palette.size() >= 16) {
      std::array<Rgb, 16> pal{};
      for (int i = 0; i < 16; ++i) pal[i] = b[0].palette[i];
      return pal;
    }
  }
  return std::nullopt;
}

bool amiga_viewport_available(const AmigaComponentStore& store) {
  return !store.blocks(110).empty();
}

bool render_first_person_amiga(const res::Level& level, int x, int y, int facing,
                              Framebuffer& fb, const ComponentStore& dos_comps,
                              const AmigaComponentStore& am_store, int ox, int oy) {
  if (!amiga_viewport_available(am_store)) return false;

  // viewport 視窗範圍(像素層裁切框;對齊 to_framebuffer 的 160×136 @ (ox,oy))。
  const int vx0 = ox, vy0 = oy, vx1 = ox + 160, vy1 = oy + 136;

  // 元件選擇 + 落點:沿用 DOS golden 序列(read-only)。
  std::vector<DrawCmd> seq = compose_draw_sequence(level, x, y, facing);

  (void)dos_comps;  // 不再需要 DOS template 尺寸(改用權威 slot 對映,見下)
  bool drew = false;
  // 繪序:sky(batch0)→ ground(batch1)→ other 牆面(batch2);後畫者疊前畫者(同 DOS)。
  for (const auto& cmd : seq) {
    const auto& blocks = am_store.blocks(cmd.tag);
    if (blocks.empty()) continue;  // 該 tag 無 Amiga 圖塊(如 sky 111)→ 跳過(留底色)
    // ── 權威對映:Amiga 圖塊 index = (DOS sprite_offset − 4) / 2 ──
    //   Amiga data3 元件的 offset 表序 = DOS template 的 sprite_offset slot 序
    //   (見 tools_build/amiga_viewport_extract.py docstring:blockidx = (sprite_offset-4)/2)。
    //   舊版改用「尺寸最近匹配」是 bug:同尺寸的近/中/遠牆面被挑錯 slot → 透視碎裂。
    //   改回 slot 對映後,近/中/遠/側牆各就各位、透視收斂(對齊 DOS 幾何)。
    int bidx = (cmd.sprite_offset - 4) / 2;
    if (bidx < 0 || bidx >= (int)blocks.size()) continue;  // 該 slot 無對應圖塊 → 跳過
    const Sprite& blk = blocks[bidx];
    // 落點:DOS DrawCmd 的 xpos/ypos 為 viewport 內像素座標 → fb 座標 +(ox,oy)。
    int px = vx0 + cmd.xpos;
    int py = vy0 + cmd.ypos;
    // index 0 = 牆面陰影底(非透明);Amiga viewport 圖塊以實心覆蓋(無透明 key)。
    //   裁切到 viewport 框,避免溢出蓋右側面板。
    blk.blit_clipped(fb, px, py, /*transparent=*/-1, vx0, vy0, vx1, vy1);
    drew = true;
  }
  return drew;
}

}  // namespace dw::render
