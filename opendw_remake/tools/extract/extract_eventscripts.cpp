// extract_eventscripts — 掃描全 40 關事件腳本,找出 op_58 跨資源 call 載入的
// 資源 tag 聯集,並把這些 tag 的(解壓後)bytes 抽成 bundle 資產
// (assets/bundle/scripts/<tag>.bin),讓 app 端的 BundleProvider 不依賴 DATA1
// 也能跑 op_58。
//
// 原理:對每關 (area 0..N) 的每個特殊格 tile 值,從 script_pc 起跑事件腳本;
// 把 resource_provider 包一層 logger,記錄 VM 實際請求過的每個 tag。跑完所有關後,
// 取聯集,逐 tag 用 Data1Provider 載入(解壓後 bytes)寫成 scripts/<tag>.bin。
//
// 用法: extract_eventscripts <maps_dir> <data_dir> <bundle_scripts_out_dir> [max_area]
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <map>
#include <optional>
#include <set>
#include <string>
#include "../../src/resource/level.hpp"
#include "../../src/resource/provider.hpp"
#include "../../src/vm/interpreter.hpp"

int main(int argc, char** argv) {
  if (argc < 4) {
    std::fprintf(stderr,
                 "usage: %s <maps_dir> <data_dir> <out_scripts_dir> [max_area]\n",
                 argv[0]);
    return 2;
  }
  std::filesystem::path maps_dir = argv[1];
  std::filesystem::path data_dir = argv[2];
  std::filesystem::path out_dir = argv[3];
  int max_area = (argc >= 5) ? std::atoi(argv[4]) : 39;

  auto prov = dw::res::Data1Provider::open(data_dir);
  if (!prov) {
    std::fprintf(stderr, "open Data1Provider failed: %s\n", data_dir.string().c_str());
    return 1;
  }

  std::set<int> requested_tags;        // 全關聯集
  std::map<int, std::set<int>> per_area;  // 每關用到哪些 tag(診斷)

  for (int area = 0; area <= max_area; ++area) {
    std::filesystem::path lvl_path = maps_dir / (std::to_string(area) + ".lvl");
    if (!std::filesystem::exists(lvl_path)) continue;
    auto lvl = dw::res::Level::load_file(lvl_path);
    if (!lvl) { std::fprintf(stderr, "  area %d: load failed\n", area); continue; }
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
      dw::vm::VmState st;
      st.script = lvl->data();
      st.data_bytes = lvl->data();
      st.script_res = level_res;
      st.data_res = level_res;
      st.pc = pc;
      // 包一層 logger:記錄每個被請求的 tag(含 level 自身與外部資源)。
      st.resource_provider = [&](int tag) -> std::optional<std::vector<std::uint8_t>> {
        requested_tags.insert(tag);
        per_area[area].insert(tag);
        return prov->load(tag);
      };
      dw::vm::Interpreter ip(st);
      ip.run();  // 不需 message sink,只關心 provider 被請求的 tag
    }
  }

  std::printf("== op_58 / 跨資源 call 請求的 tag 聯集 (%zu 個) ==\n",
              requested_tags.size());
  for (int t : requested_tags) std::printf(" 0x%02X(%d)", t, t);
  std::printf("\n\n");

  // 逐 tag 抽成 bundle scripts/<tag>.bin(解壓後 bytes)。
  std::filesystem::create_directories(out_dir);
  int written = 0, missing = 0;
  for (int t : requested_tags) {
    auto bytes = prov->load(t);
    if (!bytes) {
      std::fprintf(stderr, "  tag 0x%02X(%d): load 失敗(略過)\n", t, t);
      ++missing;
      continue;
    }
    std::filesystem::path out = out_dir / (std::to_string(t) + ".bin");
    std::FILE* f = std::fopen(out.string().c_str(), "wb");
    if (!f) { std::fprintf(stderr, "  寫入失敗:%s\n", out.string().c_str()); continue; }
    std::fwrite(bytes->data(), 1, bytes->size(), f);
    std::fclose(f);
    std::printf("  寫 scripts/%d.bin  (%zu B)\n", t, bytes->size());
    ++written;
  }
  std::printf("\n== 完成:寫 %d 個 tag,缺 %d 個 ==\n", written, missing);

  // 每關診斷
  std::printf("\n-- 每關用到的 tag --\n");
  for (auto& [area, tags] : per_area) {
    std::printf("area %d:", area);
    for (int t : tags) std::printf(" %d", t);
    std::printf("\n");
  }
  return 0;
}
