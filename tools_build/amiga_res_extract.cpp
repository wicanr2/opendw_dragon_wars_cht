// amiga_res_extract — 從 Amiga Dragon Wars data1–6 抽 + 解壓單一資源 → raw bin。
//
// Amiga data1–6 與 DOS opendw archive 同格式:每檔 768B header(384 個 LE16 size,
//   ≥0xFF00 = 資源不在此檔)+ 串接 Huffman 壓縮資源(codec 同 remake decompress.cpp:
//   [size_LE16][tree][bitstream],big-endian bit reader)。
//
// 與 remake archive.cpp 不同點:Amiga 把資源散到 6 個檔,每檔自帶 header。本工具直接吃
//   「指定資料檔 + 資源編號」,在該檔內定位 offset(768 + 該檔內前序 present 資源 size 總和)
//   並解壓。沿用 src/resource/decompress.cpp 同一份 codec(連結進來,不重寫 Huffman)。
//
// 用法: amiga_res_extract <datafile> <res_id> <out.bin>
//   e.g. amiga_res_extract /tmp/amiga_dw/data4 196 /tmp/res196.bin
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <span>
#include <vector>

#include "decompress.hpp"

static std::vector<std::uint8_t> read_file(const char* p) {
  std::FILE* f = std::fopen(p, "rb");
  if (!f) { std::fprintf(stderr, "open failed: %s\n", p); std::exit(1); }
  std::fseek(f, 0, SEEK_END);
  long n = std::ftell(f);
  std::fseek(f, 0, SEEK_SET);
  std::vector<std::uint8_t> buf(n > 0 ? (std::size_t)n : 0);
  if (!buf.empty() && std::fread(buf.data(), 1, buf.size(), f) != buf.size()) {
    std::fprintf(stderr, "read failed: %s\n", p); std::exit(1);
  }
  std::fclose(f);
  return buf;
}

int main(int argc, char** argv) {
  if (argc < 4) {
    std::fprintf(stderr, "usage: %s <datafile> <res_id> <out.bin>\n", argv[0]);
    return 2;
  }
  auto file = read_file(argv[1]);
  int id = std::atoi(argv[2]);
  if (file.size() < 768 || id < 0 || id >= 384) {
    std::fprintf(stderr, "bad file/id\n"); return 1;
  }
  std::uint16_t sizes[384];
  for (int i = 0; i < 384; ++i)
    sizes[i] = (std::uint16_t)(file[i * 2] | (file[i * 2 + 1] << 8));
  if (sizes[id] >= 0xFF00) {
    std::fprintf(stderr, "res %d absent in this file (hdr=%#x)\n", id, sizes[id]);
    return 1;
  }
  std::size_t off = 768;
  for (int i = 0; i < id; ++i)
    if (sizes[i] < 0xFF00) off += sizes[i];
  if (off + sizes[id] > file.size()) {
    std::fprintf(stderr, "res %d off=%zu size=%u exceeds file %zu\n",
                 id, off, sizes[id], file.size());
    return 1;
  }
  std::span<const std::uint8_t> comp(file.data() + off, sizes[id]);
  auto out = dw::res::decompress(comp);
  std::FILE* of = std::fopen(argv[3], "wb");
  if (!of) { std::fprintf(stderr, "out open failed\n"); return 1; }
  std::fwrite(out.data(), 1, out.size(), of);
  std::fclose(of);
  std::fprintf(stderr, "res %d: comp off=%zu size=%u -> decompressed %zu bytes -> %s\n",
               id, off, sizes[id], out.size(), argv[3]);
  return 0;
}
