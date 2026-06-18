#include "render/worldmap.hpp"

#include <algorithm>
#include <cstdint>

namespace dw::render {
namespace {

// ── 版面常數(framebuffer 320×200)──────────────────────────────────────────
// landscape 尺寸:NW = H(47)、NH = W(32)。每格 6px → 282×192,留邊框 + 圖例。
constexpr int kTile = 6;     // 每 tile 像素寬高
constexpr int kOx = 18;      // 地圖左上 x(留左邊框 + 標題)
constexpr int kOy = 14;      // 地圖左上 y

// DOS 16 色語意配色。
enum Col : std::uint8_t {
  C_BLACK = 0, C_OCEAN_DEEP = 1, C_FOREST = 2, C_TEAL = 3, C_RED = 4,
  C_MAGENTA = 5, C_BROWN = 6, C_LAND = 7, C_GRAY = 8, C_OCEAN = 9,
  C_GRASS = 10, C_WATER_LT = 11, C_HILL_LT = 12, C_PINK = 13,
  C_GOLD = 14, C_WHITE = 15,
};

// 穩定 hash(地形紋理用,固定不隨機 → 每次出圖一致)。
inline std::uint32_t hash2(int x, int y) {
  std::uint32_t h = (std::uint32_t)(x * 73856093) ^ (std::uint32_t)(y * 19349663);
  h ^= h >> 13; h *= 0x5bd1e995; h ^= h >> 15;
  return h;
}

// 旋轉 90° CCW:landscape(nx,ny) → 原 tile(x,y)。
//   正向定義 nx=y、ny=(W-1)-x ⇒ 反查 x=(W-1)-ny、y=nx。
struct Geo {
  int W, H, NW, NH;
  explicit Geo(const res::Level& L) : W(L.w), H(L.h), NW(L.h), NH(L.w) {}
  inline void to_orig(int nx, int ny, int& x, int& y) const { x = (W - 1) - ny; y = nx; }
  // 原 tile(x,y) → landscape(nx,ny)。
  inline void to_land(int x, int y, int& nx, int& ny) const { nx = y; ny = (W - 1) - x; }
};

// area 0 海洋/void tile:0x0F 是 area 0 最多數(1056 格)的填充 tile,構成島嶼間的
//   海;陸塊 = 其餘 tile(0x00 主體 + 城鎮/地點特殊格)。以「land = tile != 0x0F」
//   還原 Dilmun 群島輪廓(已用旋轉 ASCII 對照權威圖驗證:Kings Isle 左上、Eastern
//   Isles 右上、Quag 右中、Forlorn 下方皆吻合)。
constexpr std::uint8_t kOceanTile = 0x0F;

// 該 landscape 格是否陸地(原 tile != 海;界外視為海)。
inline bool land_at(const res::Level& L, const Geo& g, int nx, int ny) {
  if (nx < 0 || ny < 0 || nx >= g.NW || ny >= g.NH) return false;
  int x, y; g.to_orig(nx, ny, x, y);
  return L.tile(x, y) != kOceanTile;
}

// 8x8 圖示點陣(1 = 前景)。用於地點標記;風格貼近 Dilmun 圖的小建築剪影。
struct Icon { std::uint8_t row[8]; };
// 城堡/城鎮(雉堞 + 門)。
constexpr Icon kCastle = {{0b10101010,0b10101010,0b11111111,0b11111111,
                           0b11111111,0b11100111,0b11100111,0b11100111}};
// 神殿/聖所(尖頂 + 柱)。
constexpr Icon kTemple = {{0b00011000,0b00111100,0b01111110,0b11111111,
                           0b11011011,0b11011011,0b11011011,0b11111111}};
// 廢墟/遺跡(殘柱)。
constexpr Icon kRuin   = {{0b00000000,0b10010010,0b10010010,0b10010010,
                           0b10010010,0b11111110,0b00000000,0b00000000}};
// 森林/林地(樹冠)。
constexpr Icon kForest = {{0b00011000,0b00111100,0b01111110,0b11111111,
                           0b01111110,0b00011000,0b00011000,0b00111100}};
// 龍/巢穴(山形 + 點)。
constexpr Icon kDragon = {{0b00000000,0b00100100,0b01111110,0b11111111,
                           0b11111111,0b01111110,0b00100100,0b00000000}};
// 港口/碼頭(船帆)。
constexpr Icon kHarbor = {{0b00010000,0b00011000,0b00011100,0b00011110,
                           0b00011000,0b00011000,0b01111110,0b00111100}};
// 玩家標記(菱形)。
constexpr Icon kPlayer = {{0b00011000,0b00111100,0b01111110,0b11111111,
                           0b11111111,0b01111110,0b00111100,0b00011000}};

void blit_icon(Framebuffer& fb, int cx, int cy, const Icon& ic,
               std::uint8_t fg, std::uint8_t outline) {
  // cx,cy = 圖示中心(framebuffer 座標);8x8,先描邊再填前景 → 在地形上可辨識。
  int ox = cx - 4, oy = cy - 4;
  for (int r = 0; r < 8; ++r)
    for (int c = 0; c < 8; ++c)
      if (ic.row[r] & (0x80 >> c)) {
        for (int dy = -1; dy <= 1; ++dy)
          for (int dx = -1; dx <= 1; ++dx)
            fb.put(ox + c + dx, oy + r + dy, outline);
      }
  for (int r = 0; r < 8; ++r)
    for (int c = 0; c < 8; ++c)
      if (ic.row[r] & (0x80 >> c)) fb.put(ox + c, oy + r, fg);
}

// 地點類別 → 圖示 + 圖示色。依 area 語意(城/神殿/廢墟/林/龍谷/港)分類。
struct PlaceStyle { const Icon* icon; std::uint8_t color; };
PlaceStyle style_for(int area) {
  switch (area) {
    case 9: case 25: case 36:           // 拜占儂 / 京雄城
      return {&kCastle, C_GOLD};
    case 4:                             // 救贖之山
      return {&kTemple, C_WHITE};
    case 6: case 31:                    // 菲巴斯 / 魔法學院
      return {&kTemple, C_WATER_LT};
    case 5: case 15: case 21: case 39:  // 廢墟 / 矮人廢墟 / 沉沒遺跡 / 塔斯廢墟
      return {&kRuin, C_LAND};
    case 23: case 30:                   // 神祕林 / 獵場
      return {&kForest, C_GRASS};
    case 14: case 32:                   // 死城 / 龍谷
      return {&kDragon, C_RED};
    case 10: case 17: case 26: case 28: // 走私灣 / 自由港 / 朝聖碼頭 / 舊碼頭
      return {&kHarbor, C_WATER_LT};
    default:                            // 一般城鎮 / 據點
      return {&kCastle, C_PINK};
  }
}

}  // namespace

// area id → 繁中地點名(CONTEXT.md 譯名;查無 → 原文 / 空)。
std::string WorldMap::place_name_zh(int area) {
  switch (area) {
    case 1:  return "波卡城";
    case 2:  return "奴隸營";
    case 3:  return "守橋";
    case 4:  return "救贖之山";
    case 5:  return "廢墟";
    case 6:  return "菲巴斯";
    case 7:  return "石橋";
    case 8:  return "黃泥蟾蜍城";
    case 9:  return "拜占儂";
    case 10: return "走私灣";
    case 11: return "戰橋";
    case 12: return "蠍橋";
    case 13: return "放逐之橋";
    case 16: return "矮人城堡";
    case 14: return "死城";
    case 15: return "矮人廢墟";
    case 17: return "自由港";
    case 20: return "蘭斯克";
    case 21: return "沉沒遺跡";
    case 23: return "神祕林";
    case 24: return "蛇坑";
    case 25: return "京雄城";
    case 26: return "朝聖碼頭";
    case 28: return "舊碼頭";
    case 29: return "圍城軍營";
    case 30: return "獵場";
    case 31: return "魔法學院";
    case 32: return "龍谷";
    case 37: return "奴隸莊園";
    case 39: return "塔斯廢墟";
    default: return "";
  }
}

std::vector<WorldMap::Label> WorldMap::render(Framebuffer& fb, const res::Level& level,
                                              int px, int py) const {
  std::vector<Label> labels;
  Geo g(level);

  // ── 背景:羊皮紙感(深底 + 內框)──────────────────────────────────────
  fb.clear(C_BLACK);
  int mw = g.NW * kTile, mh = g.NH * kTile;
  // 外框(雙線:金 + 黑),營造地圖卷軸邊。
  auto rect = [&](int x0, int y0, int x1, int y1, std::uint8_t c) {
    for (int x = x0; x <= x1; ++x) { fb.put(x, y0, c); fb.put(x, y1, c); }
    for (int y = y0; y <= y1; ++y) { fb.put(x0, y, c); fb.put(x1, y, c); }
  };
  rect(kOx - 4, kOy - 4, kOx + mw + 3, kOy + mh + 3, C_GOLD);
  rect(kOx - 3, kOy - 3, kOx + mw + 2, kOy + mh + 2, C_BROWN);
  rect(kOx - 2, kOy - 2, kOx + mw + 1, kOy + mh + 1, C_GOLD);

  // ── 海洋底(漸層條紋:深藍 / 藍交替,模擬海波)+ 陸塊/地形 ───────────────
  for (int ny = 0; ny < g.NH; ++ny) {
    for (int nx = 0; nx < g.NW; ++nx) {
      int sx = kOx + nx * kTile, sy = kOy + ny * kTile;
      bool land = land_at(level, g, nx, ny);
      if (!land) {
        // 海洋:深淺交替波紋;近岸(鄰陸)用淺藍 → 海岸過渡。
        bool near_coast =
            land_at(level, g, nx + 1, ny) || land_at(level, g, nx - 1, ny) ||
            land_at(level, g, nx, ny + 1) || land_at(level, g, nx, ny - 1);
        for (int dy = 0; dy < kTile; ++dy)
          for (int dx = 0; dx < kTile; ++dx) {
            std::uint8_t c;
            if (near_coast) c = C_OCEAN;                    // 近岸淺藍
            else c = ((ny + (dy >> 1)) & 1) ? C_OCEAN_DEEP : C_OCEAN; // 波紋
            fb.put(sx + dx, sy + dy, c);
          }
        continue;
      }
      // 陸地:依 hash 決定地形(平原 / 森林 / 丘陵),分佈穩定有層次。
      std::uint32_t hh = hash2(nx, ny);
      std::uint8_t base, accent;
      int kind = hh % 100;
      bool hill = false;
      if (kind < 16) { base = C_FOREST; accent = C_GRASS; }       // 森林(深綠 + 草點)
      else if (kind < 26) { base = C_BROWN; accent = C_GOLD; hill = true; } // 丘陵/山(棕 + 金頂)
      else { base = C_GRASS; accent = C_GOLD; }                   // 平原(草 + 稀疏金斑)
      for (int dy = 0; dy < kTile; ++dy)
        for (int dx = 0; dx < kTile; ++dx) {
          std::uint8_t c = base;
          // 稀疏紋理斑點(per-pixel hash;& 0x1F == 0 → 約 3%,避免雜訊過多)。
          if (((hash2(nx * kTile + dx, ny * kTile + dy)) & 0x1F) == 0) c = accent;
          fb.put(sx + dx, sy + dy, c);
        }
      // 丘陵/山:畫「^」筆觸(棕底 + 金頂),像手繪山脈。
      if (hill) {
        fb.put(sx + kTile / 2, sy + 1, C_GOLD);
        fb.put(sx + kTile / 2 - 1, sy + 2, C_BROWN);
        fb.put(sx + kTile / 2 + 1, sy + 2, C_BROWN);
      }
    }
  }

  // ── 海岸線:陸地與海相鄰處描深色描邊(讓島形清晰、像手繪地圖)─────────────
  for (int ny = 0; ny < g.NH; ++ny)
    for (int nx = 0; nx < g.NW; ++nx) {
      if (!land_at(level, g, nx, ny)) continue;
      bool edge = !land_at(level, g, nx + 1, ny) || !land_at(level, g, nx - 1, ny) ||
                  !land_at(level, g, nx, ny + 1) || !land_at(level, g, nx, ny - 1);
      if (!edge) continue;
      int sx = kOx + nx * kTile, sy = kOy + ny * kTile;
      // 在陸地外緣(朝海那側)畫深色描邊。
      if (!land_at(level, g, nx, ny - 1))
        for (int dx = 0; dx < kTile; ++dx) fb.put(sx + dx, sy, C_TEAL);
      if (!land_at(level, g, nx, ny + 1))
        for (int dx = 0; dx < kTile; ++dx) fb.put(sx + dx, sy + kTile - 1, C_TEAL);
      if (!land_at(level, g, nx - 1, ny))
        for (int dy = 0; dy < kTile; ++dy) fb.put(sx, sy + dy, C_TEAL);
      if (!land_at(level, g, nx + 1, ny))
        for (int dy = 0; dy < kTile; ++dy) fb.put(sx + kTile - 1, sy + dy, C_TEAL);
    }

  // ── 地點圖示 + 標籤錨點(worldmap_dest 定位)──────────────────────────────
  for (int y = 0; y < level.h; ++y)
    for (int x = 0; x < level.w; ++x) {
      int dest = level.worldmap_dest(level.tile(x, y));
      if (dest < 0) continue;
      std::string name = place_name_zh(dest);
      if (name.empty()) name = level.name;  // 退而求其次:用目標關卡名
      int nx, ny; g.to_land(x, y, nx, ny);
      int cx = kOx + nx * kTile + kTile / 2;
      int cy = kOy + ny * kTile + kTile / 2;
      PlaceStyle ps = style_for(dest);
      blit_icon(fb, cx, cy, *ps.icon, ps.color, C_BLACK);
      // 標籤錨點:圖示右側;近右界則改右對齊放圖示左側。
      bool right = (nx > g.NW * 3 / 5);
      labels.push_back({right ? cx - 6 : cx + 6, cy - 4, name, right});
    }

  // ── 玩家標記(若在 area 0)─────────────────────────────────────────────
  if (px >= 0 && py >= 0 && px < level.w && py < level.h) {
    int nx, ny; g.to_land(px, py, nx, ny);
    int cx = kOx + nx * kTile + kTile / 2, cy = kOy + ny * kTile + kTile / 2;
    blit_icon(fb, cx, cy, kPlayer, C_RED, C_WHITE);
  }

  return labels;
}

}  // namespace dw::render
