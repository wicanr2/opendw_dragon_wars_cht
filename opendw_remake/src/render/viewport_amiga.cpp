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

std::optional<std::array<Rgb, 16>> AmigaComponentStore::stone_palette() const {
  // 任一 tag 的第一個圖塊即帶共用 stone 盤;110 = Castle wall 最穩定。
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

namespace {

// 讀 DOS template 於 sprite_offset slot 的目標尺寸(runlen→寬 px、numruns→高 px)。
//   DOS viewport 為 2px/byte:寬 px = runlen * 2(runlen = 每列 byte 數)。
//   回 {0,0} = 該 slot 空(size==0)或越界。
struct Dim {
  int w = 0, h = 0;
};
Dim dos_template_dim(const std::vector<std::uint8_t>* comp, int sprite_offset) {
  if (!comp) return {};
  const auto& d = *comp;
  std::size_t so = (std::size_t)sprite_offset;
  if (so + 1 >= d.size()) return {};
  std::uint16_t size = (std::uint16_t)(d[so] | (d[so + 1] << 8));
  if (size == 0) return {};
  std::size_t payload = size;  // word_104F 起始 0 → payload @ size
  if (payload + 4 > d.size()) return {};
  int runlen = d[payload];      // header[0] = 每列 byte 數
  int numruns = d[payload + 1]; // header[1] = 列數
  return {runlen * 2, numruns};
}

// 在 tag 的 Amiga 子圖塊中挑尺寸最接近目標 (tw,th) 的一個(維度匹配)。
//   以 |Δw|+|Δh| 為距離;同距優先寬高皆 ≤ 目標(不溢出 viewport)的圖塊。
const Sprite* pick_block(const std::vector<Sprite>& blocks, int tw, int th) {
  const Sprite* best = nullptr;
  int best_score = INT_MAX;
  for (const auto& s : blocks) {
    int dw = (int)s.w - tw, dh = (int)s.h - th;
    int score = std::abs(dw) + std::abs(dh);
    if (s.w <= (unsigned)tw + 4 && s.h <= (unsigned)th + 4) score -= 1000;  // 不溢出優先
    if (score < best_score) { best_score = score; best = &s; }
  }
  return best;
}

}  // namespace

bool render_first_person_amiga(const res::Level& level, int x, int y, int facing,
                              Framebuffer& fb, const ComponentStore& dos_comps,
                              const AmigaComponentStore& am_store, int ox, int oy) {
  if (!amiga_viewport_available(am_store)) return false;

  // viewport 視窗範圍(像素層裁切框;對齊 to_framebuffer 的 160×136 @ (ox,oy))。
  const int vx0 = ox, vy0 = oy, vx1 = ox + 160, vy1 = oy + 136;

  // 元件選擇 + 落點:沿用 DOS golden 序列(read-only)。
  std::vector<DrawCmd> seq = compose_draw_sequence(level, x, y, facing);

  bool drew = false;
  // 繪序:sky(batch0)→ ground(batch1)→ other 牆面(batch2);後畫者疊前畫者(同 DOS)。
  for (const auto& cmd : seq) {
    const auto& blocks = am_store.blocks(cmd.tag);
    if (blocks.empty()) continue;  // 該 tag 無 Amiga 圖塊(如 sky 111)→ 跳過(留底色)
    Dim td = dos_template_dim(dos_comps.get(cmd.tag), cmd.sprite_offset);
    if (td.w == 0 || td.h == 0) continue;  // 空 slot
    const Sprite* blk = pick_block(blocks, td.w, td.h);
    if (!blk) continue;
    // 落點:DOS DrawCmd 的 xpos/ypos 為 viewport 內像素座標 → fb 座標 +(ox,oy)。
    int px = vx0 + cmd.xpos;
    int py = vy0 + cmd.ypos;
    // index 0 = 牆面陰影底(非透明);Amiga viewport 圖塊以實心覆蓋(無透明 key)。
    //   裁切到 viewport 框,避免溢出蓋右側面板。
    blk->blit_clipped(fb, px, py, /*transparent=*/-1, vx0, vy0, vx1, vy1);
    drew = true;
  }
  return drew;
}

}  // namespace dw::render
