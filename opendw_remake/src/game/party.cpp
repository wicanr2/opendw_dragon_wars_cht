#include "party.hpp"

#include <cstdio>
#include <cstring>

namespace dw::game {

namespace {

std::uint16_t rd16(const std::uint8_t* p, int off) {
  return static_cast<std::uint16_t>(p[off] | (p[off + 1] << 8));
}

// 角色名:從 record 起逐位元組讀,直到遇到「高位元已清除」的位元組(含)為止。
// (opendw write_character_name 0x1A40:每位元組 OR 0x80 後輸出,末位元組高位元為 0 即止。)
std::string read_name(const std::uint8_t* p) {
  std::string s;
  for (int i = 0; i < 16; ++i) {
    std::uint8_t b = p[i];
    char c = static_cast<char>(b & 0x7F);
    if (c >= 0x20 && c < 0x7F) s.push_back(c);
    if ((b & 0x80) == 0) break;
  }
  return s;
}

// 狀態條色(對照 opendw color_data[] = {00,FF,CC,AA,99} & 0x0F)。
//   draw_player_stat 傳入 color idx:2=HP、3=暈眩、4=法力。
constexpr std::uint8_t kBarColorHp    = 0x0C;  // color_data[2]=0xCC → 亮紅
constexpr std::uint8_t kBarColorStun  = 0x0A;  // color_data[3]=0xAA → 亮綠
constexpr std::uint8_t kBarColorPower = 0x09;  // color_data[4]=0x99 → 亮藍
constexpr std::uint8_t kBgColor       = 0x00;  // byte_1BE5 背景(color_data[0]=0x00 → 黑)

// 原版幾何(直接映射到 320×200 線性 framebuffer):
//   - get_line_offset(y) = y*320,故 y 即掃描線。
//   - 狀態條:ui_draw_horizontal_line x 單位 = 4 px(x<<2 byte off,2px/iter)。
//     範圍 word_36C0=0x36 .. word_36C2=0x4E → 像素 54*4=216 .. 78*4=312(寬 96px)。
//   - 名字列頂 y = char*0x10 + 0x20(char0=32, char1=48, char2=64, char3=80)。
constexpr int kRowStride = 0x10;   // 16 掃描線/角色
constexpr int kRowTop    = 0x20;   // 第一列頂(y=32)
constexpr int kBarX0px   = 0x36 * 4;  // 216
constexpr int kBarX1px   = 0x4E * 4;  // 312
constexpr int kBarUnitPx = 4;         // 每條長度單位 = 4px
constexpr int kNameXpx   = 0x1B * 8;  // draw_character x<<3,x=0x1B → 216

// 狀態 bitmask → 文字(對照 unknown_1BC1[0..3]={02,04,80,01} 與 str_table_status)。
// 檢查順序與 opendw 一致(si 3→0),回傳第一個命中的狀態字串;無則回 nullptr。
const char* status_text(std::uint8_t st) {
  if (st & 0x01) return "dead";       // si=3
  if (st & 0x80) return "stunned";    // si=2
  if (st & 0x04) return "poisoned";   // si=1
  if (st & 0x02) return "chained";    // si=0
  return nullptr;
}

void hline(render::Framebuffer& fb, int x0px, int x1px, int y, std::uint8_t color) {
  for (int x = x0px; x < x1px; ++x) fb.put(x, y, color);
}

// 對照 draw_player_stat(6599):畫一條狀態條(亮色比例 + 黑色剩餘),雙掃描線。
void draw_stat_bar(render::Framebuffer& fb, int y, std::uint16_t cur,
                   std::uint16_t max_val, std::uint8_t bar_color) {
  int filled_units = 0;  // 對照 cpu.ax:預設 = cur(夾在 0..23+ 範圍)
  if (cur != 0 && max_val != 0) {
    filled_units = (cur * 23) / max_val + 1;     // (cur*23)/max + 1
  } else {
    filled_units = cur ? cur : 0;
  }
  int split_px = kBarX0px + filled_units * kBarUnitPx;  // word_36C2 = 0x36 + units
  if (split_px > kBarX1px) split_px = kBarX1px;
  // 亮色段(雙線:y、y+1)
  hline(fb, kBarX0px, split_px, y, bar_color);
  hline(fb, kBarX0px, split_px, y + 1, bar_color);
  // 剩餘黑色段
  if (split_px < kBarX1px) {
    hline(fb, split_px, kBarX1px, y, kBgColor);
    hline(fb, split_px, kBarX1px, y + 1, kBgColor);
  }
}

}  // namespace

const char* Party::status_key(std::uint8_t status) {
  const char* st = status_text(status);
  return st ? st : "normal";
}

CharacterRecord Party::parse_record(const std::uint8_t* p) {
  CharacterRecord r;
  std::memcpy(r.raw.data(), p, 512);
  r.name = read_name(p);
  r.strength = p[0x0C]; r.max_strength = p[0x0D];
  r.dexterity = p[0x0E]; r.max_dexterity = p[0x0F];
  r.intel = p[0x10]; r.max_intel = p[0x11];
  r.spirit = p[0x12]; r.max_spirit = p[0x13];
  r.health = rd16(p, 0x14); r.max_health = rd16(p, 0x16);
  r.stun = rd16(p, 0x18); r.max_stun = rd16(p, 0x1A);
  r.power = rd16(p, 0x1C); r.max_power = rd16(p, 0x1E);
  r.status = p[0x4C];
  r.gender = p[0x4E];
  r.level = rd16(p, 0x4F);
  r.gold = static_cast<std::uint32_t>(p[0x55] | (p[0x56] << 8) |
                                      (p[0x57] << 16) | (p[0x58] << 24));
  return r;
}

Party Party::from_records(const std::vector<std::uint8_t>& bytes) {
  Party party;
  std::size_t n = bytes.size() / 512;
  for (std::size_t i = 0; i < n; ++i) {
    const std::uint8_t* p = bytes.data() + i * 512;
    // 空槽(record 起為 0x00 終止 + 0xFF 填充)視為無此角色 → 不加入隊伍。
    if (p[0] == 0x00 || p[0] == 0xFF) continue;
    party.members_.push_back(parse_record(p));
  }
  return party;
}

Party Party::load_default(const std::filesystem::path& bundle_dir) {
  std::filesystem::path p = bundle_dir / "party" / "default_party.bin";
  std::FILE* f = std::fopen(p.string().c_str(), "rb");
  if (!f) {
    std::fprintf(stderr, "party: open failed: %s\n", p.string().c_str());
    return Party{};
  }
  std::fseek(f, 0, SEEK_END);
  long sz = std::ftell(f);
  std::fseek(f, 0, SEEK_SET);
  std::vector<std::uint8_t> buf(sz > 0 ? static_cast<std::size_t>(sz) : 0);
  if (!buf.empty() && std::fread(buf.data(), 1, buf.size(), f) != buf.size()) {
    std::fclose(f);
    std::fprintf(stderr, "party: short read: %s\n", p.string().c_str());
    return Party{};
  }
  std::fclose(f);
  return from_records(buf);
}

void Party::draw_status_panel(render::Framebuffer& fb, render::TextLayer& tl,
                              int name_px) const {
  // 對照 draw_player_status_panel:最多 7 列,但實際依隊伍人數。
  for (std::size_t i = 0; i < members_.size() && i < 7; ++i) {
    const CharacterRecord& c = members_.at(i);
    int row_top = static_cast<int>(i) * kRowStride + kRowTop;  // y of name baseline area

    // ── 文字層:角色名字(列頂)。原版 append_spaces 置中,這裡靠左對齊條起點。──
    // 名字字級小一點以容入面板;白色(15)。
    tl.add(kNameXpx, row_top, c.name.empty() ? "?" : c.name, 15, name_px);

    const char* st = status_text(c.status);
    if (st) {
      // 異常狀態:原版於 row_top+8 顯示 "<name> is <status>";這裡顯示狀態字。
      char buf[48];
      std::snprintf(buf, sizeof buf, "is %s", st);
      tl.add(kNameXpx, row_top + 8, buf, 12, name_px);  // 亮紅提示
      continue;  // 異常時原版不畫三條狀態條
    }

    // ── 像素層:三條狀態條(HP/暈眩/法力)。──
    // 對照 draw_player_stat y_adjust:HP +8、暈眩 +0x0B、法力 +0x0E。
    draw_stat_bar(fb, row_top + 0x08, c.health, c.max_health, kBarColorHp);
    draw_stat_bar(fb, row_top + 0x0B, c.stun, c.max_stun, kBarColorStun);
    draw_stat_bar(fb, row_top + 0x0E, c.power, c.max_power, kBarColorPower);
  }
}

}  // namespace dw::game
