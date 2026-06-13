// verify_compose — 元件選擇 + 繪製指令序列對拍:
//   remake compose_draw_sequence vs golden_select (tools_build/viewport_compose_golden)。
//
// golden 每筆 draw 一行,格式:
//   draw <idx> <batch> c=<counter> tag=<tag> off=<off> x=<xpos> y=<ypos> b104e=0x<hh>
// 本工具逐筆比對 (batch, counter, tag, sprite_offset, xpos, ypos, byte_104E)。
//
// 用法: verify_compose <level.lvl> <golden_dir>
//   golden 檔名慣例: <basename>.f<facing>.<x>_<y>.sel.golden
//   測試組合與 verify_fov 相同 (4 組 x,y,facing)。
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "render/viewport_compose.hpp"
#include "resource/level.hpp"

namespace {

struct GoldCmd {
  int batch;  // 0 SKY 1 GND 2 OTH
  int counter, tag, off, x, y;
  int b104e;
};

int batch_of(const char* s) {
  if (std::strncmp(s, "SKY", 3) == 0) return 0;
  if (std::strncmp(s, "GND", 3) == 0) return 1;
  if (std::strncmp(s, "OTH", 3) == 0) return 2;
  return -1;
}

bool parse_golden(const std::string& path, std::vector<GoldCmd>& out) {
  std::FILE* f = std::fopen(path.c_str(), "r");
  if (!f) return false;
  char line[512];
  while (std::fgets(line, sizeof(line), f)) {
    if (std::strncmp(line, "draw ", 5) != 0) continue;
    int idx;
    char batch[8];
    GoldCmd g{};
    // draw  0 SKY c=-1 tag=111 off= 4 x=     0 y=   0 b104e=0x00
    int n = std::sscanf(line, "draw %d %7s c=%d tag=%d off=%d x=%d y=%d b104e=0x%x",
                        &idx, batch, &g.counter, &g.tag, &g.off, &g.x, &g.y, &g.b104e);
    if (n != 8) {
      std::fclose(f);
      return false;
    }
    g.batch = batch_of(batch);
    out.push_back(g);
  }
  std::fclose(f);
  return !out.empty();
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

  std::string base = lvl_path;
  auto slash = base.find_last_of('/');
  if (slash != std::string::npos) base = base.substr(slash + 1);
  auto dot = base.find_last_of('.');
  if (dot != std::string::npos) base = base.substr(0, dot);

  const Case cases[] = {
      {0, 10, 10, "N (面牆)"},
      {1, 10, 10, "E (面通道)"},
      {2, 15, 12, "S"},
      {3, 8, 20, "W"},
  };

  int fails = 0, total = 0;
  for (const auto& c : cases) {
    ++total;
    char gpath[1024];
    std::snprintf(gpath, sizeof(gpath), "%s/%s.f%d.%d_%d.sel.golden", gdir.c_str(),
                  base.c_str(), c.facing, c.x, c.y);
    std::vector<GoldCmd> gold;
    if (!parse_golden(gpath, gold)) {
      std::printf("[f%d (%d,%d)] FAIL — 無法解析 golden %s\n", c.facing, c.x, c.y,
                  gpath);
      ++fails;
      continue;
    }
    auto seq = dw::render::compose_draw_sequence(*lvl, c.x, c.y, c.facing);

    bool ok = (seq.size() == gold.size());
    if (ok) {
      for (size_t i = 0; i < seq.size(); ++i) {
        const auto& a = seq[i];
        const auto& b = gold[i];
        if (a.batch != b.batch || a.counter != b.counter || a.tag != b.tag ||
            a.sprite_offset != b.off || a.xpos != b.x || a.ypos != b.y ||
            a.byte_104E != b.b104e) {
          ok = false;
          break;
        }
      }
    }

    if (ok) {
      std::printf("[f%d (%d,%d)] PASS — %zu 筆繪製指令逐筆一致  (%s)\n", c.facing,
                  c.x, c.y, seq.size(), c.note);
    } else {
      ++fails;
      std::printf("[f%d (%d,%d)] FAIL  (%s)  gold=%zu got=%zu\n", c.facing, c.x,
                  c.y, c.note, gold.size(), seq.size());
      const char* bn[] = {"SKY", "GND", "OTH", "???"};
      size_t n = seq.size() > gold.size() ? seq.size() : gold.size();
      std::printf("   %-3s %-32s | %-32s\n", "", "GOLD", "GOT");
      for (size_t i = 0; i < n; ++i) {
        char gl[64] = "-", gt[64] = "-";
        if (i < gold.size()) {
          const auto& b = gold[i];
          std::snprintf(gl, sizeof(gl), "%s c=%d tag=%d off=%d x=%d y=%d b=%02X",
                        bn[b.batch < 0 ? 3 : b.batch], b.counter, b.tag, b.off, b.x,
                        b.y, b.b104e);
        }
        if (i < seq.size()) {
          const auto& a = seq[i];
          std::snprintf(gt, sizeof(gt), "%s c=%d tag=%d off=%d x=%d y=%d b=%02X",
                        bn[a.batch < 0 ? 3 : a.batch], a.counter, a.tag,
                        a.sprite_offset, a.xpos, a.ypos, a.byte_104E);
        }
        const char* mark = (std::strcmp(gl, gt) == 0) ? "  " : "^^";
        std::printf("   %s [%2zu] %-32s | %-32s\n", mark, i, gl, gt);
      }
    }
  }
  std::printf("\n結果: %d/%d PASS\n", total - fails, total);
  return fails == 0 ? 0 : 1;
}
