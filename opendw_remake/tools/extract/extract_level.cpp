// extract_level — 從原版 DATA1/DATA2 抽關卡(level)資源:dump 成獨立資源 +
// 解析維度/名稱(+ 初步事件格)。依 opendw engine.c。
//
// level 資源 index = 區域編號 + 0x46(load_level_resources 5410)。
// read_level_metadata(5061):前 4 byte = 高(0x21)/寬(0x22)/旗標(0x23);
//   接著變長 section(data_5897 到 ≥0x80;data_56C6 成對到 ≥0x80;3× 元件清單
//   各到 ≥0x80);再 2 byte = word_5864(關卡名 offset);其後為 tile 格。
//   set_ui_header:關卡名 = extract_string(resource, word_5864)。
// tile 格(初步):column-major 反序,cell(x,y) @ grid+(W-1-x)*3*H+y*3,每格 3B。
//   ⚠ tile/事件位元意義尚在校正,本工具的事件輸出為初步,未必正確。
//
// 用法:
//   extract_level <data_dir> scan [first count]            掃描:維度 + 關卡名
//   extract_level <data_dir> parse <res_index>             單關完整解析
//   extract_level <data_dir> dump <out_dir> [first count]  dump 原始解壓 bytes 成 <idx>.lvl
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include "../../src/resource/archive.hpp"
#include "../../src/resource/text_codec.hpp"

struct Level { int H, W, flags; std::size_t name_off, grid; std::string name; bool ok=false; };

static std::size_t skip_until_high(const std::vector<std::uint8_t>& b, std::size_t p) {
  while (p < b.size() && b[p] < 0x80) ++p;
  return p + 1;
}

static Level parse(const std::vector<std::uint8_t>& b) {
  Level L;
  if (b.size() < 8) return L;
  L.H = b[0]; L.W = b[1]; L.flags = b[2];
  std::size_t p = 4;
  p = skip_until_high(b, p);                                  // data_5897
  while (p + 1 < b.size()) { std::uint8_t f = b[p]; p += 2; if (f >= 0x80) break; }  // data_56C6 pairs
  for (int k = 0; k < 3; ++k) p = skip_until_high(b, p);      // 3× 元件清單
  if (p + 1 >= b.size()) return L;
  L.name_off = b[p] | (b[p + 1] << 8); p += 2;
  L.grid = p;
  if (L.name_off < b.size()) L.name = dw::text::decode(b, L.name_off).first;
  L.ok = true;
  return L;
}

int main(int argc, char** argv) {
  if (argc < 3) { std::fprintf(stderr, "usage: %s <data_dir> scan|parse|dump ...\n", argv[0]); return 2; }
  auto arc = dw::res::Archive::open(argv[1]);
  if (!arc) { std::fprintf(stderr, "open archive failed: %s\n", argv[1]); return 1; }
  std::string cmd = argv[2];

  if (cmd == "scan") {
    int first = argc > 3 ? std::atoi(argv[3]) : 0x46, count = argc > 4 ? std::atoi(argv[4]) : 30;
    std::printf("idx(hex) area | size  | H  W  flags | name\n");
    for (int i = first; i < first + count; ++i) {
      auto r = arc->load(i);
      if (!r) { std::printf("0x%02X (load failed)\n", i); continue; }
      Level L = parse(*r);
      std::printf("0x%02X  a%-3d | %5zu | %2d %2d 0x%02X | %s\n", i, i - 0x46, r->size(),
                  L.H, L.W, L.flags, L.name.c_str());
    }
    return 0;
  }

  if (cmd == "dump") {
    std::string outdir = argc > 3 ? argv[3] : ".";
    int first = argc > 4 ? std::atoi(argv[4]) : 0x46, count = argc > 5 ? std::atoi(argv[5]) : 30;
    int ok = 0;
    for (int i = first; i < first + count; ++i) {
      auto r = arc->load(i);
      if (!r) continue;
      std::string path = outdir + "/" + std::to_string(i - 0x46) + ".lvl";  // 檔名 = 區域編號
      std::FILE* f = std::fopen(path.c_str(), "wb");
      if (!f) { std::fprintf(stderr, "open %s failed\n", path.c_str()); continue; }
      std::fwrite(r->data(), 1, r->size(), f); std::fclose(f);
      ++ok;
    }
    std::fprintf(stderr, "dumped %d levels to %s (檔名=區域編號.lvl)\n", ok, outdir.c_str());
    return 0;
  }

  if (cmd == "parse") {
    if (argc < 4) { std::fprintf(stderr, "parse needs <res_index>\n"); return 2; }
    int idx = std::atoi(argv[3]);
    auto r = arc->load(idx);
    if (!r) { std::fprintf(stderr, "load %d failed\n", idx); return 1; }
    Level L = parse(*r);
    const auto& b = *r;
    std::printf("res 0x%02X(%d) area %d: size=%zu  H=%d W=%d flags=0x%02X\n",
                idx, idx, idx - 0x46, b.size(), L.H, L.W, L.flags);
    std::printf("name_off=0x%04zX name=\"%s\"  grid@0x%04zX (need %d, have %zu)\n",
                L.name_off, L.name.c_str(), L.grid, L.W * L.H * 3, b.size() - L.grid);
    int nev = 0;
    for (int y = 0; y < L.H; ++y) for (int x = 0; x < L.W; ++x) {
      std::size_t off = L.grid + (std::size_t)(L.W - 1 - x) * 3 * L.H + (std::size_t)y * 3;
      if (off + 2 >= b.size()) continue;
      if (b[off + 2] != 0) { if (nev < 50) std::printf("  (%2d,%2d) ev=0x%02X attr=0x%04X\n",
                              x, y, b[off + 2], (std::uint16_t)(b[off] | b[off + 1] << 8)); ++nev; }
    }
    std::printf("事件格總數(初步,待校):%d\n", nev);
    return 0;
  }
  std::fprintf(stderr, "unknown cmd: %s\n", cmd.c_str());
  return 2;
}
