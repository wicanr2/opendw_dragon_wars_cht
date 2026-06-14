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

  // ── 徒手傷害公式對拍 res3 bytecode(關鍵:證明 combat.cpp 徒手式 = 原版真值)──
  //   直接跑 res3 0x0D54 徒手傷害路徑(STR/Fist 受控、強制 1 次命中),讀最終 gs[0x5d],
  //   掃多 seed 取分布,對拍「1d4 + floor(STR/5)」的理論範圍(combat.cpp 公式)。
  std::printf("== 徒手傷害對拍 res3 bytecode(dmg = 1d4 + floor(STR/5))==\n");
  {
    res::BundleProvider prov(bundle);
    auto sc = prov.load(3);
    auto run_fist = [&](int str, int fist, int& mn, int& mx, bool& saw5) {
      mn = 9999; mx = -1; saw5 = false;
      for (int s = 0; s < 6000; ++s) {
        vm::VmState st;
        st.script = *sc; st.data_bytes = *sc; st.script_res = 3; st.data_res = 3;
        st.resource_provider = [prov](int t) mutable { return prov.load(t); };
        st.random_seed = (std::uint16_t)(0x0001 + s * 2654435761u);
        st.fake_ticks = (std::uint16_t)(s * 40503u);
        st.game_state[6] = 0; st.game_state[0x0A] = 0;
        st.char_data[0x0c] = (std::uint8_t)str;   // STR
        st.char_data[0x27] = (std::uint8_t)fist;  // Fistfighting
        st.game_state[0x66] = 0xFF;               // 徒手
        st.game_state[0x7e] = 1;                  // 命中 1 次
        st.game_state[0x84] = 0;
        st.pc = 0x0D54;
        vm::Interpreter ip(st);
        for (int step = 0; step < 400; ++step) {
          if (st.halted) break;
          std::size_t before = st.pc;
          ip.run(1);
          if (before == 0x0DD2) break;
        }
        int d = st.game_state[0x5d] | (st.game_state[0x5e] << 8);
        if (d < mn) mn = d; if (d > mx) mx = d; if (d == 5) saw5 = true;
      }
    };
    int mn, mx; bool saw5;
    // STR10 Fist0 → 1d4 + 2 = [3,6] 含 5
    run_fist(10, 0, mn, mx, saw5);
    check(mn == 3 && mx == 6, ("bytecode STR10 徒手範圍 [3,6](實得 [" +
          std::to_string(mn) + "," + std::to_string(mx) + "])").c_str());
    check(saw5, "bytecode STR10 徒手含傷害值 5(證偽 DOS『無 5』)");
    // STR20 Fist0 → 1d4 + 4 = [5,8]
    run_fist(20, 0, mn, mx, saw5);
    check(mn == 5 && mx == 8, "bytecode STR20 徒手範圍 [5,8](= 1d4 + floor(20/5)=4)");
    // STR4 Fist0 → 1d4 + 0 = [1,4](證偽 floor(3))
    run_fist(4, 0, mn, mx, saw5);
    check(mn == 1 && mx == 4, "bytecode STR4 徒手範圍 [1,4](無 floor(3))");
  }

  std::printf("\n%s\n", g_fail == 0 ? "verify_combat_script: PASS"
                                    : "verify_combat_script: FAIL");
  return g_fail == 0 ? 0 : 1;
}
