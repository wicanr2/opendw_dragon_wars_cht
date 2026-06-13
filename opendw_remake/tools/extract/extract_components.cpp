// extract_components — 把某關(或全部關)用到的「牆面/地面/天空元件」資源,
// 從 DATA1/DATA2 解壓後抽成自包含 bundle 資產:
//   assets/bundle/components/<tag>.bin   (tag = DATA1 section,≥ 0x6E)
//
// 元件 tag 來源 = read_level_metadata 解析出的 data_5897[],每筆
//   sec = (data_5897[i] & 0x7F) + 0x6E
// (對拍 opendw cache_resources,engine.c:5468)。這正是 viewport_compose
// step2 對拍過的選擇鏈所需的 resource tag。
//
// 解壓走已驗證的 res::Archive(內建 DATA2 修正 + Huffman 解壓,R0 對拍 byte-for-byte),
// 因此 oracle(golden_pixel 讀 components/<tag>.bin)與 remake render_first_person
// 讀到的是 byte-identical 元件資料。
//
// 用法:
//   extract_components <data_dir> <out_dir> <area>...      # 指定區
//   extract_components <data_dir> <out_dir> all            # 全 40 區聯集
//
//   data_dir 需含小寫 data1/data2(Archive::open 讀小寫)。
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <set>
#include <string>
#include <vector>

#include "render/viewport_compose.hpp"
#include "resource/archive.hpp"
#include "resource/level.hpp"

namespace fs = std::filesystem;

// 蒐集某關用到的元件 tag(= cache_resources 會 resource_load 的所有 section)。
static void collect_tags(const dw::res::Level& lvl, std::set<int>& tags) {
  dw::render::LevelComponents lc = dw::render::parse_level_components(lvl);
  // data_5897[i]:讀到 byte ≥ 0x80 為止(含 terminator,terminator 本身也 load)。
  for (int i = 0; i < 256; ++i) {
    std::uint8_t v = lc.a5897[i];
    int sec = (v & 0x7F) + 0x6E;
    tags.insert(sec);
    if (v >= 0x80) break;  // terminator:它本身 (& 0x7F) 也被 load,故先 insert 再 break。
  }
}

int main(int argc, char** argv) {
  if (argc < 4) {
    std::fprintf(stderr,
                 "usage: %s <data_dir> <out_dir> <area>... | all\n", argv[0]);
    return 2;
  }
  std::string data_dir = argv[1];
  std::string out_dir = argv[2];

  auto arc = dw::res::Archive::open(data_dir);
  if (!arc) {
    std::fprintf(stderr, "archive open failed: %s (need lowercase data1/data2)\n",
                 data_dir.c_str());
    return 1;
  }
  fs::create_directories(out_dir);

  std::vector<int> areas;
  bool all = false;
  for (int i = 3; i < argc; ++i) {
    if (std::string(argv[i]) == "all") all = true;
    else areas.push_back(std::atoi(argv[i]));
  }
  if (all) { for (int a = 0; a < 40; ++a) areas.push_back(a); }

  std::set<int> tags;
  for (int area : areas) {
    int res_id = area + 0x46;
    auto bytes = arc->load(res_id);
    if (!bytes) { std::fprintf(stderr, "  area %d (res 0x%02X) load failed\n", area, res_id); continue; }
    auto lvl = dw::res::Level::from_bytes(*bytes);
    if (!lvl) { std::fprintf(stderr, "  area %d level parse failed\n", area); continue; }
    std::set<int> at;
    collect_tags(*lvl, at);
    std::fprintf(stderr, "area %2d \"%s\": %zu component tags\n", area, lvl->name.c_str(), at.size());
    tags.insert(at.begin(), at.end());
  }

  int written = 0, skipped = 0;
  for (int tag : tags) {
    auto bytes = arc->load(tag);  // 解壓後
    if (!bytes) { std::fprintf(stderr, "  tag %d (0x%02X) load failed — skip\n", tag, tag); ++skipped; continue; }
    std::string path = out_dir + "/" + std::to_string(tag) + ".bin";
    std::FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) { std::fprintf(stderr, "  open %s failed\n", path.c_str()); ++skipped; continue; }
    std::fwrite(bytes->data(), 1, bytes->size(), f);
    std::fclose(f);
    ++written;
  }
  std::fprintf(stderr, "extract_components: wrote %d, skipped %d, into %s\n",
               written, skipped, out_dir.c_str());
  return 0;
}
