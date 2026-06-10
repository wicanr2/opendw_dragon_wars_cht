// verify_decompress — R0 round-trip:remake 解壓某 resource,與 opendw resextract
// 產生的 golden 檔 byte-for-byte 對拍。
//
// 用法: verify_decompress <data_dir> <res_id> <golden.bin>
#include <cstdio>
#include <vector>
#include "../../src/resource/archive.hpp"

int main(int argc, char** argv) {
  if (argc < 4) {
    std::fprintf(stderr, "usage: %s <data_dir> <res_id> <golden.bin>\n", argv[0]);
    return 2;
  }
  auto arc = dw::res::Archive::open(argv[1]);
  if (!arc) { std::fprintf(stderr, "open failed\n"); return 1; }
  auto got = arc->load(std::atoi(argv[2]));
  if (!got) { std::fprintf(stderr, "load/decompress failed\n"); return 1; }

  std::FILE* f = std::fopen(argv[3], "rb");
  if (!f) { std::fprintf(stderr, "golden open failed\n"); return 1; }
  std::vector<std::uint8_t> gold;
  int c;
  while ((c = std::fgetc(f)) != EOF) gold.push_back(static_cast<std::uint8_t>(c));
  std::fclose(f);

  std::printf("remake: %zu bytes   golden: %zu bytes\n", got->size(), gold.size());
  if (*got == gold) {
    std::printf("MATCH ✓ (byte-for-byte 與 opendw 一致)\n");
    return 0;
  }
  // 找第一個分歧
  std::size_t n = got->size() < gold.size() ? got->size() : gold.size();
  for (std::size_t i = 0; i < n; ++i)
    if ((*got)[i] != gold[i]) {
      std::printf("DIFF at byte %zu: remake=0x%02X golden=0x%02X\n", i, (*got)[i], gold[i]);
      return 1;
    }
  std::printf("DIFF: 長度不同\n");
  return 1;
}
