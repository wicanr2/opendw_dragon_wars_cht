// probe_encounter_id — 跑指定 area 的每個事件格,開 headless_encounter,
// 報告哪個 tile 觸發 op_8A 以及其記錄的怪物 id(= word_3AE2 低位)。
// 用途:grounding「終戰 Namtar 遭遇格(area27 tile 0x18/0x19)的怪物 id 來源」。
//   不臆造:直接由 bytecode 跑出 op_8A 設定的 id。
// 用法: probe_encounter_id <bundle_dir> <area> [area ...]
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
  if (argc < 3) {
    std::fprintf(stderr, "usage: %s <bundle_dir> <area> [area ...]\n", argv[0]);
    return 2;
  }
  std::string bundle = argv[1];
  res::BundleProvider prov(bundle);
  for (int ai = 2; ai < argc; ++ai) {
    int area = std::atoi(argv[ai]);
    auto lvl = res::Level::load_file(bundle + "/maps/" + std::to_string(area) + ".lvl");
    if (!lvl) { std::fprintf(stderr, "area %d: load failed\n", area); continue; }
    int level_res = area + 0x46;
    std::printf("######## area %d  「%s」 %dx%d  res=0x%X ########\n",
                area, lvl->name.c_str(), lvl->w, lvl->h, level_res);
    std::set<int> vals;
    for (int y = 0; y < lvl->h; ++y)
      for (int x = 0; x < lvl->w; ++x) {
        int t = lvl->tile(x, y);
        if (t > 1) vals.insert(t);
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
      st.game_state[2] = (std::uint8_t)area;
      st.headless_encounter = true;   // op_8A 不 halt,記錄怪物 id 後續跑
      st.resource_provider =
          [&](int tag) -> std::optional<std::vector<std::uint8_t>> {
        if (tag == level_res) return lvl->data();
        return prov.load(tag);
      };
      vm::Interpreter ip(st);
      int steps = ip.run(200000);
      std::uint8_t unimpl = ip.last_unimpl();
      if (st.encounter_monster_id != 0xFF) {
        std::printf("  tile 0x%02X PC 0x%04X -> op_8A encounter monster_id=0x%02X (%d) "
                    "[steps=%d halt_op=0x%02X]\n",
                    v, pc, st.encounter_monster_id, st.encounter_monster_id, steps,
                    unimpl);
      }
    }
    std::printf("\n");
  }
  return 0;
}
