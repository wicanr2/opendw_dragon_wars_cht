// probe_monster_stats — 動態跑 res3 戰鬥 setup,dump 怪物群緩衝區 data[0x03D6]
//   與 setup driver(res3 @0x08B6)計算出的 AV/AC 表(data[0x0340]/[0x0372])。
//
// 目的:逆出「res31 21B/33B 怪物記錄 → 戰鬥屬性(AV/DV/AC/HP/傷害骰)」對映。
//   res3 setup loop(0x0500/0x053B/0x05C1)把 res31 記錄展開進群緩衝 data[0x03D6];
//   res3 setup driver(0x08B6,經 op_91 在 actor 迴圈前執行)把群緩衝的 byte[1]/byte[0x27]/
//   byte[0x28] 算成 AV/DV/AC。本工具印出兩端的實際 byte,供靜態對映佐證。
//
// 用法:probe_monster_stats <bundle_dir> [encounter_id] [max_steps]
#include "resource/provider.hpp"
#include "vm/interpreter.hpp"
#include "vm/trace.hpp"
#include "game/party.hpp"

#include <cstdio>
#include <cstdlib>

using namespace dw;

static void dumpr(const std::vector<std::uint8_t>& b, std::size_t off, std::size_t n,
                  const char* label) {
  std::printf("%-22s @0x%04zx:", label, off);
  for (std::size_t i = 0; i < n && off + i < b.size(); ++i)
    std::printf(" %02x", b[off + i]);
  std::printf("\n");
}

int main(int argc, char** argv) {
  if (argc < 2) { std::fprintf(stderr, "usage: %s <bundle> [enc_id] [max]\n", argv[0]); return 2; }
  std::string bundle = argv[1];
  int enc = argc > 2 ? (int)std::strtol(argv[2], nullptr, 0) : 0;
  long maxsteps = argc > 3 ? std::atol(argv[3]) : 60000;

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
  for (int i = 0; i < np; ++i) st.game_state[0x0A + i] = (std::uint8_t)(i * 2);
  st.r2 = (std::uint16_t)enc;        // encounter id(op_8A 用;word_3AE2)
  // 遭遇怪物資料資源 = res31(對照 engine.c:5452 gs[0x5A]=關卡怪物資源 index;
  //   monster_info 直接以 res31 為解析對象)。op_0F 用 gs[0x58+2]=gs[0x5A] 當 res index。
  st.game_state[0x5A] = 31;
  st.fake_ticks = (std::uint16_t)enc;  // 讓 op_4D RNG 走訪選不同怪(可重現)
  st.headless_encounter = true;
  st.headless_keys = {0xC6, 0xC1, 0xC1, 0xC1, 0xC1, 0xC1};
  st.headless_key = 0xC1;

  vm::Trace tr;
  vm::Interpreter ip(st, &tr);
  ip.set_message_sink([](std::size_t, const std::string&) {});
  int steps = ip.run(maxsteps);

  std::printf("=== probe_monster_stats enc=%d steps=%d halted=%d last_unimpl=0x%02X ===\n",
              enc, steps, st.halted ? 1 : 0, ip.last_unimpl());

  // 群定義指標(res3 0x04C6)= {0x03D6, 0x0412, 0x044E, 0x048A}
  const std::uint16_t grp[4] = {0x03D6, 0x0412, 0x044E, 0x048A};
  std::printf("\n-- 13-byte group template gs[0x28..0x36] --\n  ");
  for (int i = 0; i < 13; ++i) std::printf("%02x ", st.game_state[0x28 + i]);
  std::printf("\n  gs[0x27](群數)=%02x gs[0x41](群指標lo/hi)=%02x %02x\n",
              st.game_state[0x27], st.game_state[0x41], st.game_state[0x42]);

  std::printf("\n-- 群緩衝區 (data_bytes,setup loop 展開後) --\n");
  for (int g = 0; g < 4; ++g)
    dumpr(st.data_bytes, grp[g], 0x33, g == 0 ? "grp0(0x03D6)" : "grp");

  std::printf("\n-- setup driver 計算的 AV 表 data[0x0340] / 防禦表 data[0x0372] --\n");
  dumpr(st.data_bytes, 0x0340, 0x20, "AV_arr(0x0340)");
  dumpr(st.data_bytes, 0x0372, 0x20, "DEF_arr(0x0372)");
  dumpr(st.data_bytes, 0x030e, 0x16, "flag_arr(0x030e)");

  std::printf("\n-- 戰鬥工作暫存 gs --\n");
  std::printf("  AV gs[0x79]=%02x  def gs[0x7a]=%02x  gs[0x84]=%02x\n",
              st.game_state[0x79], st.game_state[0x7a], st.game_state[0x84]);
  std::printf("  dmg descriptor gs[0x7c]=%02x  gs[0x5d]=%02x  gs[0x66](weap/fist)=%02x\n",
              st.game_state[0x7c], st.game_state[0x5d], st.game_state[0x66]);
  return 0;
}
