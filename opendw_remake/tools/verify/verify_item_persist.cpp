// verify_item_persist — 端到端驗證:op_64 給物品經 run_event 的 char 同步 → 持久進
//   512B party record 背包(offset 236 = 0xEC + slot*23)。對齊 main.cpp run_event 的
//   「事件前載 party→char_data/char_ext、事件後寫回」邏輯,證明給物品端到端持久化。
#include <cstdio>
#include <array>
#include <vector>
#include "vm/interpreter.hpp"
#include "vm/vm_state.hpp"
using namespace dw;
static int g_fail = 0;
#define CHECK(c,m) do{ if(!(c)){std::printf("FAIL: %s\n",m);++g_fail;} else std::printf("ok: %s\n",m);}while(0)

int main() {
  // 1) 一名隊員的 512B record(背包空:offset 236+ 全 0)。
  std::array<std::uint8_t,512> rec{};
  // 2) run_event 前同步:record → char_data[0..511];char_ext[k]=char_data[0xEC+k];
  //    gs[0x1F]=1(人數)、gs[6]=0(當前角色)、gs[0x0A]=0(selector=0)。
  vm::VmState st;
  st.game_state[0x1F]=1; st.game_state[6]=0; st.game_state[0x0A]=0;
  for (int b=0;b<512;++b) st.char_data[(std::size_t)b]=rec[(std::size_t)b];
  for (std::size_t k=0;k+0xEC<st.char_data.size()&&k<st.char_ext.size();++k)
    st.char_ext[k]=st.char_data[k+0xEC];
  // 3) 事件 bytecode:模板資源(data_bytes)@0x40 放 23B 物品;op_09 r2=0x40;op_64 給物品;halt。
  st.data_bytes.assign(0x80,0);
  for (int j=0;j<0x17;++j) st.data_bytes[0x40+j]=(std::uint8_t)(0xC0+j);  // 可辨識 23B 模板
  st.script={0x09,0x40, 0x64, 0x5A};
  vm::Interpreter(st).run();
  CHECK(st.halted, "事件 script 正常結束");
  CHECK(st.game_state[7]==0, "op_64 給到空背包 slot 0(gs[7]=0)");
  // 4) run_event 後同步:char_ext → char_data[0xEC+];char_data → record。
  for (std::size_t k=0;k+0xEC<st.char_data.size()&&k<st.char_ext.size();++k)
    st.char_data[k+0xEC]=st.char_ext[k];
  bool changed=false;
  for (int b=0;b<512;++b) if (rec[(std::size_t)b]!=st.char_data[(std::size_t)b]){rec[(std::size_t)b]=st.char_data[(std::size_t)b];changed=true;}
  CHECK(changed, "char_data 有變動(物品已寫入)");
  // 5) 驗 record 背包 slot 0(offset 236 = 0xEC):23B 物品模板已持久。
  bool ok=true;
  for (int j=0;j<0x17;++j) ok = ok && (rec[(std::size_t)(236+j)]==(std::uint8_t)(0xC0+j));
  CHECK(ok, "512B record 背包 [236..258] = 給予的物品(端到端持久化)");
  if (g_fail==0){ std::printf("\nITEM PERSIST PASS\n"); return 0; }
  std::printf("\nITEM PERSIST FAIL (%d)\n", g_fail); return 1;
}
