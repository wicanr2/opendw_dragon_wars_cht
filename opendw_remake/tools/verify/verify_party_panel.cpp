// verify_party_panel — 載入預設隊伍 bundle,印出解析後的 record 欄位(供與 opendw
// player_record 佈局逐項對照),並把面板「像素層」(狀態條,不含 TTF 名字)dump 成 PPM。
//
// 用法:verify_party_panel <bundle_dir> [out.ppm]
//
// 像素層可獨立驗證(不需 SDL2_ttf);名字屬文字層,由 SDL app --dump 視覺驗證。
#include <cstdio>
#include <cstdlib>

#include "game/party.hpp"
#include "render/framebuffer.hpp"
#include "render/text_layer.hpp"

using namespace dw;

int main(int argc, char** argv) {
  if (argc < 2) {
    std::fprintf(stderr, "usage: %s <bundle_dir> [out.ppm]\n", argv[0]);
    return 2;
  }
  game::Party party = game::Party::load_default(argv[1]);
  if (party.size() == 0) {
    std::fprintf(stderr, "no party loaded\n");
    return 1;
  }

  std::printf("party size: %zu\n", party.size());
  for (std::size_t i = 0; i < party.size(); ++i) {
    const auto& c = party.at(i);
    std::printf("[%zu] name=%-10s STR %u/%u DEX %u/%u INT %u/%u SPI %u/%u | "
                "HP %u/%u STUN %u/%u PWR %u/%u | status=0x%02X gender=%u level=%u gold=%u\n",
                i, c.name.c_str(),
                c.strength, c.max_strength, c.dexterity, c.max_dexterity,
                c.intel, c.max_intel, c.spirit, c.max_spirit,
                c.health, c.max_health, c.stun, c.max_stun, c.power, c.max_power,
                c.status, c.gender, c.level, c.gold);
  }

  // 像素層 dump:背景填 1(深藍,模擬 in-game 右側區),畫狀態條。
  render::Framebuffer fb;
  fb.clear(1);
  render::TextLayer tl;  // 不 flush;只接住 add() 呼叫(名字),不影響像素層。
  party.draw_status_panel(fb, tl);

  if (argc >= 3) {
    std::FILE* f = std::fopen(argv[2], "wb");
    if (!f) { std::fprintf(stderr, "open out failed: %s\n", argv[2]); return 1; }
    fb.write_ppm(f);
    std::fclose(f);
    std::fprintf(stderr, "wrote pixel-layer PPM to %s\n", argv[2]);
  }
  return 0;
}
