// verify_mainline_reachable — 主線跨區連通的 flood-fill 驗證(P0-3)。
//
// 把每個 area 當節點、可走通的換區當有向邊,從 area 1(波卡城,遊戲起點)做
// BFS,輸出可達 area 集合與連通分量大小。邊集合由兩條機制聯集而成:
//
//   1. **世界圖 / 樞紐踩格進區**(本次 DRAGON.COM 反組譯新逆出):
//      wrap 關卡(area 0 Dilmun 世界圖等)的城鎮/地點格,事件腳本走
//      op_58→資源8→op_6x<IDX>,IDX = 目的地 area。由 Level::worldmap_dest() 解。
//      → 並補上「進到世界圖的反向邊」:任一 area 的事件能寫 gs[2]=0 即可回世界圖;
//        但更關鍵的是「從某地表 area 能走到世界圖 area 0」這條入口。本驗證採
//        保守模型:把 area 0 視為樞紐,凡 area 0 能到的城,皆假設該城能回 area 0
//        (原版世界圖雙向;城內離開即回世界圖對應格),故 area0 與其 27 個目的地
//        互為連通(雙向邊)。
//
//   2. **tile 事件換區**(既有 probe_areaswitch 機制):跑該關特殊格事件腳本,
//      若 gs[2] 變更則記為有向邊(對拍 op_71→run_level_script)。涵蓋 area 28→26
//      等非世界圖換場。
//
// 用法:verify_mainline_reachable <bundle_dir>
// 輸出:可達 area 列表 + 連通分量大小 + 與「全 40 area」對比。回傳 0(永遠成功;
//   這是觀測工具,非 pass/fail gate;連通數印出供 roadmap 追蹤)。
#include <cstdio>
#include <map>
#include <optional>
#include <queue>
#include <set>
#include <string>
#include <vector>
#include "../../src/resource/level.hpp"
#include "../../src/resource/provider.hpp"
#include "../../src/vm/interpreter.hpp"

using namespace dw;

static const char* area_name(int a) {
  static const char* N[40] = {
      "Dilmun(世界圖)", "Purgatory波卡城", "Slave Camp奴隸營", "Guard Bridge守橋",
      "Salvation救贖之山", "Tars Ruins塔斯廢墟", "Phoebus菲巴斯", "Bridge橋",
      "Mud Toad黃泥蟾蜍", "Byzanople拜占儂", "Smugglers Cove海盜竊穴", "War Bridge",
      "Scorpion Bridge蠍橋", "Bridge of Exiles放逐橋", "Necropolis奈羅波裡",
      "Dwarf Ruins矮人廢墟", "Dwarf Clan Hall矮人城堡", "Freeport自由港",
      "Magan瑪根地底", "Mines礦場", "Lansk蘭斯克", "Sunken Ruins沉沒(陸)",
      "Sunken沉沒(水)", "Mystic Wood神祕林", "Snake Pit蛇窟", "Kingshome京雄城",
      "Pilgrim Dock朝聖碼頭", "Depths of Nisir尼塞山腹", "Old Dock老碼頭",
      "Siege Camp軍營", "Game Preserve獵區", "Magic College魔法學院",
      "Dragon Valley龍谷", "Phoeban Dungeon菲巴斯地牢", "Lanac'toor Lab拉娜實驗室",
      "Byzan Dungeon拜占儂地下", "Kingshome Dgn京雄地牢", "Slave Estate莫格宅院",
      "Lansk Undercity蘭斯克地下", "Tars Ruins塔斯地下"};
  return (a >= 0 && a < 40) ? N[a] : "?";
}

int main(int argc, char** argv) {
  if (argc < 2) { std::fprintf(stderr, "usage: %s <bundle_dir>\n", argv[0]); return 2; }
  std::string bundle = argv[1];
  res::BundleProvider prov(bundle);

  // edges[a] = {目的地 area...}(有向);記錄來源以便印出。
  std::map<int, std::set<int>> edges;
  auto add_edge = [&](int a, int b) { if (a != b && b >= 0) edges[a].insert(b); };

  int worldmap_edges = 0, tile_edges = 0;
  for (int area = 0; area <= 39; ++area) {
    auto lvl = res::Level::load_file(bundle + "/maps/" + std::to_string(area) + ".lvl");
    if (!lvl) continue;
    int level_res = area + 0x46;
    std::set<int> vals;
    for (int y = 0; y < lvl->h; ++y) for (int x = 0; x < lvl->w; ++x) {
      int t = lvl->tile(x, y); if (t > 1) vals.insert(t);
    }
    for (int v : vals) {
      // (1) 世界圖 / 樞紐踩格進區(DRAGON.COM 反組譯反推)。
      if (lvl->wraps()) {
        int dest = lvl->worldmap_dest((std::uint8_t)v);
        if (dest >= 0 && dest != area) {
          add_edge(area, dest);
          add_edge(dest, area);   // 世界圖雙向(離城即回世界圖對應格)
          ++worldmap_edges;
          continue;               // 該格已是世界圖換區,不再跑 VM
        }
      }
      // (2) tile 事件換區(跑事件腳本看 gs[2] 是否變更)。
      std::uint16_t pc = lvl->script_pc((std::uint8_t)v);
      if (pc == 0 || pc >= lvl->data().size()) continue;
      vm::VmState st;
      st.script = lvl->data(); st.data_bytes = lvl->data();
      st.script_res = level_res; st.data_res = level_res; st.pc = pc;
      st.game_state[2] = (std::uint8_t)area;
      st.game_state[0x57] = (std::uint8_t)area;
      st.resource_provider = [&](int tag) -> std::optional<std::vector<std::uint8_t>> {
        if (tag == level_res) return lvl->data();
        if (auto b = prov.load(tag)) return b;
        return std::nullopt;
      };
      vm::Interpreter ip(st);
      ip.set_message_sink([](std::size_t, const std::string&) {});
      ip.run();
      if (st.game_state[2] != (std::uint8_t)area) {
        add_edge(area, st.game_state[2]);
        ++tile_edges;
      }
    }
  }

  // (3) **子區 relocate 邊**(resource 5 反組譯逆出:1A 41/43/45 + 58 05 → Yes 分支
  //     寫 gs[2]=gs[0x45])。靜態讀 1A 45 <AREA> 即得目的地(headless 無鍵盤取不到
  //     Yes 分支,故動態跑不出此邊;此處用靜態解碼補上)。詳見 Level::subarea_relocs。
  int subarea_edges = 0;
  for (int area = 0; area <= 39; ++area) {
    auto lvl = res::Level::load_file(bundle + "/maps/" + std::to_string(area) + ".lvl");
    if (!lvl) continue;
    for (auto& r : lvl->subarea_relocs()) {
      add_edge(area, r.area);
      ++subarea_edges;
    }
  }

  // BFS 從 area 1(波卡城起點)。
  std::set<int> reachable;
  std::queue<int> q;
  q.push(1); reachable.insert(1);
  while (!q.empty()) {
    int a = q.front(); q.pop();
    for (int b : edges[a]) if (!reachable.count(b)) { reachable.insert(b); q.push(b); }
  }

  // 同時印「area 0 世界圖連通分量」(玩家序盤一旦上世界圖能到哪)。
  std::set<int> from_world;
  q.push(0); from_world.insert(0);
  while (!q.empty()) {
    int a = q.front(); q.pop();
    for (int b : edges[a]) if (!from_world.count(b)) { from_world.insert(b); q.push(b); }
  }

  std::printf("== 跨區連通 flood-fill(DRAGON.COM 反組譯反推世界圖樞紐 + tile 事件 + 子區 relocate)==\n");
  std::printf("邊統計:世界圖樞紐邊 %d(雙向)  tile 事件邊 %d  子區 relocate 邊 %d\n",
              worldmap_edges, tile_edges, subarea_edges);
  std::printf("\n-- 換區邊(來源 -> 目的)--\n");
  for (auto& [a, dsts] : edges) {
    std::printf("  area %2d %-22s ->", a, area_name(a));
    for (int b : dsts) std::printf(" %d", b);
    std::printf("\n");
  }
  std::printf("\n-- 從 area 1(波卡城)BFS 可達 --\n  ");
  for (int a : reachable) std::printf("%d ", a);
  std::printf("\n  連通分量大小:%zu / 40\n", reachable.size());
  std::printf("\n-- 從 area 0(世界圖)BFS 可達 --\n  ");
  for (int a : from_world) std::printf("%d ", a);
  std::printf("\n  連通分量大小:%zu / 40\n", from_world.size());

  // 主線必經地區(docs/48 §1.2)可達性檢核。
  int mainline[] = {1, 2, 5, 6, 8, 17, 23, 20, 14, 25, 32, 31, 21, 30, 29, 4};
  std::printf("\n-- 主線地表地區可達性(從世界圖)--\n");
  int hit = 0, tot = 0;
  for (int a : mainline) {
    ++tot;
    bool ok = from_world.count(a) > 0;
    if (ok) ++hit;
    std::printf("  [%s] area %2d %s\n", ok ? "OK " : "MISS", a, area_name(a));
  }
  std::printf("  主線地表可達:%d / %d\n", hit, tot);

  // ── 子區入口診斷(Phoebus 6 / Byzanople 9 等非世界圖區)─────────────────
  // 這些區不在 area 0 世界圖直連表(已交叉驗證:area 0 的 26 個城鎮格 IDX 不含
  // 6 或 9)。它們是「由母區踩格進入」的子區(攻略:Phoebus 由 Mud Toad 8 進、
  // Byzanople 由 Siege Camp 29 進)。母區的進入格事件走
  //   `1A 41 .. 1A 43 .. 1A 45 ..`(設 var 0x41/0x43/0x45)+ `op_58 資源 5`(relocate
  //   handler 候選),或在中途 emit 文字。但 relocate 鏈最終命中 **未實作 opcode**
  //   (op_79 set_msg / op_68 / op_6B / op_70),VM halt → gs[2] 從未被寫 → 無法
  //   靜態/動態逆出目的地。opendw oracle 對這些 opcode 亦標 NULL(無 C 參考)。
  // 本段把「每個非世界圖 mainline 子區 + 其母區進入格的 halt opcode」列出,供
  //   docs/54 追蹤;**不臆造邊**(逆不出就不補)。
  struct SubArea { int child; int parent; const char* note; };
  SubArea subs[] = {
      {6, 8,  "Phoebus 菲巴斯 ← Mud Toad 黃泥蟾蜍(攻略 38 §5.4-5.5)"},
      {9, 29, "Byzanople 拜占儂 ← Siege Camp 軍營(攻略 38 §5.14)"},
  };
  std::printf("\n-- 非世界圖子區入口診斷(逆不出 → 不補邊,記錄 halt opcode)--\n");
  for (auto& s : subs) {
    bool child_reached = from_world.count(s.child) > 0;
    auto plvl = res::Level::load_file(bundle + "/maps/" + std::to_string(s.parent) + ".lvl");
    // 掃母區特殊格,找走 op_58 資源 5(relocate)的格,回報其 halt opcode。
    std::set<int> halt_ops;
    bool has_res5_path = false;
    if (plvl) {
      int plvl_res = s.parent + 0x46;
      std::set<int> vals;
      for (int y = 0; y < plvl->h; ++y) for (int x = 0; x < plvl->w; ++x) {
        int t = plvl->tile(x, y); if (t > 1) vals.insert(t);
      }
      for (int v : vals) {
        std::uint16_t pc = plvl->script_pc((std::uint8_t)v);
        if (pc == 0 || pc >= plvl->data().size()) continue;
        vm::VmState st;
        st.script = plvl->data(); st.data_bytes = plvl->data();
        st.script_res = plvl_res; st.data_res = plvl_res; st.pc = pc;
        st.game_state[2] = (std::uint8_t)s.parent;
        st.game_state[0x57] = (std::uint8_t)s.parent;
        bool called5 = false;
        st.resource_provider = [&](int tag) -> std::optional<std::vector<std::uint8_t>> {
          if (tag == 5) called5 = true;
          if (tag == plvl_res) return plvl->data();
          if (auto b = prov.load(tag)) return b;
          return std::nullopt;
        };
        vm::Interpreter ip(st);
        ip.set_message_sink([](std::size_t, const std::string&) {});
        ip.run(20000);
        if (st.game_state[2] != (std::uint8_t)s.parent) {
          // 若真有 gs[2] 變更(理論上不該發生),補邊。
          add_edge(s.parent, st.game_state[2]);
        }
        if (called5) has_res5_path = true;
        if (ip.last_unimpl() != 0) halt_ops.insert(ip.last_unimpl());
      }
    }
    std::printf("  area %2d %-22s reached=%s  母區 %d res5-relocate格=%s  halt opcodes:",
                s.child, area_name(s.child), child_reached ? "YES" : "NO",
                s.parent, has_res5_path ? "有" : "無");
    if (halt_ops.empty()) std::printf(" (無)");
    for (int o : halt_ops) std::printf(" 0x%02X", o);
    std::printf("\n    %s\n", s.note);
  }

  return 0;
}
