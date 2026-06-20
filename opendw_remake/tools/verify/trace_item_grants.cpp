// trace_item_grants — 逆出主線「物品/角色旗標取得端」在 level event script 裡到底出現了沒。
//   對全 40 關每個事件格(tile script)動態跑一遍,記錄所有「角色資料」相關 opcode:
//     op_5F or_char_data   (設角色 bit 屬性,如祝福 flags[85])
//     op_60 and_char_data  (清角色 bit 屬性)
//     op_61 test_char_prop (測角色 bit 屬性)
//     op_63 set_char_ext_word
//     op_68 get_char_ext   (讀物品記錄欄位)
//     op_69 set_char_ext   (寫物品記錄欄位 = 給/改物品)
//   並對 op_5F/60/61 解碼 (record_offset, bit) —— record_offset=0x55 = 祝福旗標。
//   同時記錄輸入/確認 gate(op_8C prompt_no_yes、op_8D read_string)以對照「取得端」是否
//   藏在玩家輸入分支。
//
//   解碼:op_5F/60/61 的 bit 號 = 執行前 r2(由前一條 op_09/0A 設);record offset = script[pc+1]
//   (get_bit_mask 讀的 1-byte operand)。
//
// 用法: trace_item_grants <bundle_dir> [area ...]   (不給 area → 全 40 關)
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>
#include "../../src/resource/level.hpp"
#include "../../src/resource/provider.hpp"
#include "../../src/vm/interpreter.hpp"

using namespace dw;

struct CharOp {
  int area, tile;
  std::uint16_t pc;
  std::uint8_t op;
  int rec_off;   // record offset (for 5F/60/61); -1 if n/a
  int bit;       // bit number (for 5F/60/61); -1 if n/a
};

static const char* opname(std::uint8_t op) {
  switch (op) {
    case 0x64: return "64 GIVE_ITEM (add 23B to empty slot)";
    case 0x65: return "65 take_item_check";
    case 0x67: return "67 DELETE_ITEM (compact slots)";
    case 0x5F: return "5F or_char (SET bit)";
    case 0x60: return "60 and_char (CLR bit)";
    case 0x61: return "61 test_char (TEST bit)";
    case 0x63: return "63 set_char_ext_word";
    case 0x68: return "68 get_char_ext (item READ)";
    case 0x69: return "69 set_char_ext (item WRITE)";
    case 0x8C: return "8C prompt_no_yes";
    case 0x8D: return "8D read_string";
    default: return "??";
  }
}

int main(int argc, char** argv) {
  if (argc < 2) {
    std::fprintf(stderr, "usage: %s <bundle_dir> [area ...]\n", argv[0]);
    return 2;
  }
  std::string bundle = argv[1];
  std::vector<int> areas;
  bool inject_yes = false;
  if (argc >= 3) {
    for (int i = 2; i < argc; ++i) {
      if (!std::strcmp(argv[i], "--yes")) { inject_yes = true; continue; }
      areas.push_back(std::atoi(argv[i]));
    }
  }
  if (areas.empty()) for (int a = 0; a < 40; ++a) areas.push_back(a);
  if (inject_yes) std::printf("[mode] op_8C/op_89 注入 'Y'(0xD9)走 Yes 分支\n\n");

  res::BundleProvider prov(bundle);
  std::vector<CharOp> all;
  std::map<std::uint8_t,int> op_counts;
  // (rec_off,bit) for 5F SET → which areas
  std::map<std::pair<int,int>, std::set<int>> bless_set, bless_test;

  for (int area : areas) {
    auto lvl = res::Level::load_file(bundle + "/maps/" + std::to_string(area) + ".lvl");
    if (!lvl) { std::fprintf(stderr, "area %d load fail\n", area); continue; }
    int level_res = area + 0x46;

    std::set<int> vals;
    for (int y = 0; y < lvl->h; ++y) for (int x = 0; x < lvl->w; ++x) {
      int t = lvl->tile(x, y); if (t > 1) vals.insert(t);
    }
    const auto& sc = lvl->data();
    bool area_header_printed = false;

    for (int v : vals) {
      std::uint16_t pc = lvl->script_pc((std::uint8_t)v);
      if (pc == 0 || pc >= sc.size()) continue;

      vm::VmState st;
      st.script = sc; st.data_bytes = sc;
      st.script_res = level_res; st.data_res = level_res; st.pc = pc;
      st.game_state[2] = (std::uint8_t)area;
      st.game_state[0x57] = (std::uint8_t)area;
      if (inject_yes) { st.headless_keys.assign(64, 0xD9); st.headless_key = 0xD9; }
      st.resource_provider = [&](int tag) -> std::optional<std::vector<std::uint8_t>> {
        if (tag == level_res) return sc;
        return prov.load(tag);
      };
      vm::Trace tr;
      vm::Interpreter ip(st, &tr);
      ip.set_message_sink([](std::size_t, const std::string&) {});
      ip.run(200000);
      std::uint8_t halt_op = ip.last_unimpl();
      if (halt_op == 0x64 || halt_op == 0x65 || halt_op == 0x67)
        std::printf("  !! area %d tile 0x%02X HALT on op_%02X (%s)\n",
                    area, v, halt_op, opname(halt_op));

      std::vector<CharOp> tileops;
      for (const auto& r : tr.records()) {
        std::uint8_t op = r.op;
        if (op != 0x5F && op != 0x60 && op != 0x61 && op != 0x63 &&
            op != 0x64 && op != 0x65 && op != 0x67 &&
            op != 0x68 && op != 0x69 && op != 0x8C && op != 0x8D)
          continue;
        int rec_off = -1, bit = -1;
        if (op == 0x5F || op == 0x60 || op == 0x61) {
          // bit = r2 (執行前); rec_off = script[pc+1] (get_bit_mask operand)
          bit = r.r2 & 0xFF;
          if (r.pc + 1 < sc.size()) rec_off = sc[r.pc + 1];
        }
        CharOp c{area, v, (std::uint16_t)r.pc, op, rec_off, bit};
        tileops.push_back(c);
        all.push_back(c);
        op_counts[op]++;
        if (op == 0x5F && rec_off >= 0) bless_set[{rec_off, bit}].insert(area);
        if (op == 0x61 && rec_off >= 0) bless_test[{rec_off, bit}].insert(area);
      }
      if (!tileops.empty()) {
        if (!area_header_printed) {
          std::printf("######## area %d 「%s」 res=0x%X ########\n",
                      area, lvl->name.c_str(), level_res);
          area_header_printed = true;
        }
        std::printf("  tile 0x%02X pc=0x%04X:\n", v, pc);
        for (auto& c : tileops) {
          std::printf("    pc=0x%04X op_%s", c.pc, opname(c.op));
          if (c.rec_off >= 0)
            std::printf("  [rec_off=0x%02X bit=%d]%s", c.rec_off, c.bit,
                        (c.rec_off == 0x55) ? "  <== flags[85] BLESSING" : "");
          std::printf("\n");
        }
      }
    }
  }

  std::printf("\n======== opcode 統計 ========\n");
  for (auto& [op, n] : op_counts)
    std::printf("  op_%02X %-26s : %d 次\n", op, opname(op), n);

  std::printf("\n======== 祝福旗標 op_5F SET (rec_off,bit → areas) ========\n");
  if (bless_set.empty()) std::printf("  (無 level script 在任何 tile 用 op_5F 設角色 bit 屬性)\n");
  for (auto& [k, areas2] : bless_set) {
    std::printf("  rec_off=0x%02X bit=%d : SET in areas {", k.first, k.second);
    for (int a : areas2) std::printf("%d ", a);
    std::printf("}\n");
  }
  std::printf("\n======== 祝福旗標 op_61 TEST (rec_off,bit → areas) ========\n");
  if (bless_test.empty()) std::printf("  (無 level script 在任何 tile 用 op_61 測角色 bit 屬性)\n");
  for (auto& [k, areas2] : bless_test) {
    std::printf("  rec_off=0x%02X bit=%d : TEST in areas {", k.first, k.second);
    for (int a : areas2) std::printf("%d ", a);
    std::printf("}\n");
  }
  std::printf("\n總計:角色資料/輸入 opcode 操作 %zu 次\n", all.size());
  return 0;
}
