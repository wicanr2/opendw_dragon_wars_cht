// verify_combat_round — res3 全戰鬥閉環真值化守護(docs/42 §15)。
//
// 驅動 res3 戰鬥腳本完整一回合動作指派狀態機(逆向 res18/res4):
//   res18 主選單 Fight('F'0xC6)→ 逐角色 Attack('A'0xC1)×2 menu → res4 目標 → Y('Y'0xD9)確認
//   → res3 actor 迴圈 0x0075 → to-hit(0x0F73)→ 徒手傷害(0x0D54)→ 怪物 HP 扣減(0x07C7)。
//
// 斷言:
//   1. 抵達 res3 actor 迴圈(0x0075)。
//   2. 戰鬥核心子程式實際執行:to-hit(0x0F73)>0、徒手傷害(0x0D54)>0、HP 扣減(0x07C7)>0。
//   3. 怪物群緩衝 data[0x03D6] 的 HP byte 在回合中被扣減(結算前後有變化)。
//   4. 兩次固定 seed 執行軌跡 byte-identical(確定性)。
//   5. 全程無未實作 opcode(last_unimpl=0)。
//
// 注意:**不**斷言 HP 扣減的「具體數值 == opendw 真值」——opendw 無可獨立執行的
//   完整戰鬥 oracle 可逐回合 byte-diff(§9 限制)。本測試守護「閉環跑通 + 核心公式路徑
//   被執行 + 怪物 HP 確實變化 + 確定性」,不謊稱數值對拍。to-hit/傷害公式本身的數值
//   真值由 verify_combat_script(隔離子程式對拍)守護。
#include "resource/provider.hpp"
#include "vm/interpreter.hpp"
#include "vm/trace.hpp"
#include "game/party.hpp"

#include <cstdio>
#include <cstdlib>
#include <vector>

using namespace dw;

namespace {
int g_fail = 0;
void check(bool ok, const char* msg) {
  std::printf("  [%s] %s\n", ok ? "PASS" : "FAIL", msg);
  if (!ok) ++g_fail;
}

struct RoundResult {
  bool reached_actor = false;
  int tohit = 0, fist = 0, hpsub = 0, party_atk = 0;
  std::uint8_t last_unimpl = 0;
  bool halted = false;
  std::vector<std::uint8_t> snap_enter, snap_end;
  bool got_enter = false, got_end = false;
  std::vector<std::pair<std::size_t, std::uint8_t>> trace_ops;  // (pc,op) 供確定性比對
};

RoundResult run_round(const std::string& bundle) {
  RoundResult R;
  res::BundleProvider prov(bundle);
  auto script = prov.load(3);
  if (!script) return R;

  vm::VmState st;
  st.script = *script; st.data_bytes = *script;
  st.script_res = 3; st.data_res = 3; st.pc = 0;
  st.resource_provider = [&prov](int t) { return prov.load(t); };

  auto party = game::Party::load_default(bundle);
  auto recs = party.raw_records();
  int np = (int)recs.size();
  for (int i = 0; i < np && i < 7; ++i)
    for (int k = 0; k < 512; ++k) st.char_data[(std::size_t)i * 512 + k] = recs[i][k];

  st.game_state[0x1F] = (std::uint8_t)np;
  st.game_state[6] = 0;
  for (int i = 0; i < np; ++i) st.game_state[0x0A + i] = (std::uint8_t)(i * 2);
  st.r2 = 0;
  st.game_state[0x5A] = 31;     // 遭遇怪物資料資源 = res31
  st.fake_ticks = 0;
  st.headless_encounter = true;

  // 逐角色動作指派 + 目標選擇的 op_89 鍵驅動(表起點 pc = op89_addr + 3)。
  st.key_provider = [](int res, std::size_t pc) -> std::uint8_t {
    if (res == 18) {
      if (pc == 0xFB) return 0xC6;     // 主選單 Fight
      if (pc == 0x01ED) return 0xC1;   // 角色動作 Attack
      if (pc == 0x0494) return 0xC1;   // 攻擊風格 Attack blow
    }
    if (res == 4 && pc == 0x0080) return 0xB1;  // 目標:第 1 隻怪
    return 0;
  };
  st.headless_key = 0xD9;  // op_8C "Use these commands?" → Y

  vm::Trace tr;
  vm::Interpreter ip(st, &tr);
  ip.set_message_sink([](std::size_t, const std::string&) {});

  long total = 0;
  const std::size_t kMonBase = 0x03D6;
  while (total < 3000000 && !st.halted) {
    int did = ip.run(1);
    total += did;
    if (did == 0) break;
    if (!R.got_enter && st.script_res == 3 && st.pc == 0x0075 && st.data_res == 3) {
      R.snap_enter = st.data_bytes; R.got_enter = true;
    }
    if (R.got_enter && !R.got_end && st.script_res == 3 && st.pc == 0x009C && st.data_res == 3) {
      R.snap_end = st.data_bytes; R.got_end = true;
    }
    if (st.script_res == 3 && st.pc == 0x0075) R.reached_actor = true;
  }
  (void)kMonBase;
  R.last_unimpl = ip.last_unimpl();
  R.halted = st.halted;
  for (const auto& r : tr.records()) {
    if (r.pc == 0x0F73) ++R.tohit;
    if (r.pc == 0x0D54) ++R.fist;
    if (r.pc == 0x07C7) ++R.hpsub;
    if (r.pc == 0x0AFA) ++R.party_atk;
    R.trace_ops.emplace_back(r.pc, r.op);
  }
  return R;
}
}  // namespace

int main(int argc, char** argv) {
  if (argc < 2) { std::fprintf(stderr, "usage: %s <bundle>\n", argv[0]); return 2; }
  std::string bundle = argv[1];

  std::printf("== res3 全戰鬥閉環(actor 迴圈 → to-hit → 傷害 → 怪物 HP 扣減)==\n");
  RoundResult R = run_round(bundle);

  check(R.reached_actor, "抵達 res3 actor 迴圈 0x0075");
  check(R.last_unimpl == 0, "全程無未實作 opcode(last_unimpl=0)");
  check(R.tohit > 0, "to-hit 子程式(0x0F73)實際執行");
  check(R.fist > 0, "徒手傷害子程式(0x0D54)實際執行");
  check(R.hpsub > 0, "怪物 HP 扣減路徑(0x07C7)實際執行");
  check(R.party_atk > 0, "黨側攻擊(0x0AFA)實際執行");

  // 怪物群緩衝 HP 變化(結算前後 diff)。
  bool hp_changed = false;
  if (R.got_enter && R.got_end) {
    std::size_t lo = 0x03D6, hi = 0x03D6 + 0x21;  // 第 1 隻怪記錄
    for (std::size_t a = lo; a < hi && a < R.snap_enter.size() && a < R.snap_end.size(); ++a)
      if (R.snap_enter[a] != R.snap_end[a]) {
        std::printf("    怪物 byte[0x%02zx] @0x%04zx: 0x%02x -> 0x%02x\n",
                    a - 0x03D6, a, R.snap_enter[a], R.snap_end[a]);
        hp_changed = true;
      }
  }
  check(hp_changed, "怪物群緩衝 data[0x03D6] HP/狀態在回合中被扣減(結算前後有變化)");

  // 確定性:兩次執行軌跡 byte-identical。
  RoundResult R2 = run_round(bundle);
  bool deterministic = (R.trace_ops.size() == R2.trace_ops.size());
  if (deterministic)
    for (std::size_t i = 0; i < R.trace_ops.size(); ++i)
      if (R.trace_ops[i] != R2.trace_ops[i]) { deterministic = false; break; }
  check(deterministic, "兩次固定 seed 執行軌跡 byte-identical(確定性)");

  std::printf("== 統計:steps_tohit=%d fist=%d hpsub=%d party_atk=%d ==\n",
              R.tohit, R.fist, R.hpsub, R.party_atk);
  std::printf("%s\n", g_fail == 0 ? "verify_combat_round: PASS" : "verify_combat_round: FAIL");
  return g_fail == 0 ? 0 : 1;
}
