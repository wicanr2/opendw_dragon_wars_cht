// detect_skill_checks — 路徑 B(技能版):逐關逐特殊事件格(tile>1)跑 level script,
//   攔截 op_5D(get_character_data)的角色屬性讀取,偵測「讀取角色 skill 欄(0x20-0x3A)」
//   的事件格,逆出「哪些事件格 script 在檢定哪個技能」。
//
// ── 真值層級(誠實標示,見 docs/59)─────────────────────────────────────
//   • 技能檢定「觸發點」= 可識別(bytecode 真值):op_5D 讀 char_data[(sel<<8)+prop]
//     已逐指令對拍 opendw(get_character_data @engine.c:2568)。prop 在 0x20-0x3A
//     即角色 skill 欄(player.c struct skill_info)。某事件格 script 含此讀取
//     → 該格在原版會檢定該技能。座標與 prop offset 皆來自原版 .lvl bytecode,非臆造。
//   • 「檢定門檻/成功率公式」= 受阻:成功與否的 roll(op_4D PRNG)+ compare 走
//     後續 opcode + 跨資源傷害結算(同陷阱,opendw 乾淨反編無完整 settlement C 碼)。
//     本工具只給「哪格檢定哪技能」(觸發點真值)。
//
// 用法:detect_skill_checks <bundle_dir> [area] [--list]
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "../../src/resource/level.hpp"
#include "../../src/resource/provider.hpp"
#include "../../src/vm/interpreter.hpp"

using namespace dw;

// skill struct offset → 名稱(對照 opendw player.c struct skill_info,0x20-0x3A)。
static const char* skill_name(std::uint8_t off) {
  switch (off) {
    case 0x20: return "ArcaneLore";
    case 0x21: return "CaveLore";
    case 0x22: return "ForestLore";
    case 0x23: return "MountainLore";
    case 0x24: return "TownLore";
    case 0x25: return "Bandage";
    case 0x26: return "Climb";
    case 0x27: return "Fistfighting";
    case 0x28: return "Hide";
    case 0x29: return "Lockpick";
    case 0x2A: return "Pickpocket";
    case 0x2B: return "Swim";
    case 0x2C: return "Tracking";
    case 0x2D: return "Bureaucracy";
    case 0x2E: return "DruidMagic";
    case 0x2F: return "HighMagic";
    case 0x30: return "LowMagic";
    case 0x31: return "Merchant";
    case 0x32: return "SunMagic";
    case 0x33: return "Axe";
    case 0x34: return "Flail";
    case 0x35: return "Mace";
    case 0x36: return "Sword";
    case 0x37: return "TwoHandedSword";
    case 0x38: return "Bow";
    case 0x39: return "Crossbow";
    case 0x3A: return "ThrownWeapons";
    default: return nullptr;
  }
}

// 非戰鬥技能(本任務聚焦)— 用於分類顯示。
static bool is_noncombat_skill(std::uint8_t off) {
  // 0x33-0x3A 為武器類(戰鬥),排除;0x2E-0x32 為法術類(施法,非「技能檢定」語意)。
  // 非戰鬥探索技能:Lore 0x20-0x24、Bandage 0x25、Climb 0x26、Hide 0x28、
  // Lockpick 0x29、Pickpocket 0x2A、Swim 0x2B、Tracking 0x2C、Bureaucracy 0x2D、Merchant 0x31。
  return (off >= 0x20 && off <= 0x2D) || off == 0x31;
}

struct SkillHit {
  int x, y, tile;
  std::uint8_t prop;
  std::uint16_t op_pc;
};

int main(int argc, char** argv) {
  if (argc < 2) {
    std::fprintf(stderr, "usage: %s <bundle_dir> [area] [--list]\n", argv[0]);
    return 2;
  }
  std::string dir = argv[1];
  int only = -1;
  bool list = false;
  for (int i = 2; i < argc; ++i) {
    if (std::strcmp(argv[i], "--list") == 0) list = true;
    else only = std::atoi(argv[i]);
  }

  // 全局統計:skill offset → (area,tile) 命中次數。
  res::BundleProvider prov(dir);

  std::map<std::uint8_t, int> skill_total;
  int grand_total_cells = 0;
  // 診斷:所有 op_5D 讀到的 prop offset 分佈 + 跑了幾格 / 幾格 halt。
  std::map<std::uint8_t, int> all_prop;
  int dbg_cells = 0, dbg_halt = 0, dbg_op5d_fire = 0;
  std::map<std::uint8_t, int> halt_op;  // 導致 halt 的 last_unimpl opcode 分佈

  for (int a = 0; a < 40; ++a) {
    if (only >= 0 && a != only) continue;
    auto lv = res::Level::load_file(dir + "/maps/" + std::to_string(a) + ".lvl");
    if (!lv) continue;
    int level_res = a + 0x46;

    std::map<int, std::vector<std::pair<int, int>>> cells;
    for (int y = 0; y < lv->h; ++y)
      for (int x = 0; x < lv->w; ++x) {
        int t = lv->tile(x, y);
        if (t > 1) cells[t].push_back({x, y});
      }

    std::vector<SkillHit> hits;
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
      // op_58 跨資源 call 解析(技能檢定多在被呼叫的共用子 script 內)。
      st.resource_provider = [&](int tag) -> std::optional<std::vector<std::uint8_t>> {
        if (tag == level_res) return lv->data();
        if (auto b = prov.load(tag)) return b;
        return std::nullopt;
      };
      // 角色定址:game_state[6]=當前角色 0、selector=game_state[0+0x0A]=0 → record 起點 0。
      // char_data 全 0 已可走 op_5D 讀取路徑(值非關鍵,我們只觀測「讀了哪個 prop」)。
      // 為避免讀到 0 後分支提早收斂掩蓋後續讀取,給 skill 欄一個中性值,讓 script 兩種
      // 分支都可能被走過至少一次(觀測不依賴分支結果,只記錄 op_5D 觸發)。
      for (std::uint8_t p = 0x20; p <= 0x3A; ++p) st.char_data[p] = 50;

      // 互動 op_89/op_8C 在無鍵盤下會停在選單。注入一組常見選單鍵
      // (Y / 各字母 | 0x80),讓「玩家選某動作後才檢定技能」的分支也能被走到。
      // 不臆造門檻:我們只觀測 op_5D 是否被觸發(讀到哪個 skill 欄)。
      for (char c = 'A'; c <= 'Z'; ++c)
        st.headless_keys.push_back((std::uint8_t)(c | 0x80));
      st.headless_key = (std::uint8_t)('Y' | 0x80);

      // 該格觀測到的 (prop, op_pc) 集合(去重)。
      std::set<std::pair<std::uint8_t, std::uint16_t>> seen;
      dw::vm::Interpreter ip(st);
      ip.set_char_read_observer(
          [&](std::uint8_t prop, std::uint8_t /*val*/, std::uint16_t op_pc) {
            all_prop[prop]++; dbg_op5d_fire++;
            if (skill_name(prop)) seen.insert({prop, op_pc});
          });
      ip.run(200000);
      dbg_cells++;
      if (st.halted) { dbg_halt++; halt_op[ip.last_unimpl()]++; }

      for (auto& pr : seen)
        for (auto& xy : kv.second)
          hits.push_back({xy.first, xy.second, kv.first, pr.first, pr.second});
    }

    if (!hits.empty() || (only >= 0 && list)) {
      std::printf("=== area %2d \"%s\" %dx%d : %zu skill-read hit(s) ===\n",
                  a, lv->name.c_str(), lv->w, lv->h, hits.size());
      for (auto& h : hits) {
        const char* nm = skill_name(h.prop);
        std::printf("  SKILL (%2d,%2d) tile=0x%02X  %-14s (0x%02X)  op_pc=0x%04X  %s\n",
                    h.x, h.y, h.tile, nm ? nm : "?", h.prop, h.op_pc,
                    is_noncombat_skill(h.prop) ? "[非戰鬥]" : "");
        skill_total[h.prop]++;
      }
    }
    grand_total_cells += (int)hits.size();
  }

  std::printf("\n=== 技能讀取彙總(op_5D prop 0x20-0x3A)===\n");
  for (auto& kv : skill_total) {
    const char* nm = skill_name(kv.first);
    std::printf("  %-14s (0x%02X) : %d 格次  %s\n", nm ? nm : "?", kv.first,
                kv.second, is_noncombat_skill(kv.first) ? "[非戰鬥]" : "");
  }
  std::printf("\n總計技能讀取命中(格×prop):%d\n", grand_total_cells);

  std::printf("\n=== 診斷 ===\n跑過事件格:%d  其中 halt:%d  op_5D 觸發次數:%d\n",
              dbg_cells, dbg_halt, dbg_op5d_fire);
  std::printf("op_5D 讀到的所有 prop offset 分佈:\n");
  for (auto& kv : all_prop) {
    const char* nm = skill_name(kv.first);
    std::printf("  0x%02X %-14s : %d\n", kv.first, nm ? nm : "(非skill欄)", kv.second);
  }
  std::printf("導致 halt 的 last_unimpl opcode 分佈:\n");
  for (auto& kv : halt_op)
    std::printf("  op_0x%02X : %d 格\n", kv.first, kv.second);
  return 0;
}
