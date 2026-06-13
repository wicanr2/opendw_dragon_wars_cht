// verify_automap — 俯視平面地圖 (`?` automap) 像素對拍:
//   remake Minimap::render 產生的 minimap viewport_memory (36864B)
//   vs golden_minimap.c 的 .mmem (byte-for-byte)。
//
// golden 檔名慣例:<basename>.<px>_<py>.mmem (seed=all)。
//
// 用法: verify_automap <level.lvl> <golden_dir> <viewport_dir> <components_dir>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "render/minimap.hpp"
#include "render/viewport_compose.hpp"
#include "resource/level.hpp"

namespace {

struct Case { int px, py; const char* note; };

bool read_file(const std::string& path, std::vector<std::uint8_t>& out) {
  std::FILE* f = std::fopen(path.c_str(), "rb");
  if (!f) return false;
  std::fseek(f, 0, SEEK_END);
  long sz = std::ftell(f);
  std::fseek(f, 0, SEEK_SET);
  if (sz <= 0) { std::fclose(f); return false; }
  out.resize((std::size_t)sz);
  bool ok = std::fread(out.data(), 1, (std::size_t)sz, f) == (std::size_t)sz;
  std::fclose(f);
  return ok;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 5) {
    std::fprintf(stderr,
                 "usage: %s <level.lvl> <golden_dir> <viewport_dir> <components_dir>\n",
                 argv[0]);
    return 2;
  }
  const std::string lvl_path = argv[1];
  const std::string gdir = argv[2];
  const std::string vdir = argv[3];
  const std::string cdir = argv[4];

  auto lvl = dw::res::Level::load_file(lvl_path);
  if (!lvl) { std::fprintf(stderr, "load level failed: %s\n", lvl_path.c_str()); return 1; }

  std::string base = lvl_path;
  auto slash = base.find_last_of('/');
  if (slash != std::string::npos) base = base.substr(slash + 1);
  auto dot = base.find_last_of('.');
  if (dot != std::string::npos) base = base.substr(0, dot);

  dw::render::ComponentStore comps(cdir);

  dw::render::Minimap mm;
  if (!mm.load_templates(vdir + "/minimap.bin", vdir + "/data6820.bin")) {
    std::fprintf(stderr, "load minimap templates failed under %s\n", vdir.c_str());
    return 1;
  }

  const Case cases[] = {
      {10, 10, "N-ish"},
      {16, 16, "centre"},
      {8, 20, "SW"},
  };

  int fails = 0, total = 0;
  for (const auto& c : cases) {
    ++total;
    char gpath[1024];
    std::snprintf(gpath, sizeof(gpath), "%s/%s.%d_%d.mmem", gdir.c_str(),
                  base.c_str(), c.px, c.py);
    std::vector<std::uint8_t> gold;
    if (!read_file(gpath, gold)) {
      std::printf("[(%d,%d)] FAIL — 無法讀 golden %s\n", c.px, c.py, gpath);
      ++fails;
      continue;
    }

    mm.render(*lvl, c.px, c.py, comps, dw::render::Minimap::Seed::kAll);

    if (gold.size() != mm.mem.size()) {
      std::printf("[(%d,%d)] FAIL — size mismatch gold=%zu got=%zu\n",
                  c.px, c.py, gold.size(), mm.mem.size());
      ++fails;
      continue;
    }

    int first_diff = -1, ndiff = 0;
    for (std::size_t i = 0; i < gold.size(); ++i) {
      if (gold[i] != mm.mem[i]) {
        if (first_diff < 0) first_diff = (int)i;
        ++ndiff;
      }
    }

    if (ndiff == 0) {
      std::printf("[(%d,%d)] PASS — minimap viewport_memory %zuB byte-for-byte  (%s)\n",
                  c.px, c.py, gold.size(), c.note);
    } else {
      ++fails;
      std::printf("[(%d,%d)] FAIL — %d/%zu bytes differ, first@%d gold=%02X got=%02X  (%s)\n",
                  c.px, c.py, ndiff, gold.size(), first_diff,
                  gold[first_diff], mm.mem[first_diff], c.note);
      int s = first_diff > 4 ? first_diff - 4 : 0;
      std::printf("   gold:");
      for (int i = s; i < s + 12 && i < (int)gold.size(); ++i) std::printf(" %02X", gold[i]);
      std::printf("\n   got :");
      for (int i = s; i < s + 12 && i < (int)mm.mem.size(); ++i) std::printf(" %02X", mm.mem[i]);
      std::printf("\n");
    }
  }
  std::printf("\n結果: %d/%d PASS\n", total - fails, total);
  return fails == 0 ? 0 : 1;
}
