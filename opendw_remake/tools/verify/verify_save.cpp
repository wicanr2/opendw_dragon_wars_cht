// verify_save — 存檔/讀檔 round-trip 確定性自測(不需 SDL,印 PASS/FAIL)。
//
// 驗證鐵則(prompt):存→讀→存,兩份檔 byte-for-byte 一致,且逐欄位完全還原
//   (area / x / y / facing / game_state[256] / party records)。
//
// 流程:
//   1) 構造一份非平凡 SaveState(含 4 筆 512B party record + 滿佈的 game_state)。
//   2) save() 到檔 A → load() 讀回 → 逐欄位與原始比對。
//   3) 把讀回的再 save() 到檔 B → cmp A、B byte-for-byte。
//   4) 額外:壞魔數 / 壞版本 / 截斷檔 → load() 必須拒絕(回 false)。
#include <array>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <vector>

#include "game/savegame.hpp"

using dw::game::SaveState;

namespace {

int g_fail = 0;
void check(bool cond, const char* what) {
  std::printf("  [%s] %s\n", cond ? "ok" : "FAIL", what);
  if (!cond) ++g_fail;
}

std::vector<std::uint8_t> read_all(const std::filesystem::path& p) {
  std::vector<std::uint8_t> v;
  std::FILE* f = std::fopen(p.string().c_str(), "rb");
  if (!f) return v;
  int c;
  while ((c = std::fgetc(f)) != EOF) v.push_back((std::uint8_t)c);
  std::fclose(f);
  return v;
}

SaveState make_state() {
  SaveState s;
  s.area = 7;
  s.x = 12;
  s.y = 34;
  s.facing = 3;
  for (int i = 0; i < 256; ++i)
    s.game_state[i] = (std::uint8_t)((i * 31 + 13) & 0xFF);
  // 4 名角色,各 512B record(含確定性內容 + 名字編碼樣式 + 高位元組欄位)。
  for (int r = 0; r < 4; ++r) {
    std::array<std::uint8_t, 512> rec{};
    for (int i = 0; i < 512; ++i)
      rec[i] = (std::uint8_t)((i * 3 + r * 17 + 1) & 0xFF);
    rec[0] = (std::uint8_t)('A' + r);          // 非空槽(name 起始;不可為 0x00/0xFF)
    s.party_records.push_back(rec);
  }
  return s;
}

bool fields_equal(const SaveState& a, const SaveState& b) {
  if (a.area != b.area || a.x != b.x || a.y != b.y || a.facing != b.facing)
    return false;
  if (a.game_state != b.game_state) return false;
  if (a.party_records.size() != b.party_records.size()) return false;
  for (std::size_t i = 0; i < a.party_records.size(); ++i)
    if (a.party_records[i] != b.party_records[i]) return false;
  return true;
}

}  // namespace

int main() {
  namespace fs = std::filesystem;
  fs::path dir = "_verify_save_tmp";
  fs::create_directories(dir);
  fs::path a = dir / "a.sav";
  fs::path b = dir / "b.sav";

  std::printf("verify_save: round-trip\n");
  SaveState orig = make_state();

  // 1) save A
  check(dw::game::save(orig, a), "save A");

  // 2) load A → 逐欄位比對
  SaveState loaded;
  check(dw::game::load(a, loaded), "load A");
  check(fields_equal(orig, loaded), "field-by-field equal (area/x/y/facing/game_state/party)");

  // 3) save B(用讀回的)→ cmp A、B byte-for-byte
  check(dw::game::save(loaded, b), "save B");
  auto ba = read_all(a), bb = read_all(b);
  check(!ba.empty() && ba == bb, "save->load->save byte-for-byte identical");

  // 4) 健全性:壞魔數 / 壞版本 / 截斷 → load 必須拒絕
  {
    auto buf = ba;
    buf[0] ^= 0xFF;  // 破壞魔數
    std::FILE* f = std::fopen((dir / "badmagic.sav").string().c_str(), "wb");
    std::fwrite(buf.data(), 1, buf.size(), f); std::fclose(f);
    SaveState junk;
    check(!dw::game::load(dir / "badmagic.sav", junk), "reject bad magic");
  }
  {
    auto buf = ba;
    buf[4] = 0xEE; buf[5] = 0xEE;  // 破壞版本(offset 4..5)
    std::FILE* f = std::fopen((dir / "badver.sav").string().c_str(), "wb");
    std::fwrite(buf.data(), 1, buf.size(), f); std::fclose(f);
    SaveState junk;
    check(!dw::game::load(dir / "badver.sav", junk), "reject bad version");
  }
  {
    auto buf = ba; buf.resize(buf.size() / 2);  // 截斷
    std::FILE* f = std::fopen((dir / "trunc.sav").string().c_str(), "wb");
    std::fwrite(buf.data(), 1, buf.size(), f); std::fclose(f);
    SaveState junk;
    check(!dw::game::load(dir / "trunc.sav", junk), "reject truncated file");
  }
  {
    SaveState junk;
    check(!dw::game::load(dir / "does_not_exist.sav", junk), "reject missing file");
  }

  std::error_code ec; fs::remove_all(dir, ec);
  std::printf("%s: verify_save\n", g_fail == 0 ? "PASS" : "FAIL");
  return g_fail == 0 ? 0 : 1;
}
