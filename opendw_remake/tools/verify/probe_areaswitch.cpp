// probe_areaswitch — 掃所有關卡的特殊格事件腳本,找「會改 game_state[2](area)
//   或入口 gs[0]/gs[1]/gs[3]」的事件。對拍 opendw:op_71→run_level_script→run_script
//   後,load_level_resources 比對 gs[2] vs gs[0x57] 觸發重載。
//
// 跑法對齊 main.cpp 的 run_event:bundle provider + level-self fallback(op_0F/op_58 跨資源)。
//
// 用法:probe_areaswitch <bundle_dir> [area]
#include <cstdio>
#include <cstdlib>
#include <optional>
#include <set>
#include <string>
#include <vector>
#include "../../src/resource/level.hpp"
#include "../../src/resource/provider.hpp"
#include "../../src/vm/interpreter.hpp"

using namespace dw;

int main(int argc, char** argv) {
  if (argc < 2) { std::fprintf(stderr, "usage: %s <bundle_dir> [area]\n", argv[0]); return 2; }
  std::string bundle = argv[1];
  int only_area = -1;
  std::string data1_dir;
  for (int i = 2; i < argc; ++i) {
    std::string a = argv[i];
    if (a == "--data1" && i + 1 < argc) data1_dir = argv[++i];
    else only_area = std::atoi(a.c_str());
  }
  res::BundleProvider prov(bundle);
  // 可選:用 DATA1 補齊 op_58 跨資源 call(僅供「發掘所有換場」文件用,非執行期依賴)。
  std::optional<res::Data1Provider> d1;
  if (!data1_dir.empty()) {
    d1 = res::Data1Provider::open(data1_dir);
    std::printf("[data1 %s]\n", d1 ? "loaded" : "FAILED to load");
  }

  int hits = 0;
  for (int area = 0; area <= 42; ++area) {
    if (only_area >= 0 && area != only_area) continue;
    std::string path = bundle + "/maps/" + std::to_string(area) + ".lvl";
    auto lvl = res::Level::load_file(path);
    if (!lvl) continue;
    int level_res = area + 0x46;
    std::set<int> vals;
    for (int y = 0; y < lvl->h; ++y) for (int x = 0; x < lvl->w; ++x) {
      int t = lvl->tile(x, y); if (t > 1) vals.insert(t);
    }
    for (int v : vals) {
      std::uint16_t pc = lvl->script_pc((std::uint8_t)v);
      if (pc == 0 || pc >= lvl->data().size()) continue;
      vm::VmState st;
      st.script = lvl->data();
      st.data_bytes = lvl->data();
      st.script_res = level_res;
      st.data_res = level_res;
      st.pc = pc;
      // gs[2]=當前 area(自身),gs[0x57]=cached(對齊 op_71 進場前 gs[2]==gs[0x57])
      st.game_state[2] = (std::uint8_t)area;
      st.game_state[0x57] = (std::uint8_t)area;
      st.resource_provider = [&](int tag) -> std::optional<std::vector<std::uint8_t>> {
        if (tag == level_res) return lvl->data();
        if (auto b = prov.load(tag)) return b;
        if (d1) return d1->load(tag);   // 補齊 op_58 跨資源(文件用)
        return std::nullopt;
      };
      auto before = st.game_state;
      // 可選逐指令 trace:設環境 PROBE_TRACE=1 + 指定單一 area 時,印出 trace 看寫了哪些 gs。
      bool do_trace = only_area >= 0 && std::getenv("PROBE_TRACE");
      vm::Trace trace;
      vm::Interpreter ip(st, do_trace ? &trace : nullptr);
      std::string emitted;
      ip.set_message_sink([&](std::size_t, const std::string& s) {
        if (!emitted.empty()) emitted += " | "; emitted += s; });
      ip.run();
      if (do_trace) {
        std::printf("--- trace area %d tile 0x%02X ---\n", area, v);
        trace.dump(stdout);
      }
      auto& gs = st.game_state;
      bool area_chg = gs[2] != before[2];
      bool pos_chg = gs[0] != before[0] || gs[1] != before[1] || gs[3] != before[3];
      if (area_chg || pos_chg) {
        ++hits;
        // 找該 tile 值在地圖上第一個座標(供 --map A --at X Y headless 驗證)。
        int fx = -1, fy = -1;
        for (int y = 0; y < lvl->h && fx < 0; ++y) for (int x = 0; x < lvl->w; ++x)
          if (lvl->tile(x, y) == v) { fx = x; fy = y; break; }
        std::printf("area %d \"%s\" tile 0x%02X @(%d,%d) pc 0x%04X:", area, lvl->name.c_str(), v, fx, fy, pc);
        if (area_chg) std::printf("  AREA %d->%d", before[2], gs[2]);
        if (gs[0] != before[0]) std::printf("  x %d->%d", before[0], gs[0]);
        if (gs[1] != before[1]) std::printf("  y %d->%d", before[1], gs[1]);
        if (gs[3] != before[3]) std::printf("  facing %d->%d", before[3], gs[3]);
        if (!area_chg && (gs[0] != before[0] || gs[1] != before[1]))
          std::printf("  [same-map walkable(%d,%d)=%d]", gs[0], gs[1],
                      (int)lvl->walkable(gs[0], gs[1]));
        if (area_chg) {
          auto dst = res::Level::load_file(bundle + "/maps/" + std::to_string(gs[2]) + ".lvl");
          if (dst) std::printf("  [dst \"%s\" %dx%d, in_bounds(%d,%d)=%d]",
                               dst->name.c_str(), dst->w, dst->h, gs[0], gs[1],
                               (int)dst->in_bounds(gs[0], gs[1]));
        }
        std::printf("  halt@0x%02X", ip.last_unimpl());
        if (!emitted.empty()) std::printf("  emit=\"%.50s\"", emitted.c_str());
        std::printf("\n");
      }
    }
  }
  std::printf("\n== %d area/pos-changing events found ==\n", hits);
  return 0;
}
