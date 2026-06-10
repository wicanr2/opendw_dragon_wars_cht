// verify_r0 — R0 驗證:用 remake 的 archive + text_codec 解出 section 0 的內嵌字串,
// 對拍已知乾淨輸出(opendw disasm 產生的 ALL_TEXT_FROM_SCRIPTS)。
//
// 用法: verify_r0 <data_dir>     (data_dir 內需有 data1)
// 預期: 印出 Interplay / Do you wish to.. / Begin a new game ... 等乾淨字串。
#include <cstdio>
#include "../../src/resource/archive.hpp"
#include "../../src/resource/text_codec.hpp"

int main(int argc, char** argv) {
  if (argc < 2) {
    std::fprintf(stderr, "usage: %s <data_dir>\n", argv[0]);
    return 2;
  }
  auto arc = dw::res::Archive::open(argv[1]);
  if (!arc) {
    std::fprintf(stderr, "failed to open data1 in %s\n", argv[1]);
    return 1;
  }
  auto sec = arc->load(0);  // RESOURCE_SCRIPT, 未壓縮
  if (!sec) {
    std::fprintf(stderr, "failed to load section 0\n");
    return 1;
  }
  std::printf("section 0: %zu bytes\n", sec->size());

  // 內嵌字串緊接 op_77(draw_and_set) / op_78(set_msg) / op_7B(ui_header) 之後,byte 對齊。
  int count = 0;
  for (std::size_t p = 0; p + 1 < sec->size(); ++p) {
    std::uint8_t op = (*sec)[p];
    if (op != 0x77 && op != 0x78 && op != 0x7B) continue;
    auto [text, next] = dw::text::decode(*sec, p + 1);
    int printable = 0;
    for (char c : text)
      if (c >= 32 && c < 127) ++printable;
    if (text.size() >= 4 && printable * 100 / static_cast<int>(text.size()) > 85) {
      std::printf("  op_%02X @0x%zx: \"", op, p);
      for (char c : text) std::printf("%c", c == '\r' ? '/' : c);
      std::printf("\"\n");
      ++count;
    }
  }
  std::printf("decoded %d clean inline strings\n", count);
  return 0;
}
