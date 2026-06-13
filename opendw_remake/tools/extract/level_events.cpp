// level_events — 把關卡的「特殊格 → 事件腳本」跑出來,看 emit 什麼字串。
//
// 對拍 opendw op_71/run_level_script:特殊格 tile 值 v → script 表 entry
// (grid+w*3*h)+(v+1)*2 → 16-bit script PC → 在 VM 跑該關 bytecode,
// 攔截 emit 的字串(含「Read paragraph N」),用以驗證 tile→事件→段落,
// 並與《軟體世界》攻略的「訊息 N」比對。
//
// 用法: level_events <level.lvl> [i18n.tsv]
#include <cstdio>
#include <set>
#include <string>
#include "../../src/resource/level.hpp"
#include "../../src/vm/interpreter.hpp"

int main(int argc, char** argv) {
  if (argc < 2) { std::fprintf(stderr, "usage: %s <level.lvl>\n", argv[0]); return 2; }
  auto lvl = dw::res::Level::load_file(argv[1]);
  if (!lvl) { std::fprintf(stderr, "load level failed\n"); return 1; }
  std::printf("關卡「%s」 %dx%d  script 表@0x%zX\n", lvl->name.c_str(), lvl->w, lvl->h, lvl->script_table());

  // 收集地圖上實際出現的特殊格值
  std::set<int> vals;
  for (int y = 0; y < lvl->h; ++y) for (int x = 0; x < lvl->w; ++x) {
    int t = lvl->tile(x, y); if (t > 1) vals.insert(t);
  }
  std::printf("出現的特殊格值:"); for (int v : vals) std::printf(" 0x%02X", v); std::printf("\n\n");

  for (int v : vals) {
    std::uint16_t pc = lvl->script_pc((std::uint8_t)v);
    std::printf("── tile 0x%02X → script PC 0x%04X ──\n", v, pc);
    if (pc == 0 || pc >= lvl->data().size()) { std::printf("  (PC 無效)\n"); continue; }
    dw::vm::VmState st; st.script = lvl->data(); st.pc = pc;
    dw::vm::Interpreter ip(st);
    int n = 0;
    ip.set_message_sink([&](std::size_t, const std::string& s) { std::printf("  emit: \"%s\"\n", s.c_str()); ++n; });
    int steps = ip.run();
    if (n == 0) std::printf("  (跑 %d 步,停於未實作 opcode 0x%02X;未 emit)\n", steps, ip.last_unimpl());
  }
  return 0;
}
