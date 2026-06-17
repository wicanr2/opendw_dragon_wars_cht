#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "render/framebuffer.hpp"
#include "render/viewport_compose.hpp"  // ComponentStore, LevelComponents
#include "resource/level.hpp"

// 俯視平面地圖 (overhead minimap) — 對應原版手冊 `?` 鍵。
//
// 忠實 port 自 Devin Smith 的 opendw C 反組譯 (src/lib/engine.c):
//   process_minimap_commands (3260) / draw_minimap (3204) /
//   draw_minimap_row (2994) / calc_minimap_position (2955) /
//   draw_minimap_segment (2979) / plot_minimap_resource (3150) /
//   draw_minimap_from_data6820 (3184) / get_map_tile_data (5206)。
//
// 演算法重點:
//   - 以玩家為中心,9 格 (col 0..8) × N pass 的視窗;每格 0x20 (32) px 寬。
//   - init_offsets(0x90):viewport_memory 列 stride = 0x90 (144 byte = 288 px)。
//   - 每格依 tile record (word_11C6) 的位元選 sprite:
//       高 byte bit3 (0x08) = 「已探索 (seen)」旗標 — 只有探索過的格才畫內容;
//       未探索 → 畫 minimap_viewport 空地磚 (全 0)。
//       高 byte bit4-5 → 地形型 (data_56E5[+4]);
//       低 byte bit4-7 → 怪物/NPC (data_56C6);bit0-3 → 物品/事件 (data_56C6)。
//     玩家所在格再疊 data_6820 玩家標記。
//   - sprite blit 用與第一人稱相同的 decode_viewport_data + draw_sprite_to_viewport。
//
// 「已探索」旗標由遊戲走動時 refresh_viewport 設定 (engine.c:5688
// `data_5521[word_551F+1] |= 0x8`),屬 per-tile fog of war。本層提供 seed 介面
// 讓呼叫端決定要顯示哪些格 (玩家走過的集合 / 全圖)。
//
// 對拍基準:tools_build/minimap_golden/golden_minimap.c 的 minimap viewport_memory。

namespace dw::render {

// minimap viewport_memory:0x90 stride × 0x100 列 = 36864 byte。
// (與 golden_minimap.c MMEM_STRIDE×MMEM_ROWS 一致,以利 byte-for-byte 對拍。)
class Minimap {
public:
  static constexpr int kStride = 0x90;   // 144 byte/列 = 288 px
  static constexpr int kRows = 0x100;    // 256 列
  static constexpr int kMemSize = kStride * kRows;  // 36864

  std::array<std::uint8_t, kMemSize> mem{};

  // 探索旗標 seeding 模式。
  enum class Seed {
    kAll,      // 全圖探索 (= 走遍全關;展示完整地圖,行使所有分支)
    kPlayer,   // 只探索玩家當前格 (= 剛進關卡一次 refresh)
    kNone,     // 不額外 seed (用 level 既有的 0x08 位元;通常全空)
  };

  // 載入 minimap viewport 模板:
  //   minimap_bin  — com 0x695C (assets/bundle/viewport/minimap.bin),空地磚。
  //   player_bin   — com 0x6820 (assets/bundle/viewport/data6820.bin),玩家標記。
  // render() 前必須先呼叫;回傳是否兩個檔都成功載入。
  bool load_templates(const std::string& minimap_bin, const std::string& player_bin);

  // 直接給 bytes (測試 / harness 用)。
  void set_templates(std::vector<std::uint8_t> minimap, std::vector<std::uint8_t> player) {
    minimap_template_ = std::move(minimap);
    player_template_ = std::move(player);
  }

  // 把目前關卡組成俯視圖到 mem (對應 draw_minimap 第一次完整 sweep)。
  //   level     — 目前關卡。
  //   px,py     — 玩家地圖座標 (game_state.unknown[0]=x, [1]=y)。
  //   comps     — 元件 sprite 資源 (與第一人稱共用 bundle)。
  //   seed      — 探索旗標 seeding (測試 / golden 用:kAll/kPlayer/kNone)。
  // 完成後 mem 即 viewport_memory,可 to_framebuffer 上螢幕。
  void render(const res::Level& level, int px, int py,
              const ComponentStore& comps, Seed seed = Seed::kAll);

  // 完整 8 趟平面地圖(對齊原版 draw_minimap 的 byte_1964 0..7 捲動疊圖):
  //   每趟讀不同 map row 帶並疊進 mem 的對應螢幕列,使俯視圖鋪滿整個視窗
  //   (修稽核 #1:render() 單趟只畫最頂一帶)。to_framebuffer 取到完整地圖。
  //   render() 仍保留供 golden 對拍(leaf-level 單趟 viewport_memory)。
  void render_full(const res::Level& level, int px, int py,
                   const ComponentStore& comps, Seed seed = Seed::kAll);
  void render_full_with_seen(const res::Level& level, int px, int py,
                             const ComponentStore& comps,
                             const std::uint8_t* seen, int seen_w, int seen_h);

  // 遊戲內 fog of war:用呼叫端維護的 seen bitmap (row-major, w*h bytes;
  // 非 0 = 該格已看過) 決定哪些格揭露。seen 累積對齊 oracle
  // (refresh_viewport 每步只標記玩家當前格;見 game::SeenMap)。
  //   seen_w/seen_h 應 = level.w/level.h;seen 為 nullptr 時等同全不揭露。
  void render_with_seen(const res::Level& level, int px, int py,
                        const ComponentStore& comps,
                        const std::uint8_t* seen, int seen_w, int seen_h);

  // 把 mem blit 到 framebuffer。
  // 原版 set_viewport_size + ui_update_viewport 會把 0x90-stride 的 mem
  // 以水平切片捲動上螢幕;這裡直接把可見的 9 格寬 (288 px / 2 = 144 byte)
  // × 高度區域畫進 framebuffer 左上 (ox,oy)。
  //   每 byte = 2 像素 (hi nibble = 左、lo nibble = 右)。
  void to_framebuffer(render::Framebuffer& fb, int ox = 1, int oy = 8,
                      int rows = 0xC0) const;

private:
  void reset() { mem.fill(0); }

  std::vector<std::uint8_t> minimap_template_;  // com 0x695C
  std::vector<std::uint8_t> player_template_;   // com 0x6820
};

}  // namespace dw::render
