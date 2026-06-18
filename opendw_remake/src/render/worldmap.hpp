// worldmap — area 0 (Dilmun) 專屬的「美化世界地圖」view。
//
// 與 oracle automap(render/minimap.cpp,逐位元對拍 opendw)**並存且分工**:
//   - area 0(Dilmun overworld)按 `?` → 本美化 view(WorldMap)。
//   - 其餘 39 關 → 維持 oracle automap(Minimap),保真資產不動。
//
// 設計來源(全部取自遊戲自有資料,org_map 浮水印圖僅參考方向/佈局,不入庫):
//   1. 地理依據:area 0 的 tile 格(Level::tile)。tile==0 = 水域/void,非 0 = 陸塊。
//   2. 方向校正:原版 area 0 為 portrait(W=32 窄、H=47 高),與權威 Dilmun
//      landscape 圖差 90°。本 view **旋轉 90° 逆時針(CCW)** 成 landscape:
//        landscape(nx,ny) ← 原 tile(x,y),其中 nx = y、ny = (W-1) - x。
//      已用 worldmap_dest 地標交叉驗證(Byzanople 左上、Freeport 右上、
//      Purgatory/Forlorn 下方、Mud Toad/Quag 右中…),對齊 Dilmun 佈局。
//   3. 地點標記:Level::worldmap_dest(IDX&0x7F → 目的地 area)定位每個可進入
//      的城鎮/地區,配 area→繁中名(CONTEXT.md 譯名)。
//
// Deep module:對外只露 render()(畫地形進 framebuffer)+ labels()(回傳地點
//   標籤的虛擬座標 + 繁中字串,供呼叫端用文字層繪製;避免 render 模組相依 SDL_ttf)。
#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "render/framebuffer.hpp"
#include "resource/level.hpp"

namespace dw::render {

class WorldMap {
public:
  // 一個地點標籤:像素層已畫好圖示,標籤(繁中名)由呼叫端文字層繪製。
  struct Label {
    int x, y;            // 文字錨點(framebuffer 虛擬座標,字串左端基準)
    std::string name;    // 繁中地點名
    bool right_align;    // true = 文字右端對齊 x(避免衝出右邊界)
  };

  // 畫整張美化世界地圖到 framebuffer(清空 → 邊框 → 海洋 → 陸塊/地形 → 海岸線 →
  //   地點圖示)。area 必須是 area 0(Dilmun);其餘關卡呼叫端應走 Minimap。
  //   px,py = 玩家在 area 0 的 tile 座標(<0 表示不在 area 0 / 不畫玩家標記)。
  // 回傳:地點標籤清單(繁中名 + 錨點),供呼叫端文字層繪製。
  std::vector<Label> render(Framebuffer& fb, const res::Level& level,
                            int px = -1, int py = -1) const;

  // area id → 繁中地點名(查無 → 空字串)。世界圖標記與除錯共用。
  static std::string place_name_zh(int area);
};

}  // namespace dw::render
