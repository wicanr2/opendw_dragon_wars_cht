// real_terrain — 實作。見 real_terrain.hpp 檔頭(真值層級)。
#include "game/real_terrain.hpp"

#include <cctype>
#include <map>
#include <string>
#include <vector>

#include "vm/interpreter.hpp"

namespace dw::game {

// 陷阱訊息語意類(小寫子字串;任一命中即判該事件格 script 為傷害/敵意機關)。
//   來源:攻略 docs/walkthrough/38 描述的陷阱 + 全 40 關 emit 掃描人工 curate(保守取「即時敵意
//   環境 / 機關」句,排除景物 / 劇情描述以避免誤報)。與 tools/verify/detect_traps.cpp
//   同一份判定集(此處為單一真理來源;工具可獨立維護自身副本供逐關 dump)。
static const char* kTrapPhrases[] = {
    "tear at your soul",               // 尼塞山腹 icy winds of despair
    "the floor moved",                 // 尼塞山腹「floor is moving」陷阱
    "fury of the sun seems to burn",   // 尼塞山腹 solarium 灼燒走廊
    "saps your very life essence",     // 矮人鑄爐(area16)耗命
    "smashs the party into",           // 魔法學院 tripwire 巨石(area31)
    "icy winds of despair",            // 同 soul(備援)
};

static bool is_trap_message(const std::string& s) {
  std::string ll = s;
  for (auto& c : ll) c = (char)std::tolower((unsigned char)c);
  for (const char* p : kTrapPhrases)
    if (ll.find(p) != std::string::npos) return true;
  return false;
}

RealTraps RealTraps::identify(const res::Level& level, int area) {
  RealTraps out;
  const int level_res = area + 0x46;

  // tile 值 → 出現座標。
  std::map<int, std::vector<std::pair<int, int>>> cells;
  for (int y = 0; y < level.h; ++y)
    for (int x = 0; x < level.w; ++x) {
      int t = level.tile(x, y);
      if (t > 1) cells[t].push_back({x, y});
    }

  for (auto& kv : cells) {
    std::uint16_t pc = level.script_pc((std::uint8_t)kv.first);
    if (pc == 0 || pc >= level.data().size()) continue;

    dw::vm::VmState st;
    st.script = level.data();
    st.data_bytes = level.data();
    st.script_res = level_res;
    st.data_res = level_res;
    st.pc = pc;
    st.headless_encounter = true;  // 隨機遭遇不 halt,讓 trap script 跑到 emit

    bool trap = false;
    dw::vm::Interpreter ip(st);
    ip.set_message_sink([&](std::size_t off, const std::string& s) {
      if (off == dw::vm::Interpreter::kNumberSink) return;
      if (!trap && is_trap_message(s)) trap = true;
    });
    ip.run(200000);  // 步數上限,避免壞跳轉無限執行

    if (trap)
      for (auto& xy : kv.second) out.cells_.insert(xy);
  }
  return out;
}

}  // namespace dw::game
