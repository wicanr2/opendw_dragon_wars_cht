// find_chests — 掃全 40 關每個事件 tile,跑其 script 捕捉 emit 文字,
//   報出訊息含 "locked chest" 的格(grounded 寶箱開箱 K 的觸發來源)。
//   用法: find_chests <bundle_dir> [area ...]
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include "../../src/resource/level.hpp"
#include "../../src/resource/provider.hpp"
#include "../../src/vm/interpreter.hpp"

using namespace dw;

int main(int argc, char** argv) {
  if (argc < 2) { std::fprintf(stderr, "usage: %s <bundle> [area...]\n", argv[0]); return 2; }
  std::string bundle = argv[1];
  std::vector<int> areas;
  for (int i = 2; i < argc; ++i) areas.push_back(std::atoi(argv[i]));
  if (areas.empty()) for (int a = 0; a < 40; ++a) areas.push_back(a);

  res::BundleProvider prov(bundle);
  int hits = 0;
  for (int area : areas) {
    auto lvl = res::Level::load_file(bundle + "/maps/" + std::to_string(area) + ".lvl");
    if (!lvl) continue;
    int level_res = area + 0x46;
    const auto& sc = lvl->data();
    for (int y = 0; y < lvl->h; ++y) for (int x = 0; x < lvl->w; ++x) {
      int v = lvl->tile(x, y); if (v <= 1) continue;
      std::uint16_t pc = lvl->script_pc((std::uint8_t)v);
      if (pc == 0 || pc >= sc.size()) continue;
      vm::VmState st;
      st.script = sc; st.data_bytes = sc;
      st.script_res = level_res; st.data_res = level_res; st.pc = pc;
      st.game_state[0] = (std::uint8_t)x; st.game_state[1] = (std::uint8_t)y;
      st.game_state[2] = (std::uint8_t)area; st.game_state[0x57] = (std::uint8_t)area;
      st.resource_provider = [&](int tag) -> std::optional<std::vector<std::uint8_t>> {
        if (tag == level_res) return sc; return prov.load(tag);
      };
      vm::Interpreter ip(st);
      std::string out;
      ip.set_message_sink([&](std::size_t, const std::string& s){ out += s; out += ' '; });
      ip.run(200000);
      if (out.find("ocked ches") != std::string::npos)
        std::printf("CHEST area %d tile 0x%02X @ (%d,%d): %.60s\n",
                    area, v, x, y, out.c_str());
      if (out.find("ocked ches") != std::string::npos) ++hits;
    }
  }
  std::printf("total chest tiles: %d\n", hits);
  return 0;
}
