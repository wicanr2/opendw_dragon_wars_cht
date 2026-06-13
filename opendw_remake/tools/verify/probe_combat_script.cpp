// probe_combat_script — 把原版戰鬥腳本(res 3)餵進 remake VM,看跑到哪裡停。
//
// 目的(調查用,非 ctest):確認「戰鬥邏輯=原版 bytecode」是否能在 remake VM 跑通,
// 逐一記錄卡住的 opcode / 缺的子系統。對照 oracle:engine.c run_script。
//
// 設置(對照 script 3 入口的 game_state 依賴):
//   game_state[0x1F] = 隊伍人數;game_state[6] = 當前角色 index;
//   game_state[0x0A+i] = 角色 i 的 record selector(= record_index*2);
//   char_data = 預設隊伍 4×512B(data_C960 前 4 槽)。
//   word_3AE2 = 怪物 encounter id(op_8A 用);資源由 BundleProvider 供應。
//
// 用法:probe_combat_script <bundle_dir> [script_tag=3] [max_steps=20000]
#include "resource/provider.hpp"
#include "vm/interpreter.hpp"
#include "vm/trace.hpp"
#include "game/party.hpp"

#include <cstdio>
#include <cstdlib>

using namespace dw;

int main(int argc, char** argv) {
  if (argc < 2) { std::fprintf(stderr, "usage: %s <bundle> [tag] [max]\n", argv[0]); return 2; }
  std::string bundle = argv[1];
  int tag = argc > 2 ? std::atoi(argv[2]) : 3;
  long maxsteps = argc > 3 ? std::atol(argv[3]) : 20000;

  res::BundleProvider prov(bundle);
  auto script = prov.load(tag);
  if (!script) { std::fprintf(stderr, "load script tag %d failed\n", tag); return 1; }

  vm::VmState st;
  st.script = *script;
  st.data_bytes = *script;
  st.script_res = tag;
  st.data_res = tag;
  st.pc = 0;
  st.resource_provider = [&prov](int t) { return prov.load(t); };

  // 載入預設隊伍進 char_data(data_C960 前 4 槽)。
  auto party = game::Party::load_default(bundle);
  auto recs = party.raw_records();
  int np = (int)recs.size();
  for (int i = 0; i < np && i < 7; ++i)
    for (int k = 0; k < 512; ++k) st.char_data[(std::size_t)i * 512 + k] = recs[i][k];

  // game_state 初值(對照戰鬥入口)。
  st.game_state[0x1F] = (std::uint8_t)np;   // 隊伍人數
  st.game_state[6] = 0;                      // 當前角色
  for (int i = 0; i < np; ++i)
    st.game_state[0x0A + i] = (std::uint8_t)(i * 2);  // selector = record_index*2
  st.r2 = 0;  // 怪物 encounter id（先給 0；op_8A 在 remake 會 halt，見下分析）
  st.headless_encounter = true;  // op_8A 不 halt:略過圖形,跑戰鬥結算路徑

  vm::Trace tr;
  vm::Interpreter ip(st, &tr);
  ip.set_message_sink([](std::size_t, const std::string& s) {
    std::fprintf(stderr, "  [msg] %s\n", s.c_str());
  });
  int steps = ip.run(maxsteps);

  const auto& R = tr.records();
  std::printf("ran %d steps; halted=%d last_unimpl=0x%02X final_pc=0x%04zx\n",
              steps, st.halted ? 1 : 0, ip.last_unimpl(), st.pc);
  // 印最後 12 步軌跡。
  std::printf("-- last %d trace recs --\n", (int)(R.size() > 12 ? 12 : R.size()));
  for (std::size_t i = (R.size() > 12 ? R.size() - 12 : 0); i < R.size(); ++i)
    std::printf("  %04zx op=%02x r2=%04x r4=%04x fl=%04x m=%02x\n",
                R[i].pc, R[i].op, R[i].r2, R[i].r4, R[i].flags, R[i].mode);
  if (ip.last_unimpl())
    std::printf("STOPPED at unimplemented opcode 0x%02X (pc=0x%04zx)\n",
                ip.last_unimpl(), st.pc);
  return 0;
}
