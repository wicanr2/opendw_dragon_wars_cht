// trace_quest_gates — 逆出主線 quest gate 的 flag 依賴:對每個 area 的每個事件格
//   (tile script),動態跑一遍,解碼所有「game_state bit」旗標操作:
//     op_9B set_gs_bit  (寫旗標 = 事件發生標記)
//     op_9D test_gs_bit (測旗標 → jnz/jz 分支 gate)
//     op_50 test_gs_bit (同上,bit 號取自 r2)
//     op_4E/op_4F set/clr_gs_bit (bit 號取自 r2)
//   並把每個旗標解碼成 (gs_byte, bit) = flag id,標記 SET / TEST。
//
//   旗標編碼(對照 interpreter get_bit_mask / op9B/9D/50/4E/4F):
//     bit_no(al) → byte_base(pcb) → flag_byte = byte_base + (al>>3),flag_bit = al&7。
//     op_9B/9D:al = script[pc+1](立即),pcb = script[pc+2]。
//     op_50/4E/4F:al = r2(執行前 TraceRec.r2),pcb = script[pc+1]。
//
//   這是 grounded gate 依賴圖的資料來源:哪個 area/tile「設」旗標、哪個「測」旗標
//   做分支(進度 gate)。op_8A encounter(= 戰鬥 gate,如終戰 Namtar)亦標記。
//
// 用法: trace_quest_gates <bundle_dir> [area ...]   (不給 area → 全 40 關)
#include <cstdio>
#include <cstdlib>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>
#include "../../src/resource/level.hpp"
#include "../../src/resource/provider.hpp"
#include "../../src/vm/interpreter.hpp"

using namespace dw;

struct FlagOp {
  int area, tile;
  std::uint16_t pc;
  char kind;            // 'S'=set(9b/4e) 'C'=clr(4f) 'T'=test(9d/50)
  int gs_byte, gs_bit;
  std::uint8_t op;
};

int main(int argc, char** argv) {
  if (argc < 2) {
    std::fprintf(stderr, "usage: %s <bundle_dir> [area ...]\n", argv[0]);
    return 2;
  }
  std::string bundle = argv[1];
  std::vector<int> areas;
  if (argc >= 3) for (int i = 2; i < argc; ++i) areas.push_back(std::atoi(argv[i]));
  else for (int a = 0; a < 40; ++a) areas.push_back(a);

  res::BundleProvider prov(bundle);
  std::vector<FlagOp> all;
  // (gs_byte,gs_bit) → 哪些 area set / 哪些 area test
  std::map<std::pair<int,int>, std::set<int>> setters, testers;
  std::map<int,int> encounter_tiles;  // area → encounter 觸發格數

  for (int area : areas) {
    auto lvl = res::Level::load_file(bundle + "/maps/" + std::to_string(area) + ".lvl");
    if (!lvl) { std::fprintf(stderr, "area %d load fail\n", area); continue; }
    int level_res = area + 0x46;
    std::printf("######## area %d 「%s」 %dx%d res=0x%X ########\n",
                area, lvl->name.c_str(), lvl->w, lvl->h, level_res);

    std::set<int> vals;
    for (int y = 0; y < lvl->h; ++y) for (int x = 0; x < lvl->w; ++x) {
      int t = lvl->tile(x, y); if (t > 1) vals.insert(t);
    }
    const auto& sc = lvl->data();

    for (int v : vals) {
      std::uint16_t pc = lvl->script_pc((std::uint8_t)v);
      if (pc == 0 || pc >= sc.size()) continue;

      vm::VmState st;
      st.script = sc; st.data_bytes = sc;
      st.script_res = level_res; st.data_res = level_res; st.pc = pc;
      st.game_state[2] = (std::uint8_t)area;
      st.game_state[0x57] = (std::uint8_t)area;
      st.resource_provider = [&](int tag) -> std::optional<std::vector<std::uint8_t>> {
        if (tag == level_res) return sc;
        return prov.load(tag);
      };
      vm::Trace tr;
      vm::Interpreter ip(st, &tr);
      ip.set_message_sink([](std::size_t, const std::string&) {});
      ip.run(200000);

      std::vector<FlagOp> tileops;
      for (const auto& r : tr.records()) {
        int al = -1, pcb = -1;
        char kind = 0;
        if (r.op == 0x9B || r.op == 0x9D) {
          // al = script[pc+1], pcb = script[pc+2]
          if (r.pc + 2 < sc.size()) { al = sc[r.pc + 1]; pcb = sc[r.pc + 2]; }
          kind = (r.op == 0x9B) ? 'S' : 'T';
        } else if (r.op == 0x50 || r.op == 0x4E || r.op == 0x4F) {
          // al = r2 (執行前), pcb = script[pc+1]
          al = r.r2 & 0xFF;
          if (r.pc + 1 < sc.size()) pcb = sc[r.pc + 1];
          kind = (r.op == 0x50) ? 'T' : (r.op == 0x4E ? 'S' : 'C');
        } else {
          continue;
        }
        if (al < 0 || pcb < 0) continue;
        int gb = pcb + (al >> 3);
        int bit = al & 7;
        FlagOp f{area, v, (std::uint16_t)r.pc, kind, gb, bit, r.op};   // r.pc 為 size_t → 明確窄化(AppleClang 嚴格,不容隱式)
        tileops.push_back(f);
        all.push_back(f);
        if (kind == 'S' || kind == 'C') setters[{gb, bit}].insert(area);
        if (kind == 'T') testers[{gb, bit}].insert(area);
      }
      // 偵測 encounter（op_8A）= 戰鬥 gate
      bool has_enc = false;
      for (const auto& r : tr.records()) if (r.op == 0x8A) { has_enc = true; break; }
      if (has_enc) encounter_tiles[area]++;

      if (!tileops.empty() || has_enc) {
        std::printf("  tile 0x%02X pc=0x%04X:", v, pc);
        for (auto& f : tileops)
          std::printf(" %c[gs%02X.%d]", f.kind, f.gs_byte, f.gs_bit);
        if (has_enc) std::printf(" {ENCOUNTER op_8A}");
        std::printf("\n");
      }
    }
    std::printf("\n");
  }

  std::printf("======== flag 依賴摘要(gs_byte.bit → set areas / test areas)========\n");
  std::set<std::pair<int,int>> allflags;
  for (auto& [k, _] : setters) allflags.insert(k);
  for (auto& [k, _] : testers) allflags.insert(k);
  for (auto& k : allflags) {
    std::printf("  flag gs%02X.%d :", k.first, k.second);
    std::printf(" SET={"); for (int a : setters[k]) std::printf("%d ", a); std::printf("}");
    std::printf(" TEST={"); for (int a : testers[k]) std::printf("%d ", a); std::printf("}\n");
  }
  std::printf("\n-- encounter(op_8A 戰鬥 gate)觸發格 --\n");
  for (auto& [a, c] : encounter_tiles) std::printf("  area %d : %d tile\n", a, c);
  std::printf("\n總計:flag 操作 %zu 次,唯一 flag %zu 個\n", all.size(), allflags.size());
  return 0;
}
