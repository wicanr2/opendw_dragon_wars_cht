// trace_readpara — remake 側,跑與 oracle_readpara.c 相同的 Read-paragraph 路徑,
// 逐指令印出 (offset, op, r2, r4, flags, mode),供與 opendw oracle 對拍。
// 用法: trace_readpara <level.lvl> <data_dir> <area>
#include <cstdio>
#include <cstdlib>
#include <optional>
#include "../../src/resource/level.hpp"
#include "../../src/resource/provider.hpp"
#include "../../src/vm/interpreter.hpp"

int main(int argc, char** argv) {
  auto lvl = dw::res::Level::load_file(argv[1]);
  auto prov = dw::res::Data1Provider::open(argv[2]);
  int area = atoi(argv[3]);
  int level_res = area + 0x46;

  // 掃描第一個「58 08 15 00」(op_58 tag=8 src=0x15)當進入點(對齊 oracle)。
  std::uint16_t tile_pc[][2] = {{17,0x6EC},{29,0x507},{31,0x695},{32,0x4BD},{35,0x5B6},{36,0x3F2}};
  std::size_t start = 0;
  for (auto& m : tile_pc) if (m[0]==area) start = m[1];
  const auto& d = lvl->data();
  std::size_t pc = 0;
  for (std::size_t i = start; i+4 < d.size(); i++)
    if (d[i]==0x58 && d[i+1]==0x08 && d[i+2]==0x15 && d[i+3]==0x00) { pc=i; break; }

  dw::vm::VmState st;
  st.script = d; st.data_bytes = d;
  st.script_res = level_res; st.data_res = level_res;
  st.pc = pc; st.flags = 0x02;
  st.resource_provider = [&prov](int tag){ return prov->load(tag); };

  dw::vm::Trace tr;
  dw::vm::Interpreter ip(st, &tr);
  ip.set_message_sink([](std::size_t off, const std::string& s){
    std::fprintf(stderr, "  emit[%s]: \"%s\"\n",
                 off==dw::vm::Interpreter::kNumberSink?"NUM":"str", s.c_str());
  });
  ip.run();
  for (const auto& r : tr.records())
    std::printf("TR %04zx op=%02x r2=%04x r4=%04x fl=%04x m=%02x\n",
                r.pc, r.op, r.r2, r.r4, r.flags, r.mode);
  std::fprintf(stderr, "remake area %d entry op58@0x%04zx last_unimpl=0x%02x\n",
               area, pc, ip.last_unimpl());
  return 0;
}
