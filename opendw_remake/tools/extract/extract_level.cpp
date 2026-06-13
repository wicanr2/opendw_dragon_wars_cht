// extract_level — 從原版 DATA1/DATA2 抽關卡(level)資源並解析。
//
// 依 opendw engine.c:load_level_resources —— level 資源 index = 區域編號 + 0x46。
// read_level_metadata:前 4 byte → 高(0x21)/寬(0x22)/旗標(0x23)/?(0x24)。
// 本工具先 dump 候選關卡的 header(確認哪些 index 是關卡、維度合理),
// 後續再解 tile 格 + 事件格,與攻略(38/39)比對。
//
// 用法: extract_level <data_dir> [first=70] [count=24]
#include <cstdio>
#include "../../src/resource/archive.hpp"

int main(int argc, char** argv) {
  if (argc < 2) { std::fprintf(stderr, "usage: %s <data_dir> [first] [count]\n", argv[0]); return 2; }
  auto arc = dw::res::Archive::open(argv[1]);
  if (!arc) { std::fprintf(stderr, "open archive failed: %s\n", argv[1]); return 1; }
  int first = argc > 2 ? std::atoi(argv[2]) : 0x46;   // 0x46 = 70
  int count = argc > 3 ? std::atoi(argv[3]) : 24;

  std::printf("idx(hex) | size  | h  w  f  ? | next8\n");
  std::printf("---------+-------+------------+------------------------\n");
  for (int i = first; i < first + count; ++i) {
    auto r = arc->load(i);
    if (!r) { std::printf("0x%02X     | (load failed)\n", i); continue; }
    const auto& b = *r;
    std::printf("0x%02X(%3d)| %5zu | %3d%3d%3d%3d|", i, i, b.size(),
                b.size() > 0 ? b[0] : -1, b.size() > 1 ? b[1] : -1,
                b.size() > 2 ? b[2] : -1, b.size() > 3 ? b[3] : -1);
    for (int j = 4; j < 12 && (size_t)j < b.size(); ++j) std::printf(" %02X", b[j]);
    std::printf("\n");
  }
  return 0;
}
