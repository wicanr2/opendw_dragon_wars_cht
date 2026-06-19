// mainline_events — 跑指定 area 清單的事件腳本(每個特殊格 tile 值),用
// BundleProvider 供 op_58 跨資源 call,攔截 emit 的英文字串並回報每格的
// halt opcode。對齊 app 的 run_event(main.cpp:507):同一條 VM 路徑,但
// 不做翻譯(輸出英文原文鍵,供 events.tsv 在地化)。
//
// 兩用:
//   (1) 抽主線事件字串(emit 的英文鍵)。
//   (2) quest gate 驗證:列出哪個 area/tile 卡在哪個未實作 opcode。
//
// 用法: mainline_events <bundle_dir> [area ...]
//   不給 area → 預設主線必經區(docs/assessment/48 §1.2 + 世界圖 0 + 結局 27)。
#include <cstdio>
#include <cstdlib>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>
#include "../../src/resource/level.hpp"
#include "../../src/resource/provider.hpp"
#include "../../src/vm/interpreter.hpp"
#include "../../src/i18n/strings.hpp"

using namespace dw;

// 載入 zh-TW 全部 tsv 表(對齊 app load_locale)。供「未在地化段落」偵測。
static i18n::Strings load_zh_tw() {
  i18n::Strings tr;
  auto m = i18n::Strings::load("assets/i18n/zh-TW/menu.tsv");
  if (m) tr = *m;
  for (const char* f : {"events", "combat", "chars", "items", "shop", "spells"})
    tr.merge(std::string("assets/i18n/zh-TW/") + f + ".tsv");
  return tr;
}
// segment 是否「未在地化的英文回退」:tr 後與原文相同(查無鍵)且含 ASCII 字母。
static bool is_untranslated_en(const i18n::Strings& tr, const std::string& s) {
  if (s.empty() || s.rfind("Read paragraph", 0) == 0) return false;
  if (tr.tr(s) != s) return false;                 // 有譯 → OK
  bool has_alpha = false;
  for (char c : s) if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')) { has_alpha = true; break; }
  return has_alpha;
}

int main(int argc, char** argv) {
  if (argc < 2) {
    std::fprintf(stderr, "usage: %s <bundle_dir> [area ...]\n", argv[0]);
    return 2;
  }
  std::string bundle = argv[1];
  std::vector<int> areas;
  if (argc >= 3) {
    for (int i = 2; i < argc; ++i) areas.push_back(std::atoi(argv[i]));
  } else {
    // 主線必經(地表)+ 世界圖(0)+ 結局(27)。
    areas = {0, 1, 2, 5, 6, 8, 17, 23, 20, 14, 25, 32, 31, 21, 30, 29, 4, 27};
  }
  res::BundleProvider prov(bundle);
  const i18n::Strings zh = load_zh_tw();             // zh-TW 在地化表(未譯偵測用)
  std::set<std::string> untranslated;                // 全域去重:未在地化英文段落

  // halt opcode 統計(全域):opcode -> 出現次數
  std::map<int, int> halt_count;
  // 卡住的 (area,tile,opcode) 清單
  struct Stall { int area, tile, op, steps; std::uint16_t pc; };
  std::vector<Stall> stalls;
  int total_emit = 0;
  std::set<std::string> uniq_strings;

  for (int area : areas) {
    auto lvl = res::Level::load_file(bundle + "/maps/" + std::to_string(area) + ".lvl");
    if (!lvl) { std::fprintf(stderr, "area %d: load failed\n", area); continue; }
    int level_res = area + 0x46;
    std::printf("######## area %d  「%s」 %dx%d  res=0x%X ########\n",
                area, lvl->name.c_str(), lvl->w, lvl->h, level_res);

    std::set<int> vals;
    for (int y = 0; y < lvl->h; ++y)
      for (int x = 0; x < lvl->w; ++x) {
        int t = lvl->tile(x, y);
        if (t > 1) vals.insert(t);
      }

    for (int v : vals) {
      std::uint16_t pc = lvl->script_pc((std::uint8_t)v);
      if (pc == 0 || pc >= lvl->data().size()) {
        std::printf("── tile 0x%02X → PC 0x%04X  (PC 無效)\n", v, pc);
        continue;
      }
      vm::VmState st;
      st.script = lvl->data();
      st.data_bytes = lvl->data();
      st.script_res = level_res;
      st.data_res = level_res;
      st.pc = pc;
      st.game_state[2] = (std::uint8_t)area;
      st.resource_provider =
          [&](int tag) -> std::optional<std::vector<std::uint8_t>> {
        if (tag == level_res) return lvl->data();
        return prov.load(tag);
      };
      vm::Interpreter ip(st);
      std::vector<std::string> emitted;
      bool read_para_pending = false;
      ip.set_message_sink([&](std::size_t off, const std::string& s) {
        if (s.empty()) return;
        if (off == vm::Interpreter::kNumberSink) {
          if (read_para_pending) {
            read_para_pending = false;
            emitted.push_back("Read paragraph " + s);
            return;
          }
          return;  // 一般數字輸出不收
        }
        if (s.rfind("Read paragraph", 0) == 0) read_para_pending = true;
        emitted.push_back(s);
      });
      int steps = ip.run(200000);
      std::uint8_t unimpl = ip.last_unimpl();
      std::printf("── tile 0x%02X → PC 0x%04X ──\n", v, pc);
      for (auto& s : emitted) {
        bool en = is_untranslated_en(zh, s);
        std::printf("  emit:%s \"%s\"\n", en ? " [EN!]" : "", s.c_str());
        if (s.rfind("Read paragraph", 0) != 0) uniq_strings.insert(s);
        if (en) untranslated.insert(s);
        ++total_emit;
      }
      if (unimpl != 0) {
        std::printf("  [HALT op 0x%02X after %d steps]\n", unimpl, steps);
        halt_count[unimpl]++;
        stalls.push_back({area, v, unimpl, steps, pc});
      } else if (emitted.empty()) {
        std::printf("  (跑 %d 步,正常結束,無 emit)\n", steps);
      }
    }
    std::printf("\n");
  }

  std::printf("======== 總結 ========\n");
  std::printf("emit 字串總數 %d;去重後唯一字串 %zu 條\n",
              total_emit, uniq_strings.size());
  std::printf("在地化:唯一 %zu 條,其中 zh-TW 未譯(英文回退)%zu 條\n",
              uniq_strings.size(), untranslated.size());
  if (!untranslated.empty()) {
    std::printf("\n-- zh-TW 未在地化英文段落(可達)--\n");
    for (auto& s : untranslated) std::printf("  [EN!] \"%s\"\n", s.c_str());
  }
  std::printf("\n-- halt opcode 分佈(未實作而卡住的格)--\n");
  for (auto& [op, c] : halt_count)
    std::printf("  op 0x%02X : %d 格\n", op, c);
  std::printf("\n-- 卡住格清單(area / tile / op / PC)--\n");
  for (auto& s : stalls)
    std::printf("  area %2d tile 0x%02X  op 0x%02X  PC 0x%04X\n",
                s.area, s.tile, s.op, s.pc);
  return 0;
}
