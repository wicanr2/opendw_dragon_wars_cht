// verify_viewport — viewport 解碼對拍:對 vp0 / vp2 / data6820 三個 template
// 各做 reset(0) + decode(tmpl, xpos=0, ypos=0, 0x50, 0) (與 golden harness
// 同參數),再把 mem 與對應 .vpmem golden 做 byte-for-byte memcmp。
//
// 用法: verify_viewport <viewport資產目錄>
//   目錄需含 <name>.bin (template) 與 <name>.bin.vpmem (golden),各 10880 bytes。
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <string>
#include <vector>

#include "render/viewport.hpp"

namespace {

std::vector<std::uint8_t> read_file(const std::string& path, bool& ok) {
  ok = false;
  std::FILE* f = std::fopen(path.c_str(), "rb");
  if (!f) return {};
  std::vector<std::uint8_t> buf;
  int c;
  while ((c = std::fgetc(f)) != EOF) buf.push_back(static_cast<std::uint8_t>(c));
  std::fclose(f);
  ok = true;
  return buf;
}

// 回傳 true = PASS。
bool verify_one(const std::string& dir, const std::string& name) {
  const std::string tmpl_path = dir + "/" + name + ".bin";
  const std::string gold_path = dir + "/" + name + ".bin.vpmem";

  bool ok1 = false, ok2 = false;
  std::vector<std::uint8_t> tmpl = read_file(tmpl_path, ok1);
  std::vector<std::uint8_t> gold = read_file(gold_path, ok2);

  if (!ok1) {
    std::printf("[%s] FAIL — 無法讀取 template %s\n", name.c_str(), tmpl_path.c_str());
    return false;
  }
  if (!ok2) {
    std::printf("[%s] FAIL — 無法讀取 golden %s\n", name.c_str(), gold_path.c_str());
    return false;
  }
  if (gold.size() != static_cast<std::size_t>(dw::render::ViewportDecoder::kViewportMemSize)) {
    std::printf("[%s] FAIL — golden 大小 %zu != %d\n",
                name.c_str(), gold.size(), dw::render::ViewportDecoder::kViewportMemSize);
    return false;
  }

  dw::render::ViewportDecoder dec;
  dec.reset(0);
  dec.decode(tmpl.data(), /*xpos=*/0, /*ypos=*/0, /*word_1053=*/0x50, /*byte_104E=*/0);

  if (std::memcmp(dec.mem.data(), gold.data(), gold.size()) == 0) {
    std::printf("[%s] PASS — mem 與 golden byte-for-byte 一致 (%zu bytes)\n",
                name.c_str(), gold.size());
    return true;
  }

  // 找第一個不符 offset。
  for (std::size_t i = 0; i < gold.size(); ++i) {
    if (dec.mem[i] != gold[i]) {
      std::printf("[%s] FAIL — 第一個不符 offset=%zu (0x%zX): remake=0x%02X golden=0x%02X\n",
                  name.c_str(), i, i, dec.mem[i], gold[i]);
      break;
    }
  }
  return false;
}

} // namespace

int main(int argc, char** argv) {
  if (argc < 2) {
    std::fprintf(stderr, "usage: %s <viewport資產目錄>\n", argv[0]);
    return 2;
  }
  const std::string dir = argv[1];
  const char* names[] = {"vp0", "vp2", "data6820"};

  int fails = 0;
  for (const char* n : names) {
    if (!verify_one(dir, n)) ++fails;
  }

  std::printf("\n結果: %d/%d PASS\n", 3 - fails, 3);
  return fails == 0 ? 0 : 1;
}
