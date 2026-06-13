// verify_fov — FOV 取樣 + 牆 nibble 對拍:remake sample_fov vs golden_compose。
//
// golden 由 tools_build/viewport_compose_golden/golden_compose.c 產生,
// 格式為每槽一行 + 兩行 5A56 dump。本工具只比對 5A56[0..23](= wall+ground),
// 那是 opendw refresh_viewport 取樣段的 authoritative 輸出。
//
// 用法: verify_fov <level.lvl> <golden_dir>
//   golden 檔名慣例: <basename>.f<facing>.<x>_<y>.golden
//   測試組合在 main 內列出 (含面牆 / 面通道)。
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "render/viewport_compose.hpp"
#include "resource/level.hpp"

namespace {

// 從 golden 檔抽出 "5A56[0..11] =" 與 "5A56[12..23]=" 兩行的 24 個 byte。
bool parse_golden(const std::string& path, std::uint8_t out[24]) {
  std::FILE* f = std::fopen(path.c_str(), "r");
  if (!f) return false;
  char line[512];
  int got_lo = 0, got_hi = 0;
  while (std::fgets(line, sizeof(line), f)) {
    int vals[12];
    if (std::strstr(line, "5A56[0..11]")) {
      const char* p = std::strchr(line, '=');
      if (!p) continue;
      ++p;
      int n = std::sscanf(p, " %x %x %x %x %x %x %x %x %x %x %x %x",
                          &vals[0], &vals[1], &vals[2], &vals[3], &vals[4],
                          &vals[5], &vals[6], &vals[7], &vals[8], &vals[9],
                          &vals[10], &vals[11]);
      if (n == 12) {
        for (int i = 0; i < 12; ++i) out[i] = static_cast<std::uint8_t>(vals[i]);
        got_lo = 1;
      }
    } else if (std::strstr(line, "5A56[12..23]")) {
      const char* p = std::strchr(line, '=');
      if (!p) continue;
      ++p;
      int n = std::sscanf(p, " %x %x %x %x %x %x %x %x %x %x %x %x",
                          &vals[0], &vals[1], &vals[2], &vals[3], &vals[4],
                          &vals[5], &vals[6], &vals[7], &vals[8], &vals[9],
                          &vals[10], &vals[11]);
      if (n == 12) {
        for (int i = 0; i < 12; ++i)
          out[12 + i] = static_cast<std::uint8_t>(vals[i]);
        got_hi = 1;
      }
    }
  }
  std::fclose(f);
  return got_lo && got_hi;
}

struct Case {
  int facing, x, y;
  const char* note;
};

}  // namespace

int main(int argc, char** argv) {
  if (argc < 3) {
    std::fprintf(stderr, "usage: %s <level.lvl> <golden_dir>\n", argv[0]);
    return 2;
  }
  const std::string lvl_path = argv[1];
  const std::string gdir = argv[2];

  auto lvl = dw::res::Level::load_file(lvl_path);
  if (!lvl) {
    std::fprintf(stderr, "load level failed: %s\n", lvl_path.c_str());
    return 1;
  }

  // basename without dir/ext
  std::string base = lvl_path;
  auto slash = base.find_last_of('/');
  if (slash != std::string::npos) base = base.substr(slash + 1);
  auto dot = base.find_last_of('.');
  if (dot != std::string::npos) base = base.substr(0, dot);

  const Case cases[] = {
      {0, 10, 10, "N (面通道?)"},
      {1, 10, 10, "E"},
      {2, 15, 12, "S"},
      {3, 8, 20, "W"},
  };

  int fails = 0, total = 0;
  for (const auto& c : cases) {
    ++total;
    char gpath[1024];
    std::snprintf(gpath, sizeof(gpath), "%s/%s.f%d.%d_%d.golden", gdir.c_str(),
                  base.c_str(), c.facing, c.x, c.y);
    std::uint8_t gold[24];
    if (!parse_golden(gpath, gold)) {
      std::printf("[f%d (%d,%d)] FAIL — 無法解析 golden %s\n", c.facing, c.x,
                  c.y, gpath);
      ++fails;
      continue;
    }
    auto s = dw::render::sample_fov(*lvl, c.x, c.y, c.facing);
    std::uint8_t got[24];
    for (int i = 0; i < 12; ++i) {
      got[i] = s.wall[i];
      got[12 + i] = s.ground[i];
    }
    if (std::memcmp(got, gold, 24) == 0) {
      std::printf("[f%d (%d,%d)] PASS — 5A56[0..23] byte-for-byte 一致  (%s)\n",
                  c.facing, c.x, c.y, c.note);
    } else {
      ++fails;
      std::printf("[f%d (%d,%d)] FAIL  (%s)\n", c.facing, c.x, c.y, c.note);
      std::printf("   slot:  ");
      for (int i = 0; i < 24; ++i) std::printf("%2d ", i);
      std::printf("\n   gold:  ");
      for (int i = 0; i < 24; ++i) std::printf("%02X ", gold[i]);
      std::printf("\n   got :  ");
      for (int i = 0; i < 24; ++i) std::printf("%02X ", got[i]);
      std::printf("\n   diff:  ");
      for (int i = 0; i < 24; ++i)
        std::printf("%s ", got[i] == gold[i] ? "  " : "^^");
      std::printf("\n");
    }
  }
  std::printf("\n結果: %d/%d PASS\n", total - fails, total);
  return fails == 0 ? 0 : 1;
}
