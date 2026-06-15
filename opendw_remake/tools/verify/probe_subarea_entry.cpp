// probe_subarea_entry — 對指定 area 的每個特殊格,跑事件 script(BundleProvider 供 op_58),
// 觀測:gs[2] 是否變更(換區)、哪些 gs index 被寫、halt 於哪個未實作 opcode。
// 用途:判定 Phoebus(6)/Byzanople(9) 等子區入口在 remake VM 是否跑得通、卡在哪。
// 用法:probe_subarea_entry <bundle_dir> <area> [area...]
#include <cstdio>
#include <set>
#include <string>
#include <vector>
#include "../../src/resource/level.hpp"
#include "../../src/resource/provider.hpp"
#include "../../src/vm/interpreter.hpp"

using namespace dw;

int main(int argc, char** argv) {
  if (argc < 3) { std::fprintf(stderr, "usage: %s <bundle_dir> <area>...\n", argv[0]); return 2; }
  std::string bundle = argv[1];
  res::BundleProvider prov(bundle);

  for (int ai = 2; ai < argc; ++ai) {
    int area = std::atoi(argv[ai]);
    auto lvl = res::Level::load_file(bundle + "/maps/" + std::to_string(area) + ".lvl");
    if (!lvl) { std::printf("area %d: load fail\n", area); continue; }
    int level_res = area + 0x46;
    std::printf("=== area %d (%dx%d wraps=%d) ===\n", area, lvl->w, lvl->h, lvl->wraps()?1:0);
    std::set<int> vals;
    for (int y = 0; y < lvl->h; ++y) for (int x = 0; x < lvl->w; ++x) {
      int t = lvl->tile(x, y); if (t > 1) vals.insert(t);
    }
    for (int v : vals) {
      std::uint16_t pc = lvl->script_pc((std::uint8_t)v);
      if (pc == 0 || pc >= lvl->data().size()) continue;
      vm::VmState st;
      st.script = lvl->data(); st.data_bytes = lvl->data();
      st.script_res = level_res; st.data_res = level_res; st.pc = pc;
      st.game_state[2] = (std::uint8_t)area;
      st.game_state[0x57] = (std::uint8_t)area;
      // 記錄 gs 寫入:用一個拷貝對比(run 後 diff)。
      auto before = st.game_state;
      std::set<int> op58_tags;
      st.resource_provider = [&](int tag) -> std::optional<std::vector<std::uint8_t>> {
        op58_tags.insert(tag);
        if (tag == level_res) return lvl->data();
        if (auto b = prov.load(tag)) return b;
        return std::nullopt;
      };
      vm::Interpreter ip(st);
      ip.set_message_sink([](std::size_t, const std::string&) {});
      int steps = ip.run(20000);  // 步數上限避免壞跳轉
      // diff gs
      std::vector<int> changed;
      for (int i = 0; i < 256; ++i) if (st.game_state[i] != before[i]) changed.push_back(i);
      std::printf("  tile 0x%02X pc=0x%04X steps=%d halt_unimpl=0x%02X gs2:%d->%d",
                  v, pc, steps, ip.last_unimpl(),
                  area, st.game_state[2]);
      if (st.game_state[2] != (std::uint8_t)area) std::printf("  *** AREA CHANGE ***");
      std::printf("\n    gs writes:");
      for (int i : changed) std::printf(" [%02X]=%02X", i, st.game_state[i]);
      std::printf("\n    op58 tags called:");
      for (int t : op58_tags) std::printf(" %d", t);
      std::printf("\n");
    }
  }
  return 0;
}
