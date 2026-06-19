// detect_traps — 路徑 B:逐關逐特殊事件格(word_11C8≥2)跑 level script,以 VM 攔截
//   emit 的事件訊息,判定該格 script 是否屬「對隊伍造成傷害/敵意環境」語意類(陷阱)。
//
// ── 真值層級(誠實標示)─────────────────────────────────────────────────
//   • 陷阱「位置」: 可識別(bytecode 真值)—— tile → script_pc → VM 跑出的 emit 字串
//     已逐指令對拍 opendw(op_71/run_level_script);字串本身是原版資產解碼結果。
//   • 陷阱「傷害結算」: 受阻 —— 傷害的 HP/Stun 實際扣減走 op_58 跨資源呼叫 +
//     未反編的戰鬥/傷害 settlement(opendw 乾淨反編無此 C 碼,docs/gameplay/57_DOORS_TRAPS_TERRAIN.md §1)。VM 觀察到
//     trap script「emit 敵意訊息後 dispatch 到傷害 handler」,但不寫 char_data HP
//     (已實測:area27 trap tile 走 op_52 跳轉/op_58,非 op_5E 寫 Health)。
//   故本工具以「emit 訊息語意類」識別陷阱格(位置真值),不宣稱逆出傷害數值。
//
//   訊息判定集 = 由攻略(docs/walkthrough/38)描述的陷阱 + 全 40 關 emit 掃描人工 curate;
//   保守取「瞬時敵意環境/機關」語句,排除單純景物描述(避免誤報)。
//
// 用法:detect_traps <bundle_dir> [area] [--list]
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "../../src/resource/level.hpp"
#include "../../src/resource/provider.hpp"
#include "../../src/vm/interpreter.hpp"

using namespace dw;

// 陷阱訊息語意類:小寫子字串,任一命中即判該 script 為傷害/敵意機關。
//   來源:攻略 docs/walkthrough/38(尼塞山腹陷阱「the floor is moving」、放逐橋單向門、
//   矮人鑄爐耗命、魔法學院 tripwire 巨石、Mystic Wood 陷阱)+ 全關 emit 掃描。
//   保守:只取「對玩家施加即時傷害/環境敵意」的句子,景物/劇情描述不收。
static const char* kTrapPhrases[] = {
    "tear at your soul",          // 尼塞山腹 icy winds of despair
    "the floor moved",            // 尼塞山腹「floor is moving」陷阱
    "fury of the sun seems to burn",  // 尼塞山腹 solarium 灼燒走廊
    "saps your very life essence", // 矮人鑄爐(area16)耗命
    "smashs the party into",      // 魔法學院 tripwire 巨石(area31)
    "tripwire",                   // 機關線(壓觸發)
    "icy winds of despair",       // 同 soul(備援)
    "winds of despair tear",      // 同上
};

static bool is_trap_message(const std::string& s) {
  std::string ll = s;
  for (auto& c : ll) c = (char)std::tolower((unsigned char)c);
  for (const char* p : kTrapPhrases)
    if (ll.find(p) != std::string::npos) return true;
  return false;
}

struct TrapHit { int x, y, tile; std::string msg; };

int main(int argc, char** argv) {
  if (argc < 2) { std::fprintf(stderr, "usage: %s <bundle_dir> [area] [--list]\n", argv[0]); return 2; }
  std::string dir = argv[1];
  int only = -1;
  bool list = false;
  for (int i = 2; i < argc; ++i) {
    if (std::strcmp(argv[i], "--list") == 0) list = true;
    else only = std::atoi(argv[i]);
  }

  int grand_total = 0;
  for (int a = 0; a < 40; ++a) {
    if (only >= 0 && a != only) continue;
    auto lv = res::Level::load_file(dir + "/maps/" + std::to_string(a) + ".lvl");
    if (!lv) continue;
    int level_res = a + 0x46;

    // tile 值 → 該值出現的座標。
    std::map<int, std::vector<std::pair<int, int>>> cells;
    for (int y = 0; y < lv->h; ++y)
      for (int x = 0; x < lv->w; ++x) {
        int t = lv->tile(x, y);
        if (t > 1) cells[t].push_back({x, y});
      }

    std::vector<TrapHit> hits;
    for (auto& kv : cells) {
      std::uint16_t pc = lv->script_pc((std::uint8_t)kv.first);
      if (pc == 0 || pc >= lv->data().size()) continue;

      dw::vm::VmState st;
      st.script = lv->data();
      st.data_bytes = lv->data();
      st.script_res = level_res;
      st.data_res = level_res;
      st.pc = pc;
      st.headless_encounter = true;

      std::string trap_msg;
      dw::vm::Interpreter ip(st);
      ip.set_message_sink([&](std::size_t off, const std::string& s) {
        if (off == dw::vm::Interpreter::kNumberSink) return;
        if (trap_msg.empty() && is_trap_message(s)) trap_msg = s;
      });
      ip.run(200000);

      if (!trap_msg.empty())
        for (auto& xy : kv.second)
          hits.push_back({xy.first, xy.second, kv.first, trap_msg});
    }

    if (!hits.empty() || (only >= 0 && list)) {
      std::printf("=== area %2d \"%s\" %dx%d : %zu trap cell(s) ===\n",
                  a, lv->name.c_str(), lv->w, lv->h, hits.size());
      for (auto& h : hits)
        std::printf("  TRAP (%2d,%2d) tile=0x%02X  \"%.60s\"\n", h.x, h.y, h.tile, h.msg.c_str());
    }
    grand_total += (int)hits.size();
  }
  std::printf("\n總計識別陷阱格:%d\n", grand_total);
  return 0;
}
