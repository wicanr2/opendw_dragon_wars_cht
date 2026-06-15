// verify_city_entry — 鎖定本輪動態 trace 逆出的「進城/換區」事實(回歸守門)。
//
// 由 trace_subarea_dyn 動態逆出(VM 實機執行 + 注入 'Y' 通過 op_8C):
//   1) 世界圖 tile 0x1C @(27,7) 的 IDX byte = 0x89,**bit7 為「需 Y/N 確認」旗標**,
//      目的地 area = 0x89 & 0x7F = 9 = Byzanople 拜占儂。resource 8 @off=6 解碼段
//      @0x01ad `38 7F`(AND 0x7F)→ `12 02`(gs[2]=r2)實證。Level::worldmap_dest 應回 9。
//   2) area 29(Siege Camp 軍營)有「直接 gs[2] 進入」格:1A 00 07 / 1A 01 09 / 1A 02 09
//      → 進拜占儂(9),入口 (7,9),op_8C 門控。Level::area_entry_relocs 應含此邊。
//   3) **誠實鐵則守門**:全 40 關 **無任何** 指向 area 6(Phoebus 菲巴斯)的正向換區邊
//      (worldmap_dest / subarea_relocs 1A 45 / area_entry_relocs 1A 02 三套機制皆無),
//      唯一含 6 的邊是 area 33(Phoeban Dungeon)→ 6 的 1A 45 06 反向/內部邊。
//      此測試確保「不臆造 Phoebus 入口」這條鐵則不被未來改動悄悄破壞。
//
// 用法:verify_city_entry <bundle_dir>
#include <cstdio>
#include <set>
#include <string>
#include "../../src/resource/level.hpp"

using namespace dw;
static int fails = 0;
static void check(bool ok, const char* msg) {
  std::printf("[%s] %s\n", ok ? "PASS" : "FAIL", msg);
  if (!ok) ++fails;
}

int main(int argc, char** argv) {
  if (argc < 2) { std::fprintf(stderr, "usage: %s <bundle_dir>\n", argv[0]); return 2; }
  std::string bundle = argv[1];

  // (1) 世界圖 tile 0x1C → Byzanople 9(IDX 0x89 遮罩 0x7F)。
  auto a0 = res::Level::load_file(bundle + "/maps/0.lvl");
  check(a0.has_value(), "area 0 (Dilmun 世界圖) 載入");
  if (a0) {
    int dest = a0->worldmap_dest(0x1C);
    std::printf("    worldmap_dest(tile 0x1C) = %d (期望 9 = Byzanople)\n", dest);
    check(dest == 9, "世界圖 tile 0x1C → area 9 拜占儂(IDX 0x89 & 0x7F)");
    // 原 26 條不受 bit7 mask 影響(抽樣):波卡城 1 / 黃泥蟾蜍 8 / 京雄城 25。
    check(a0->worldmap_dest(0x03) == 1, "世界圖 tile 0x03 → area 1 波卡城(回歸)");
    check(a0->worldmap_dest(0x0B) == 8, "世界圖 tile 0x0B → area 8 黃泥蟾蜍(回歸)");
    check(a0->worldmap_dest(0x14) == 25, "世界圖 tile 0x14 → area 25 京雄城(回歸)");
  }

  // (2) area 29 Siege Camp 的直接 gs[2] 進入邊 → 9(入口 7,9,op_8C 門控)。
  auto a29 = res::Level::load_file(bundle + "/maps/29.lvl");
  check(a29.has_value(), "area 29 (Siege Camp 軍營) 載入");
  if (a29) {
    bool found = false;
    for (auto& e : a29->area_entry_relocs())
      if (e.area == 9 && e.x == 7 && e.y == 9 && e.gated) found = true;
    check(found, "area 29 → 9 拜占儂(1A 02 09,入口(7,9),op_8C 門控)= 第三種進城機制");
  }

  // (3) 鐵則守門:全 40 關無任何「正向」指向 area 6 的換區邊。
  int fwd_to_6 = 0, rev_33_to_6 = 0;
  for (int a = 0; a <= 39; ++a) {
    auto lvl = res::Level::load_file(bundle + "/maps/" + std::to_string(a) + ".lvl");
    if (!lvl) continue;
    // worldmap_dest 指向 6?**只查地圖上實際出現的 tile 值**(對齊 flood-fill;
    //   掃全 v 會誤踩未引用的資料 byte 假性命中,非真實可踩格)。
    if (lvl->wraps()) {
      std::set<int> present;
      for (int y = 0; y < lvl->h; ++y) for (int x = 0; x < lvl->w; ++x) {
        int t = lvl->tile(x, y); if (t > 1) present.insert(t);
      }
      for (int v : present) if (lvl->worldmap_dest((std::uint8_t)v) == 6) ++fwd_to_6;
    }
    // area_entry_relocs(1A 02 06)指向 6?
    for (auto& e : lvl->area_entry_relocs()) if (e.area == 6) ++fwd_to_6;
    // subarea_relocs(1A 45 06):area 33→6 為唯一(反向/內部),其餘皆不該有。
    for (auto& r : lvl->subarea_relocs())
      if (r.area == 6) { if (a == 33) ++rev_33_to_6; else ++fwd_to_6; }
  }
  std::printf("    指向 area 6 的正向邊數 = %d(期望 0);area 33→6 反向邊 = %d(期望 1)\n",
              fwd_to_6, rev_33_to_6);
  check(fwd_to_6 == 0, "Phoebus 6 無任何正向入口邊(誠實鐵則:逆不出 → 不臆造)");
  check(rev_33_to_6 == 1, "唯一含 6 的邊 = area 33(Phoeban Dungeon)→ 6 的 1A 45 06");

  std::printf(fails ? "\n%d 項失敗\n" : "\n全部通過\n", fails);
  return fails ? 1 : 0;
}
