// dump_event_catalog — 全 40 關每個事件格:座標 + T/S flag ops + emit 英文訊息。
//   供 grounded 物品/旗標 binding 編目(哪互動 → 該 set 哪 gate flag / 給哪物品)。
//   用法: dump_event_catalog <bundle> [area...]
#include <cstdio>
#include <cstdlib>
#include <set>
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

  for (int area : areas) {
    auto lvl = res::Level::load_file(bundle + "/maps/" + std::to_string(area) + ".lvl");
    if (!lvl) continue;
    int lr = area + 0x46;
    const auto& sc = lvl->data();
    // 逐「唯一 tile 值」跑一次(對拍 trace_quest_gates:不設玩家座標,讓 gate 測試分支被跑到);
    // 取該 tile 值第一個出現的座標供編目定位。
    std::set<int> seen_vals;
    for (int y = 0; y < lvl->h; ++y) for (int x = 0; x < lvl->w; ++x) {
      int v = lvl->tile(x, y); if (v <= 1) continue;
      if (seen_vals.count(v)) continue;
      seen_vals.insert(v);
      std::uint16_t pc = lvl->script_pc((std::uint8_t)v);
      if (pc == 0 || pc >= sc.size()) continue;
      vm::VmState st;
      st.script = sc; st.data_bytes = sc; st.script_res = lr; st.data_res = lr; st.pc = pc;
      st.game_state[2] = (std::uint8_t)area; st.game_state[0x57] = (std::uint8_t)area;
      st.resource_provider = [&](int t) -> std::optional<std::vector<std::uint8_t>> {
        if (t == lr) return sc; return prov.load(t); };
      vm::Trace tr;
      vm::Interpreter ip(st, &tr);
      std::string out;
      ip.set_message_sink([&](std::size_t, const std::string& s){ out += s; out += ' '; });
      ip.run(200000);
      // 收集 flag ops(解碼對拍 trace_quest_gates:gb = pcb + (al>>3),bit = al&7)。
      std::string flags;
      for (const auto& r : tr.records()) {
        int al = -1, pcb = -1; char kind = 0;
        if (r.op == 0x9B || r.op == 0x9D) {
          if (r.pc + 2 < sc.size()) { al = sc[r.pc + 1]; pcb = sc[r.pc + 2]; }
          kind = (r.op == 0x9B) ? 'S' : 'T';
        } else if (r.op == 0x50 || r.op == 0x4E || r.op == 0x4F) {
          al = r.r2 & 0xFF;
          if (r.pc + 1 < sc.size()) pcb = sc[r.pc + 1];
          kind = (r.op == 0x50) ? 'T' : (r.op == 0x4E ? 'S' : 'C');
        } else {
          if (r.op == 0x64) flags += "GIVE64 ";
          if (r.op == 0x8C) flags += "ASK ";
          if (r.op == 0x8A) flags += "ENC ";
          continue;
        }
        if (al < 0 || pcb < 0) continue;
        char buf[24];
        std::snprintf(buf, sizeof buf, "%c%02X.%d ", kind, pcb + (al >> 3), al & 7);
        flags += buf;
      }
      // 只印「有 flag 操作或訊息像取得/通行」的格(降噪)。
      std::printf("a%d t0x%02X (%d,%d) | %s| %.70s\n",
                  area, v, x, y, flags.c_str(), out.c_str());
    }
  }
  return 0;
}
