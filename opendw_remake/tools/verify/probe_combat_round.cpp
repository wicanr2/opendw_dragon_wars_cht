// probe_combat_round — 驅動 res3 戰鬥腳本完整一回合動作指派,抵達 actor 迴圈(0x0075),
//   觀察對 data[0x03D6] 怪物群緩衝區的 HP 結算(res3 全戰鬥閉環真值化,docs/42 §15)。
//
// 收斂鏈(逆向 res18/res4 動作指派狀態機):
//   res18 主選單 @0x00F8: Fight 'F'(0xC6) → 0x0108: gs[0x67]=0(逐角色動作 index)。
//   逐角色 @0x0111: 角色動作選單 @0x01EA: Attack 'A'(0xC1) → 0x03BC → 攻擊風格 @0x0491:
//     'A'(0xC1) → 0x04AB → 0x04B4 op_58 res4(目標選擇)。
//   res4 目標 @0x007D: 數字鍵('1'|0x80=0xB1)選怪 → 0x008F clc retf → res18 0x04B8 jc 不跳 →
//     0x04C0 jmp 0x023E: inc gs[0x67]。
//   gs[0x67] < gs[0x1F] → 回 0x0111(下一角色);== → 0x0247 "Use these commands?" Y/N:
//     'Y'(0xD9,op_8C) → 0x0265 clc retf → 回 res3 0x0069 → 0x006C res3@0x08b6 → 0x0075 actor 迴圈。
#include "resource/provider.hpp"
#include "vm/interpreter.hpp"
#include "vm/trace.hpp"
#include "game/party.hpp"

#include <cstdio>
#include <cstdlib>
#include <vector>

using namespace dw;

static constexpr std::size_t kMonBase = 0x03D6;
static constexpr std::size_t kRecLen = 0x21;  // 怪物記錄 33 byte

static void dump_mon(const std::vector<std::uint8_t>& d, int nrec, const char* tag) {
  std::printf("== [%s] data[0x03D6] %d 隻怪物記錄(每 0x21 byte)==\n", tag, nrec);
  for (int m = 0; m < nrec; ++m) {
    std::size_t base = kMonBase + (std::size_t)m * kRecLen;
    std::printf("  m%-2d: ", m);
    for (std::size_t i = 0; i < kRecLen && base + i < d.size(); ++i)
      std::printf("%02x ", d[base + i]);
    std::printf("\n");
  }
}

int main(int argc, char** argv) {
  if (argc < 2) { std::fprintf(stderr, "usage: %s <bundle> [max]\n", argv[0]); return 2; }
  std::string bundle = argv[1];
  long maxsteps = argc > 2 ? std::atol(argv[2]) : 3000000;

  res::BundleProvider prov(bundle);
  auto script = prov.load(3);
  if (!script) { std::fprintf(stderr, "load res3 failed\n"); return 1; }

  vm::VmState st;
  st.script = *script;
  st.data_bytes = *script;
  st.script_res = 3;
  st.data_res = 3;
  st.pc = 0;
  st.resource_provider = [&prov](int t) { return prov.load(t); };

  auto party = game::Party::load_default(bundle);
  auto recs = party.raw_records();
  int np = (int)recs.size();
  for (int i = 0; i < np && i < 7; ++i)
    for (int k = 0; k < 512; ++k) st.char_data[(std::size_t)i * 512 + k] = recs[i][k];

  st.game_state[0x1F] = (std::uint8_t)np;
  st.game_state[6] = 0;
  for (int i = 0; i < np; ++i)
    st.game_state[0x0A + i] = (std::uint8_t)(i * 2);
  int enc = 0;
  st.r2 = (std::uint16_t)enc;          // encounter id(op_8A 用)
  st.game_state[0x5A] = 31;            // 遭遇怪物資料資源 = res31(對照 engine.c:5452 gs[0x5A])
  st.fake_ticks = (std::uint16_t)enc;  // op_4D RNG 可重現
  // 目標選擇 context(原版由 engine C 在遭遇/選目標時設):
  //   gs[0x92]=3 → res4 直接 retf clc、target = gs[0x81];gs[0x81]=0 = 鎖定第 1 隻怪。
  //   讓 headless 戰鬥有「具體單體目標」→ 攻擊解算(to-hit/傷害)能對該怪執行。
  if (getenv("DWTGT3")) { st.game_state[0x92] = 3; st.game_state[0x81] = 0; }
  st.headless_encounter = true;

  bool log_keys = getenv("DWLOGKEYS") != nullptr;
  st.key_provider = [&](int res, std::size_t pc) -> std::uint8_t {
    std::uint8_t k = 0;
    if (res == 18) {
      if (pc == 0xFB) k = 0xC6;        // 主選單 @0xF8 Fight
      else if (pc == 0x01ED) k = 0xC1; // 角色動作 @0x01EA:Attack
      else if (pc == 0x0494) k = 0xC1; // 攻擊風格 @0x0491:Attack blow
    } else if (res == 4 && pc == 0x0080) k = 0xB1;  // 目標 @0x007D:第 1 隻怪
    if (log_keys && k) std::printf("  [key] res%d pc=0x%04zx -> 0x%02X\n", res, pc, k);
    else if (log_keys) std::printf("  [key?] res%d pc=0x%04zx -> (default)\n", res, pc);
    return k;
  };
  st.headless_key = 0xD9;  // op_8C "Use these commands?" → Y

  vm::Trace tr;
  vm::Interpreter ip(st, &tr);
  ip.set_message_sink([](std::size_t, const std::string&) {});

  // 持續維護「最近一次 res3 自身 data_bytes」快照(怪物緩衝在 res3 內);
  //   並在「首次抵達 actor 迴圈 0x0075」與「結束」各 dump 一次。
  std::vector<std::uint8_t> snap_enter, snap_end;
  bool got_enter = false, got_end = false, reached_actor = false;
  int monster_count = 0;
  long total = 0;
  while (total < maxsteps && !st.halted) {
    int did = ip.run(1);
    total += did;
    if (did == 0) break;
    // 首次抵達 actor 迴圈頂(res3 0x0075):快照「結算前」怪物緩衝。
    if (!got_enter && st.script_res == 3 && st.pc == 0x0075 && st.data_res == 3) {
      snap_enter = st.data_bytes;
      monster_count = (kMonBase + 0x0a < snap_enter.size())
                          ? (snap_enter[kMonBase + 0x0a] & 0x1f) : 0;
      got_enter = true;
      std::printf("== 抵達 res3 actor 迴圈 0x0075(step %ld);怪物群 count(byte0a&0x1f)=%d ==\n",
                  total, monster_count);
      std::printf("   gs[0x72]=%02x gs[0x73]=%02x gs[0x74]=%02x | gs[0x75]=%02x gs[0x76]=%02x gs[0x77]=%02x\n",
                  st.game_state[0x72], st.game_state[0x73], st.game_state[0x74],
                  st.game_state[0x75], st.game_state[0x76], st.game_state[0x77]);
      std::printf("   黨動作陣列 data[0x04EA..0x04EE]: ");
      for (std::size_t i = 0; i < 6; ++i)
        std::printf("%02x ", st.data_bytes[0x04EA + i]);
      std::printf("\n   黨動作記錄 data[0x04CE..0x04D6]: ");
      for (std::size_t i = 0; i < 8; ++i)
        std::printf("%02x ", st.data_bytes[0x04CE + i]);
      std::printf("\n");
    }
    // 首次抵達 actor 迴圈結束(res3 0x009C,兩側 actor 耗盡 → 回合結束,清理前):
    //   快照「結算後」——此時 HP 扣減已完成、尚未被回合 teardown 清陣列。
    if (got_enter && !got_end && st.script_res == 3 && st.pc == 0x009C && st.data_res == 3) {
      snap_end = st.data_bytes;
      got_end = true;
      std::printf("== 抵達 actor 迴圈結束 0x009C(step %ld;回合結束、清理前)==\n", total);
    }
    if (st.script_res == 3 && st.pc == 0x0075) reached_actor = true;
  }
  std::vector<std::uint8_t> snap_res3 = got_end ? snap_end : st.data_bytes;

  // 統計戰鬥核心子程式執行次數(證明走了 to-hit / 傷害路徑)。
  int tohit = 0, dmg_fist = 0, dmg_weap = 0, hpsub = 0;
  for (const auto& r : tr.records()) {
    if (r.pc == 0x0F73) ++tohit;     // to-hit 子程式入口
    if (r.pc == 0x0D54) ++dmg_fist;  // 徒手傷害
    if (r.pc == 0x0D68) ++dmg_weap;  // 武器傷害
    if (r.pc == 0x07C7) ++hpsub;     // 怪物 HP 扣減路徑
  }
  std::printf("== 戰鬥核心子程式執行次數:to-hit(0x0F73)=%d 徒手傷害(0x0D54)=%d "
              "武器傷害(0x0D68)=%d 怪物HP扣減(0x07C7)=%d ==\n",
              tohit, dmg_fist, dmg_weap, hpsub);

  // 列出回合內(step 4403..15050)所有經過的關鍵戰鬥 pc(去重連續),協助確認攻擊路徑。
  {
    std::printf("== 回合內關鍵 pc 命中(0x0AFA/0x0FAC/0x0F73/0x0D54/0x0D68/0x077B/0x07C7/0x07D3/0x07E6)==\n");
    int c_afa=0,c_fac=0,c_77b=0,c_7d3=0,c_7e6=0;
    for (const auto& r : tr.records()) {
      if (r.pc==0x0AFA) ++c_afa;
      if (r.pc==0x0FAC) ++c_fac;
      if (r.pc==0x077B) ++c_77b;
      if (r.pc==0x07D3) ++c_7d3;   // op_0D 0x0278(讀怪物 HP 工作值)
      if (r.pc==0x07E6) ++c_7e6;   // 0x07E5 分支:怪物未死後續
    }
    std::printf("  0x0AFA(party-attack?)=%d 0x0FAC(monster-attack?)=%d 0x077B(apply-dmg)=%d "
                "0x07D3(mon-HP-read 0x0278)=%d 0x07E6(mon-alive)=%d\n",
                c_afa, c_fac, c_77b, c_7d3, c_7e6);
  }

  std::printf("ran %ld steps; halted=%d last_unimpl=0x%02X final_pc=0x%04zx "
              "script_res=%d data_res=%d reached_actor=%d gs[0x67]=%u gs[0x1F]=%u\n",
              total, st.halted ? 1 : 0, ip.last_unimpl(), st.pc,
              st.script_res, st.data_res, reached_actor ? 1 : 0,
              st.game_state[0x67], st.game_state[0x1F]);

  // 用怪物群 count(clamp)決定要 dump 幾筆記錄。
  int nrec = monster_count > 0 && monster_count < 32 ? monster_count : 6;
  if (got_enter) dump_mon(snap_enter, nrec, "結算前 (actor-loop-enter 0x0075)");
  if (!snap_res3.empty()) dump_mon(snap_res3, nrec, got_end ? "結算後 (回合結束 0x009C)" : "結算後 (final)");

  // 逐 byte diff:列出 [0x0270, 0x03D6 + nrec*0x21) 範圍內所有變化(= HP/狀態結算證據)。
  //   0x0278 = 怪物 HP 工作陣列(res3 0x07D3 op_0D 0x0278 / op_15 0x0278 扣血處);
  //   0x03D6 = 怪物群緩衝區。
  if (got_enter && !snap_res3.empty()) {
    std::printf("== 結算前後 diff(0x0270..怪物群緩衝區)==\n");
    std::size_t lo = 0x0270, hi = kMonBase + (std::size_t)nrec * kRecLen;
    int ndiff = 0;
    for (std::size_t a = lo; a < hi && a < snap_enter.size() && a < snap_res3.size(); ++a) {
      if (snap_enter[a] != snap_res3[a]) {
        const char* reg = (a >= kMonBase) ? "群緩衝" : (a >= 0x0278 ? "HP工作陣列(0x0278)" : "");
        std::printf("  @0x%04zx [%s]: 0x%02x -> 0x%02x (Δ=%+d)\n",
                    a, reg, snap_enter[a], snap_res3[a],
                    (int)snap_res3[a] - (int)snap_enter[a]);
        ++ndiff;
      }
    }
    if (ndiff == 0) std::printf("  (無變化)\n");
  }
  return 0;
}
