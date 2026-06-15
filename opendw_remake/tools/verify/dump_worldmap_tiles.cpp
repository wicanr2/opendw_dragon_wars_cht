// dump_worldmap_tiles — 逐 area 列出特殊格 → script bytes → 解碼 IDX(grounding 用,非回歸)。
// 用法:dump_worldmap_tiles <bundle_dir>
#include <cstdio>
#include <map>
#include <set>
#include <string>
#include "../../src/resource/level.hpp"

using namespace dw;

static const char* NAME(int a) {
  static const char* N[40] = {
      "Dilmun世界圖", "Purgatory波卡城", "SlaveCamp奴隸營", "GuardBridge守橋",
      "Salvation救贖之山", "TarsRuins塔斯廢墟", "Phoebus菲巴斯", "Bridge橋",
      "MudToad黃泥蟾蜍", "Byzanople拜占儂", "SmugglersCove海盜竊穴", "WarBridge",
      "ScorpionBridge蠍橋", "BridgeOfExiles放逐橋", "Necropolis奈羅波裡",
      "DwarfRuins矮人廢墟", "DwarfClanHall矮人城堡", "Freeport自由港",
      "Magan瑪根地底", "Mines礦場", "Lansk蘭斯克", "SunkenRuins沉沒陸",
      "Sunken沉沒水", "MysticWood神祕林", "SnakePit蛇窟", "Kingshome京雄城",
      "PilgrimDock朝聖碼頭", "DepthsNisir尼塞山腹", "OldDock老碼頭",
      "SiegeCamp軍營", "GamePreserve獵區", "MagicCollege魔法學院",
      "DragonValley龍谷", "PhoebanDgn菲巴斯地牢", "LanacLab拉娜實驗室",
      "ByzanDgn拜占儂地下", "KingshomeDgn京雄地牢", "SlaveEstate莫格宅院",
      "LanskUnder蘭斯克地下", "TarsUnder塔斯地下"};
  return (a >= 0 && a < 40) ? N[a] : "?";
}

int main(int argc, char** argv) {
  if (argc < 2) { std::fprintf(stderr, "usage: %s <bundle_dir>\n", argv[0]); return 2; }
  std::string bundle = argv[1];

  for (int area = 0; area <= 39; ++area) {
    auto lvl = res::Level::load_file(bundle + "/maps/" + std::to_string(area) + ".lvl");
    if (!lvl) continue;
    // 收集每個特殊 tile 值 + 第一個座標
    std::map<int, std::pair<int,int>> tilepos;
    for (int y = 0; y < lvl->h; ++y) for (int x = 0; x < lvl->w; ++x) {
      int t = lvl->tile(x, y);
      if (t > 1 && !tilepos.count(t)) tilepos[t] = {x, y};
    }
    std::printf("=== area %2d %-22s  %dx%d flags=0x%02X wraps=%d  特殊格種類=%zu\n",
                area, NAME(area), lvl->w, lvl->h, lvl->flags, lvl->wraps()?1:0, tilepos.size());
    for (auto& [t, xy] : tilepos) {
      std::uint16_t pc = lvl->script_pc((std::uint8_t)t);
      std::printf("   tile 0x%02X @(%2d,%2d) pc=0x%04X", t, xy.first, xy.second, pc);
      if (pc == 0 || pc >= lvl->size()) { std::printf("  [no script]\n"); continue; }
      // 印 script 前 16 bytes
      std::printf("  bytes:");
      for (int i = 0; i < 16 && (std::size_t)pc + i < lvl->size(); ++i)
        std::printf(" %02X", lvl->byte_at(pc + i));
      int d = lvl->worldmap_dest((std::uint8_t)t);
      if (d >= 0) std::printf("  => IDX=%d (%s)", d, NAME(d));
      // 也印「即使非 wrap 樣式」的 IDX 候選:若頭是 58 08 06 00,印 pc+8
      else if (lvl->byte_at(pc)==0x58 && lvl->byte_at(pc+1)==0x08 &&
               lvl->byte_at(pc+2)==0x06 && lvl->byte_at(pc+3)==0x00) {
        int raw = lvl->byte_at(pc+8);
        std::printf("  => [58 08 06 00 pattern] op=0x%02X raw_idx=0x%02X(%d)",
                    lvl->byte_at(pc+7), raw, raw);
      }
      // 偵測 area18 樣式 58 08 03 00
      else if (lvl->byte_at(pc)==0x58 && lvl->byte_at(pc+1)==0x08 &&
               lvl->byte_at(pc+2)==0x03 && lvl->byte_at(pc+3)==0x00) {
        std::printf("  => [58 08 03 00 off=3 pattern]");
      }
      std::printf("\n");
    }
  }
  return 0;
}
