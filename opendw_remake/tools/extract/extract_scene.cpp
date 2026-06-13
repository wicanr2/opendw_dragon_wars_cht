// extract_scene — 把全螢幕場景圖資源(res N,解壓後 32000B)dump 成 bundle .pic 檔。
// 讓場景圖脫離 DATA1/DATA2(自包含):remake 執行期改讀 assets/bundle/scenes/<N>.pic。
//
// 用法: extract_scene <data_dir> <res_id> <out.pic>
#include <cstdio>
#include "../../src/resource/archive.hpp"

int main(int argc, char** argv) {
  if (argc < 4) { std::fprintf(stderr, "usage: %s <data_dir> <res_id> <out.pic>\n", argv[0]); return 2; }
  auto arc = dw::res::Archive::open(argv[1]);
  if (!arc) { std::fprintf(stderr, "open archive failed: %s\n", argv[1]); return 1; }
  int id = std::atoi(argv[2]);
  auto pic = arc->load(id);
  if (!pic) { std::fprintf(stderr, "load res %d failed\n", id); return 1; }
  std::FILE* f = std::fopen(argv[3], "wb");
  if (!f) { std::fprintf(stderr, "open out failed: %s\n", argv[3]); return 1; }
  std::fwrite(pic->data(), 1, pic->size(), f);
  std::fclose(f);
  std::fprintf(stderr, "res %d -> %s (%zu bytes)\n", id, argv[3], pic->size());
  return 0;
}
