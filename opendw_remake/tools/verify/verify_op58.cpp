// verify_op58 — 跨資源事件 op_58 halt 統計 / 確定性 PASS/FAIL。
//
// 動機:VM op_58 = 跨資源 script call。事件腳本(40 關每個特殊格)常 op_58 進
// 別份資源(script section 1/3/5/8/9/10/11/17/19 與「關卡自身」資源 area+0x46)。
// 若 BundleProvider 取不到目標資源 → op58_xcall 標記 last_unimpl=0x58 並 halt,
// 換場/事件無法完成。
//
// 本工具用「同一份 maps + 兩種 provider」重放全 40 關所有事件 tile,統計 op_58
// halt 次數,印「halt 前 X 次 → 後 Y 次」並分類殘餘原因:
//   - BEFORE:模擬補齊前的 BundleProvider —— 只認原本 11 個 script tag,且
//             不提供「關卡自身」資源(level-self),完全比照舊 bundle 行為。
//   - AFTER :現行 BundleProvider(assets/bundle/scripts/*.bin)+ level-self fallback
//             (= main.cpp 的 `tag == level_res → level->data()`)。
//
// op_58 目標分類(AFTER 殘餘):data-not-script / wrap-world / 真未實作 opcode / 高位雜訊。
//
// 用法: verify_op58 <bundle_dir> [max_area]
#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>
#include "../../src/resource/level.hpp"
#include "../../src/resource/provider.hpp"
#include "../../src/vm/interpreter.hpp"

using namespace dw;

namespace {

// 補齊前(BEFORE)bundle 只含這些 script tag(見 build_bundle.sh 的 SECTIONS 舊集合)。
const std::set<int> kOldTags = {0, 1, 3, 5, 6, 8, 9, 10, 11, 17, 19};

struct Stats {
  long events = 0;       // 跑過的事件 tile 數
  long op58_halt = 0;    // op_58 取不到資源而 halt 的次數
  long other_halt = 0;   // 其它未實作 opcode halt
  std::set<int> missing_tags;          // op_58 取不到的目標 tag
  std::map<int, long> missing_tag_cnt; // 各 tag 的 halt 次數
  std::map<int, long> unimpl_hist;     // 未實作 opcode 直方圖(last_unimpl→halt 次數)
};

// 跑一關所有事件 tile;mode=0 → BEFORE(舊 11 tag、無 level-self),mode=1 → AFTER。
void run_area(const std::string& bundle, int area, res::BundleProvider& prov,
              bool after, Stats& st) {
  auto lvl = res::Level::load_file(bundle + "/maps/" + std::to_string(area) + ".lvl");
  if (!lvl) return;
  int level_res = area + 0x46;

  std::set<int> vals;
  for (int y = 0; y < lvl->h; ++y)
    for (int x = 0; x < lvl->w; ++x) {
      int t = lvl->tile(x, y);
      if (t > 1) vals.insert(t);
    }

  for (int v : vals) {
    std::uint16_t pc = lvl->script_pc((std::uint8_t)v);
    if (pc == 0 || pc >= lvl->data().size()) continue;
    ++st.events;

    vm::VmState s;
    s.script = lvl->data();
    s.data_bytes = lvl->data();
    s.script_res = level_res;
    s.data_res = level_res;
    s.pc = pc;
    // 起步狀態:對拍 main.cpp run_event 的 seed(入口座標 + area + 朝向)。
    int px = 0, py = 0;
    for (int y = 0; y < lvl->h && px == 0 && py == 0; ++y)
      for (int x = 0; x < lvl->w; ++x)
        if (lvl->tile(x, y) == 1) { px = x; py = y; y = lvl->h; break; }
    s.game_state[0] = (std::uint8_t)px;
    s.game_state[1] = (std::uint8_t)py;
    s.game_state[2] = (std::uint8_t)area;
    s.game_state[3] = 1;
    s.game_state[0x1F] = 1;  // 隊伍至少 1 人,讓 op_5C 子迴圈會跑

    s.resource_provider = [&](int tag) -> std::optional<std::vector<std::uint8_t>> {
      if (after) {
        // AFTER:純 BundleProvider(已自包含 level-self via maps/*.lvl),
        // 不需呼叫端的 `tag == level_res` 特例。
        return prov.load(tag);
      }
      // BEFORE:只認舊 11 個 script tag,且不提供 level-self(模擬補齊前)。
      if (kOldTags.count(tag) && tag < 0x46) return prov.load(tag);
      return std::nullopt;
    };

    vm::Interpreter ip(s);
    ip.run(200000);  // 上限保護
    if (s.halted && ip.last_unimpl() == 0x58) {
      ++st.op58_halt;
      // 重跑記錄被擋的 tag(provider 回 nullopt 的那個)。
      // op58_xcall 已 fetch 了 tag;這裡不易直接取回,改在 provider 端記錄。
    } else if (s.halted && ip.last_unimpl() != 0 && ip.last_unimpl() != 0x58) {
      ++st.other_halt;
    }
    // 未實作 opcode 直方圖(含 0x58;0=非未實作所致的 halt)。
    if (s.halted && ip.last_unimpl() != 0)
      st.unimpl_hist[ip.last_unimpl()]++;
  }
}

// AFTER 的失敗 tag 追蹤:用一個會記錄「最後一個 nullopt tag」的 provider 包裝。
void run_area_track(const std::string& bundle, int area, res::BundleProvider& prov,
                    Stats& st) {
  auto lvl = res::Level::load_file(bundle + "/maps/" + std::to_string(area) + ".lvl");
  if (!lvl) return;
  int level_res = area + 0x46;
  std::set<int> vals;
  for (int y = 0; y < lvl->h; ++y)
    for (int x = 0; x < lvl->w; ++x) {
      int t = lvl->tile(x, y);
      if (t > 1) vals.insert(t);
    }
  for (int v : vals) {
    std::uint16_t pc = lvl->script_pc((std::uint8_t)v);
    if (pc == 0 || pc >= lvl->data().size()) continue;
    vm::VmState s;
    s.script = lvl->data(); s.data_bytes = lvl->data();
    s.script_res = level_res; s.data_res = level_res; s.pc = pc;
    int px = 0, py = 0;
    for (int y = 0; y < lvl->h && px == 0 && py == 0; ++y)
      for (int x = 0; x < lvl->w; ++x)
        if (lvl->tile(x, y) == 1) { px = x; py = y; y = lvl->h; break; }
    s.game_state[0]=(std::uint8_t)px; s.game_state[1]=(std::uint8_t)py;
    s.game_state[2]=(std::uint8_t)area; s.game_state[3]=1; s.game_state[0x1F]=1;
    int last_fail = -1;
    s.resource_provider = [&](int tag) -> std::optional<std::vector<std::uint8_t>> {
      if (tag == level_res) return lvl->data();
      auto r = prov.load(tag);
      if (!r) last_fail = tag;
      return r;
    };
    vm::Interpreter ip(s);
    ip.run(200000);
    if (s.halted && ip.last_unimpl() == 0x58 && last_fail >= 0) {
      st.missing_tags.insert(last_fail);
      st.missing_tag_cnt[last_fail]++;
    }
  }
}

const char* classify(int tag) {
  if (tag >= 0x107) return "高位雜訊/非法 section(>=RESOURCE_MAX,空 game_state 衍生)";
  if (tag >= 0x46 && tag <= 0x6D) return "關卡自身資源(level-self,應由 maps/*.lvl 提供)";
  return "script section(應在 bundle/scripts)";
}

}  // namespace

int main(int argc, char** argv) {
  std::string bundle = (argc >= 2) ? argv[1] : "assets/bundle";
  int max_area = (argc >= 3) ? std::atoi(argv[2]) : 39;

  res::BundleProvider prov(bundle);

  Stats before, after;
  for (int a = 0; a <= max_area; ++a) {
    run_area(bundle, a, prov, /*after=*/false, before);
    run_area(bundle, a, prov, /*after=*/true, after);
  }
  // AFTER 殘餘失敗 tag 追蹤(分開跑,避免污染計數)。
  Stats track;
  for (int a = 0; a <= max_area; ++a) run_area_track(bundle, a, prov, track);

  std::printf("== op_58 跨資源 halt 統計(bundle=%s, area 0..%d)==\n\n",
              bundle.c_str(), max_area);
  std::printf("事件 tile 數(每關特殊格 script): %ld\n", before.events);
  std::printf("op_58 halt:  前 %ld 次  →  後 %ld 次\n", before.op58_halt, after.op58_halt);
  std::printf("其它未實作 opcode halt:  前 %ld  →  後 %ld\n\n",
              before.other_halt, after.other_halt);

  // 未實作 opcode 直方圖(AFTER):依 halt 次數排序輸出,供補齊優先排序。
  std::printf("-- AFTER 未實作 opcode 直方圖(halt 次數,含 0x58)--\n");
  {
    std::vector<std::pair<int, long>> v(after.unimpl_hist.begin(),
                                        after.unimpl_hist.end());
    std::sort(v.begin(), v.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });
    if (v.empty()) std::printf("  (無)\n");
    for (const auto& [op, n] : v)
      std::printf("  op_%02X x%ld\n", op, n);
    std::printf("\n");
  }

  if (!track.missing_tags.empty()) {
    std::printf("-- AFTER 殘餘 op_58 失敗目標 tag 分類 --\n");
    for (int t : track.missing_tags)
      std::printf("  tag 0x%X(%d) x%ld : %s\n", t, t, track.missing_tag_cnt[t], classify(t));
    std::printf("\n");
  } else {
    std::printf("-- AFTER 無殘餘 op_58 失敗 --\n\n");
  }

  // PASS 判準:AFTER 的 op_58 halt 必須 <= BEFORE,且殘餘(若有)只能是
  // 「高位雜訊/非法 section」這類非真實資源(空 game_state 暴力重放的產物)。
  bool residual_ok = true;
  for (int t : track.missing_tags)
    if (t < 0x107) residual_ok = false;  // 真實 section/level-self 不應殘留
  bool pass = (after.op58_halt <= before.op58_halt) && residual_ok;
  std::printf("%s: op_58 halt 補齊(%ld→%ld;殘餘僅非法 section=%s)\n",
              pass ? "PASS" : "FAIL", before.op58_halt, after.op58_halt,
              residual_ok ? "yes" : "no");
  return pass ? 0 : 1;
}
