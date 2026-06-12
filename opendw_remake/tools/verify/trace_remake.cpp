// trace_remake — 用 remake VM 跑與 opendw trace_harness 相同的 bytecode,輸出相同格式 trace。
// 與 opendw 的輸出 diff = 差異測試(逐指令對拍 oracle)。
#include <cstdio>
#include "../../src/vm/interpreter.hpp"
using namespace dw::vm;
static const unsigned char prog[] = {
  0x00, 0x09,0x2A,0x00, 0x21, 0x22, 0x4B, 0x4C, 0x99, 0x53,0x14,0x00,
  0x09,0x63,0x00, 0x52,0x1A,0x00, 0x00,0x00, 0x09,0x07,0x00, 0x54, 0x00,0x00
};
int main(){
  VmState s; s.script.assign(prog, prog+sizeof(prog));
  Trace tr; Interpreter ip(s,&tr); ip.run();
  tr.dump(stdout);
  return 0;
}
