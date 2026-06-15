// trace_subarea_dyn — 動態逐指令追蹤母區事件格 + 跨資源 call(op_58)進入的
//   resource 8/9 子常式,印出每條指令(含 op_58 目標資源+offset、op_1A/op_19
//   寫的 gs index/值、gs[2] 變化、最終落點)。
//
//   目的:逆出 Phoebus(6)/Byzanople(9)的進城機制 —— 母區(8/29)哪格、跑到
//   resource 8/9 的哪段、它怎麼決定目的地 area、是否被 NULL opcode 擋住。
//
// 用法: trace_subarea_dyn <bundle_dir> <area> [--tile 0xNN] [--max N] [--full]
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <set>
#include <string>
#include <vector>
#include "../../src/resource/level.hpp"
#include "../../src/resource/provider.hpp"
#include "../../src/vm/interpreter.hpp"

using namespace dw;

static const char* opname(std::uint8_t op) {
  switch (op) {
    case 0x00: return "word_mode"; case 0x01: return "byte_mode";
    case 0x05: return "ld_gs_r4"; case 0x06: return "imm_r4";
    case 0x09: return "set_r2"; case 0x0A: return "r2_from_gs";
    case 0x0B: return "r2_from_gs_off"; case 0x0C: return "r2_from_data";
    case 0x0D: return "r2_from_data_off"; case 0x0E: return "r2_data_gsoff";
    case 0x0F: return "r2_from_res";
    case 0x10: return "r2_data_gs"; case 0x11: return "gs_from_ah"; case 0x12: return "gs_from_r2";
    case 0x13: return "gs_off_from_r2"; case 0x14: return "data_from_r2";
    case 0x19: return "gs_copy"; case 0x1A: return "gs_imm";
    case 0x1C: return "data_store";
    case 0x23: return "inc_gs"; case 0x26: return "dec_gs";
    case 0x30: return "rcr_add_imm"; case 0x32: return "rcr_sub_imm";
    case 0x33: return "mul_gs"; case 0x34: return "mul_imm";
    case 0x35: return "div_gs"; case 0x36: return "div_imm";
    case 0x38: return "and"; case 0x39: return "or_gs"; case 0x3A: return "or_imm";
    case 0x3B: return "xor_gs"; case 0x3C: return "xor_imm";
    case 0x3D: return "cmp_gs"; case 0x3E: return "cmp_imm";
    case 0x40: return "cmp_r4_imm"; case 0x3F: return "cmp_r4_gs";
    case 0x41: return "jnc"; case 0x42: return "jc"; case 0x43: return "jump_above";
    case 0x44: return "jz"; case 0x45: return "jnz"; case 0x46: return "js"; case 0x47: return "jns";
    case 0x48: return "set_gs_msb"; case 0x49: return "loop"; case 0x4A: return "loop_eq";
    case 0x4B: return "stc"; case 0x4C: return "clc"; case 0x4D: return "prng";
    case 0x4E: return "set_gs_bit"; case 0x4F: return "clr_gs_bit"; case 0x50: return "test_gs_bit";
    case 0x52: return "jmp"; case 0x53: return "call"; case 0x54: return "ret";
    case 0x58: return "XCALL"; case 0x59: return "XRET"; case 0x5A: return "RET_SCRIPT";
    case 0x5B: return "get_map_tile"; case 0x5C: return "party_loop";
    case 0x60: return "and_char"; case 0x66: return "test_gs";
    case 0x68: return "get_char_ext(NULL?)"; case 0x69: return "set_char_ext";
    case 0x6B: return "op6B(NULL?)"; case 0x70: return "op70(NULL?)";
    case 0x71: return "op71_tile_event"; case 0x73: return "clear_event";
    case 0x74: return "draw_frame"; case 0x75: return "ui_full"; case 0x76: return "draw_pattern";
    case 0x77: return "draw_set_str"; case 0x78: return "set_msg"; case 0x79: return "draw_emit";
    case 0x7A: return "emit_data_str"; case 0x7B: return "ui_header"; case 0x7C: return "ui_header_data";
    case 0x80: return "advance_cursor"; case 0x81: return "print_num"; case 0x83: return "print_char";
    case 0x88: return "wait_escape"; case 0x89: return "wait_event"; case 0x8A: return "encounter";
    case 0x8B: return "refresh_vp"; case 0x8C: return "prompt_no_yes"; case 0x90: return "sound";
    case 0x9A: return "set_gs_ff"; case 0x9B: return "set_gs_bit"; case 0x99: return "test_r2";
    case 0x9D: return "test_gs_bit";
    default: return "?";
  }
}

int main(int argc, char** argv) {
  if (argc < 3) {
    std::fprintf(stderr, "usage: %s <bundle_dir> <area> [--tile 0xNN] [--max N] [--full]\n", argv[0]);
    return 2;
  }
  std::string bundle = argv[1];
  int area = (int)std::strtol(argv[2], nullptr, 0);
  int only_tile = -1; long maxsteps = 200000; bool full = false;
  std::vector<std::uint8_t> keys;  // 注入鍵序列(op_89/op_8C 取用)
  for (int i = 3; i < argc; ++i) {
    if (!std::strcmp(argv[i], "--tile") && i + 1 < argc) only_tile = (int)std::strtol(argv[++i], nullptr, 0);
    else if (!std::strcmp(argv[i], "--max") && i + 1 < argc) maxsteps = std::atol(argv[++i]);
    else if (!std::strcmp(argv[i], "--full")) full = true;
    // --yes:對所有 op_89/op_8C 提示注入 'Y'|0x80=0xD9(玩家確認進城)。
    else if (!std::strcmp(argv[i], "--yes")) keys.assign(64, 0xD9);
    // --key 0xNN:單一鍵重複注入(如 Enter='E'|0x80=0xC5、'1'|0x80=0xB1)。
    else if (!std::strcmp(argv[i], "--key") && i + 1 < argc) keys.assign(64, (std::uint8_t)std::strtol(argv[++i], nullptr, 0));
  }

  res::BundleProvider prov(bundle);
  auto lvl = res::Level::load_file(bundle + "/maps/" + std::to_string(area) + ".lvl");
  if (!lvl) { std::printf("area %d load fail\n", area); return 1; }
  int level_res = area + 0x46;
  std::printf("=== area %d (%dx%d wraps=%d) level_res=0x%X ===\n",
              area, lvl->w, lvl->h, lvl->wraps() ? 1 : 0, level_res);

  std::set<int> vals;
  for (int y = 0; y < lvl->h; ++y) for (int x = 0; x < lvl->w; ++x) {
    int t = lvl->tile(x, y); if (t > 1) vals.insert(t);
  }

  for (int v : vals) {
    if (only_tile >= 0 && v != only_tile) continue;
    std::uint16_t pc = lvl->script_pc((std::uint8_t)v);
    if (pc == 0 || pc >= lvl->data().size()) continue;

    vm::VmState st;
    st.script = lvl->data(); st.data_bytes = lvl->data();
    st.script_res = level_res; st.data_res = level_res; st.pc = pc;
    st.game_state[2] = (std::uint8_t)area;
    st.game_state[0x57] = (std::uint8_t)area;
    st.headless_keys = keys;  // 注入鍵(空 = 維持 op_89/op_8C 既有 halt/No 行為)
    auto before = st.game_state;

    std::set<int> op58_tags;
    st.resource_provider = [&](int tag) -> std::optional<std::vector<std::uint8_t>> {
      op58_tags.insert(tag);
      if (tag == level_res) return lvl->data();
      if (auto b = prov.load(tag)) return b;
      return std::nullopt;
    };

    vm::Trace tr;
    vm::Interpreter ip(st, &tr);
    ip.set_message_sink([](std::size_t, const std::string&) {});
    // 記錄每個 op_58 的 (序號→tag,offset),供逐指令重放時標出進入的資源。
    std::vector<std::pair<int,std::uint16_t>> xcalls;  // 依執行順序
    ip.set_xcall_observer([&](int tag, std::uint16_t off, std::uint16_t) {
      xcalls.push_back({tag, off});
    });

    int steps = ip.run(maxsteps);

    std::printf("\n--- tile 0x%02X pc=0x%04X steps=%d halt_unimpl=0x%02X gs2:%d->%d%s ---\n",
                v, pc, steps, ip.last_unimpl(), area, st.game_state[2],
                st.game_state[2] != (std::uint8_t)area ? "  *** AREA CHANGE ***" : "");
    std::printf("    op58 tags: "); for (int t : op58_tags) std::printf("%d ", t); std::printf("\n");
    // gs diff
    std::printf("    gs writes:");
    for (int i = 0; i < 256; ++i) if (st.game_state[i] != before[i])
      std::printf(" [%02X]=%02X", i, st.game_state[i]);
    std::printf("\n");

    // 逐指令重放 trace,用 op_58/op_59 維護資源棧(op_58 目標取自 xcalls 序列)。
    const auto& recs = tr.records();
    std::vector<int> resstack; int cur_res = level_res; std::size_t xc_idx = 0;
    long printed = 0, limit = full ? 100000 : 800;
    for (std::size_t i = 0; i < recs.size() && printed < limit; ++i) {
      const auto& r = recs[i];
      char tgt[32] = "";
      if (r.op == 0x58 && xc_idx < xcalls.size())
        std::snprintf(tgt, sizeof tgt, " -> res%d@%04x", xcalls[xc_idx].first, xcalls[xc_idx].second);
      std::printf("    [res%3d] %04zx %02x %-18s%s  r2=%04x r4=%04x fl=%04x m=%02x\n",
                  cur_res, r.pc, r.op, opname(r.op), tgt, r.r2, r.r4, r.flags, r.mode);
      ++printed;
      if (r.op == 0x58) {
        resstack.push_back(cur_res);
        cur_res = (xc_idx < xcalls.size()) ? xcalls[xc_idx].first : -2;
        ++xc_idx;
      } else if (r.op == 0x59) {
        if (!resstack.empty()) { cur_res = resstack.back(); resstack.pop_back(); }
      }
    }
    if (printed >= limit) std::printf("    ... (trace truncated at %ld; use --full)\n", limit);
  }
  return 0;
}
