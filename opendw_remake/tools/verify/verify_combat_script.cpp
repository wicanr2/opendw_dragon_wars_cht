// verify_combat_script — 戰鬥腳本(原版 res3 bytecode)執行路徑的確定性 PASS/FAIL。
//
// ── 此 ctest 驗什麼(誠實界定)──────────────────────────────────────────
//  ✅ 驗:原版戰鬥 bytecode 能在 remake VM 上「確定性地」跑——固定 seed + 固定隊伍
//     + 固定注入鍵(Fight/Attack),跑到攻擊迴圈,兩次執行的指令軌跡 byte-identical;
//     且全程不踩未實作 opcode(守護 op_89/17/63/69/33-36/18/4D 等戰鬥路徑不回歸)。
//  ❌ 不驗:逐回合 HP 變化對拍 opendw byte-identical。原因(見 docs/42 §9):
//     怪物參戰角色(roster)由「res3@0x4c6 遭遇表 + res31 怪物記錄 + RNG」建立的
//     pipeline 尚未完整逆向(opendw monster_info.cpp 亦只部分 RE),且無「可獨立執行
//     的 opendw」可對一場完整戰鬥逐位元組對拍。故 combat.cpp 仍為乾淨室 placeholder。
// ───────────────────────────────────────────────────────────────────────
//
// 用法:verify_combat_script <bundle_dir>
// 退出碼:0=PASS,非 0=FAIL。

#include "resource/provider.hpp"
#include "vm/interpreter.hpp"
#include "vm/trace.hpp"
#include "game/party.hpp"

#include <cstdio>
#include <string>
#include <vector>

using namespace dw;

namespace {
int g_fail = 0;
void check(bool ok, const char* what) {
  std::printf("  [%s] %s\n", ok ? "PASS" : "FAIL", what);
  if (!ok) g_fail++;
}

// 跑一場戰鬥腳本(res3),回傳 (指令軌跡 hash 用的序列, 最終 last_unimpl, halted)。
struct RunResult {
  std::vector<std::uint32_t> trace;  // 每步 (op<<24)|(pc&0xFFFFFF) 摘要
  std::uint8_t last_unimpl = 0;
  bool halted = false;
  std::size_t steps = 0;
};

RunResult run_combat(const std::string& bundle, long max_steps) {
  res::BundleProvider prov(bundle);
  auto sc = prov.load(3);
  RunResult rr;
  if (!sc) return rr;
  vm::VmState st;
  st.script = *sc; st.data_bytes = *sc; st.script_res = 3; st.data_res = 3;
  st.resource_provider = [prov](int t) mutable { return prov.load(t); };
  auto party = game::Party::load_default(bundle);
  auto recs = party.raw_records(); int np = (int)recs.size();
  for (int i = 0; i < np && i < 7; i++)
    for (int k = 0; k < 512; k++) st.char_data[i * 512 + k] = recs[i][k];
  st.game_state[0x1F] = (std::uint8_t)np; st.game_state[6] = 0;
  for (int i = 0; i < np; i++) st.game_state[0x0A + i] = (std::uint8_t)(i * 2);
  st.random_seed = 0x1234;
  st.headless_encounter = true;
  st.headless_keys = {0xC6, 0xC1};  // Fight, Attack
  st.headless_key = 0xC1;
  vm::Trace tr;
  vm::Interpreter ip(st, &tr);
  rr.steps = (std::size_t)ip.run(max_steps);
  rr.last_unimpl = ip.last_unimpl();
  rr.halted = st.halted;
  for (const auto& r : tr.records())
    rr.trace.push_back(((std::uint32_t)r.op << 24) |
                       ((std::uint32_t)r.pc & 0x00FFFFFFu));
  return rr;
}
}  // namespace

int main(int argc, char** argv) {
  if (argc < 2) { std::fprintf(stderr, "usage: %s <bundle>\n", argv[0]); return 2; }
  const std::string bundle = argv[1];
  const long N = 40000;

  std::printf("== combat bytecode 確定性執行(res3,固定 seed/隊伍/鍵)==\n");

  auto a = run_combat(bundle, N);
  auto b = run_combat(bundle, N);

  check(a.steps > 2000,
        ("跑到攻擊迴圈(steps > 2000;實得 " + std::to_string(a.steps) + ")").c_str());
  check(a.last_unimpl == 0,
        ("全程無未實作 opcode(last_unimpl=0x" +
         [&] { char b2[8]; std::snprintf(b2, sizeof b2, "%02X", a.last_unimpl); return std::string(b2); }() +
         ")").c_str());
  check(a.trace == b.trace,
        ("兩次固定-seed 執行指令軌跡 byte-identical(" +
         std::to_string(a.trace.size()) + " 步)").c_str());

  std::printf("\n%s\n", g_fail == 0 ? "verify_combat_script: PASS"
                                    : "verify_combat_script: FAIL");
  return g_fail == 0 ? 0 : 1;
}
