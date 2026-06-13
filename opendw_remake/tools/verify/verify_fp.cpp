// verify_fp — 第一人稱 viewport 像素對拍:
//   remake render_first_person 產生的 viewport_memory (10880B)
//   vs golden_pixel.c 的 .vpmem (byte-for-byte)。
//
// golden 檔名慣例:<basename>.f<facing>.<x>_<y>.vpmem
// 測試組合與 verify_fov / verify_compose 相同 (Purgatory 4 朝向)。
//
// 用法: verify_fp <level.lvl> <golden_dir> <components_dir>
#include <cstdint>
#include <cstdio>
#include <vector>

#include "render/viewport.hpp"
#include "render/viewport_compose.hpp"
#include "resource/level.hpp"

namespace {

struct Case { int facing, x, y; const char* note; };

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
  if (argc < 4) {
    std::fprintf(stderr, "usage: %s <level.lvl> <golden_dir> <components_dir>\n", argv[0]);
    return 2;
  }
  const std::string lvl_path = argv[1];
  const std::string gdir = argv[2];
  const std::string cdir = argv[3];

  auto lvl = dw::res::Level::load_file(lvl_path);
  if (!lvl) { std::fprintf(stderr, "load level failed: %s\n", lvl_path.c_str()); return 1; }

  std::string base = lvl_path;
  auto slash = base.find_last_of('/');
  if (slash != std::string::npos) base = base.substr(slash + 1);
  auto dot = base.find_last_of('.');
  if (dot != std::string::npos) base = base.substr(0, dot);

  dw::render::ComponentStore comps(cdir);

  const Case cases[] = {
      {0, 10, 10, "N"},
      {1, 10, 10, "E"},
      {2, 15, 12, "S"},
      {3, 8, 20, "W"},
  };

  int fails = 0, total = 0;
  for (const auto& c : cases) {
    ++total;
    char gpath[1024];
    std::snprintf(gpath, sizeof(gpath), "%s/%s.f%d.%d_%d.vpmem", gdir.c_str(),
                  base.c_str(), c.facing, c.x, c.y);
    std::vector<std::uint8_t> gold;
    if (!read_file(gpath, gold)) {
      std::printf("[f%d (%d,%d)] FAIL — 無法讀 golden %s\n", c.facing, c.x, c.y, gpath);
      ++fails;
      continue;
    }

    dw::render::ViewportDecoder dec;
    dw::render::render_first_person(*lvl, c.x, c.y, c.facing, dec, comps);

    if (gold.size() != dec.mem.size()) {
      std::printf("[f%d (%d,%d)] FAIL — size mismatch gold=%zu got=%zu\n",
                  c.facing, c.x, c.y, gold.size(), dec.mem.size());
      ++fails;
      continue;
    }

    int first_diff = -1, ndiff = 0;
    for (std::size_t i = 0; i < gold.size(); ++i) {
      if (gold[i] != dec.mem[i]) {
        if (first_diff < 0) first_diff = (int)i;
        ++ndiff;
      }
    }

    if (ndiff == 0) {
      std::printf("[f%d (%d,%d)] PASS — viewport_memory 10880B byte-for-byte  (%s)\n",
                  c.facing, c.x, c.y, c.note);
    } else {
      ++fails;
      std::printf("[f%d (%d,%d)] FAIL — %d/%zu bytes differ, first@%d gold=%02X got=%02X  (%s)\n",
                  c.facing, c.x, c.y, ndiff, gold.size(), first_diff,
                  gold[first_diff], dec.mem[first_diff], c.note);
      // dump 12 bytes around first diff
      int s = first_diff > 4 ? first_diff - 4 : 0;
      std::printf("   gold:");
      for (int i = s; i < s + 12 && i < (int)gold.size(); ++i) std::printf(" %02X", gold[i]);
      std::printf("\n   got :");
      for (int i = s; i < s + 12 && i < (int)dec.mem.size(); ++i) std::printf(" %02X", dec.mem[i]);
      std::printf("\n");
    }
  }
  std::printf("\n結果: %d/%d PASS\n", total - fails, total);
  return fails == 0 ? 0 : 1;
}
