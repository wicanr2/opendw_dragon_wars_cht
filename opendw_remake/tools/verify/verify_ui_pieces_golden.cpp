// verify_ui_pieces_golden — 遊戲內 UI chrome(石磚邊框 + Dragon Wars logo + pillar)
//   渲染對拍 oracle(byte-for-byte)。
//
// oracle 路徑(唯讀):opendw src/lib/ui.c
//   draw_ui_piece()@546(nibble→DOS16,x=dx*4,y=y_pos,每 byte 2px hi/lo)+
//   ui_draw()@744 靜態邊框序列 + draw_right_pillar()@720 + ui_header_draw()@762。
//   golden PPM 由 tools_build/ui_pieces_golden/golden_ui_pieces.c 以「同一份 ui_pieces 資料 +
//   同一個 draw_ui_piece 解碼」直接從 DRAGON.COM 渲染 320×200 framebuffer 產生
//   (見該檔;與本 remake 的 UiPieces::draw_chrome 互為獨立實作)。
//
// 本工具:載 bundle 的 viewport/ui_pieces.bin,用 remake 的 UiPieces::draw_chrome
//   重建 320×200 framebuffer → PPM,與 oracle golden PPM 逐 byte 比對。
//   任何幾何/定位/nibble 解碼/調色盤偏差都會被抓出。
//
// 用法:verify_ui_pieces_golden <ui_pieces.bin> <golden_ppm>
// 退出碼:0=PASS(byte-for-byte 相同),非 0=FAIL。

#include "render/framebuffer.hpp"
#include "render/ui_pieces.hpp"

#include <cstdint>
#include <cstdio>
#include <vector>

namespace {
std::vector<std::uint8_t> read_file(const char* p) {
  std::FILE* f = std::fopen(p, "rb");
  if (!f) return {};
  std::fseek(f, 0, SEEK_END); long n = std::ftell(f); std::fseek(f, 0, SEEK_SET);
  std::vector<std::uint8_t> v(n > 0 ? (std::size_t)n : 0);
  if (!v.empty() && std::fread(v.data(), 1, v.size(), f) != v.size()) v.clear();
  std::fclose(f); return v;
}
}  // namespace

int main(int argc, char** argv) {
  if (argc < 3) {
    std::fprintf(stderr, "usage: %s <ui_pieces.bin> <golden_ppm>\n", argv[0]);
    return 2;
  }
  auto ui = dw::render::UiPieces::load(argv[1]);
  if (!ui) { std::fprintf(stderr, "FAIL: load ui_pieces.bin %s\n", argv[1]); return 1; }

  dw::render::Framebuffer fb;
  fb.clear(0);
  ui->draw_chrome(fb);

  // 重建 PPM(P6 320 200 255 + RGB),與 Framebuffer::write_ppm 同格式。
  std::vector<std::uint8_t> mine;
  {
    FILE* tmp = std::tmpfile();
    fb.write_ppm(tmp);
    std::fseek(tmp, 0, SEEK_END); long n = std::ftell(tmp); std::fseek(tmp, 0, SEEK_SET);
    mine.resize(n > 0 ? (std::size_t)n : 0);
    if (!mine.empty() && std::fread(mine.data(), 1, mine.size(), tmp) != mine.size())
      mine.clear();
    std::fclose(tmp);
  }
  if (mine.empty()) { std::fprintf(stderr, "FAIL: render ppm\n"); return 1; }

  auto gold = read_file(argv[2]);
  if (gold.empty()) { std::fprintf(stderr, "FAIL: read golden %s\n", argv[2]); return 1; }

  if (mine.size() != gold.size()) {
    std::fprintf(stderr, "FAIL: size mine=%zu golden=%zu\n", mine.size(), gold.size());
    return 1;
  }
  std::size_t diff = 0, first = (std::size_t)-1;
  for (std::size_t i = 0; i < mine.size(); ++i)
    if (mine[i] != gold[i]) { if (first == (std::size_t)-1) first = i; ++diff; }

  if (diff == 0) {
    std::printf("PASS: in-game UI chrome == oracle golden byte-for-byte (320x200, %zu bytes)\n",
                mine.size());
    return 0;
  }
  std::fprintf(stderr, "FAIL: %zu/%zu bytes differ (first @%zu)\n", diff, mine.size(), first);
  return 1;
}
