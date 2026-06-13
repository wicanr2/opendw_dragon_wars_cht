// extract_monsters — 從 DATA1 res31 萃取怪物記錄,寫成自包含 bundle 資源。
//
// Oracle 對齊(唯讀):opendw src/tools/monster_info.cpp 與 docs/26_MONSTERS_AND_SPRITES.md。
//   res31 解壓後 2177 bytes。怪物記錄為「變長」結構:
//     - record 起點 + 0x00 .. +0x20:21 bytes 戰鬥屬性(語意大多未由 opendw 解出,
//       已知 byte[0x0B] = sprite 資源索引基底,sprite_res = (byte[0x0B] << 1) + 0x8A,
//       見 engine.c trigger_random_encounter @0x4818)。
//     - record 起點 + 0x21:5-bit 壓縮名字(含 0xAF/0xDC 單複數 escape)。
//     - 名字結束的 byte offset = 下一筆 record 的起點(text::decode 回傳 next offset)。
//   第一筆 record 起點 = 0x022C(經 dump 對齊 opendw monster_info 輸出與 doc 表)。
//
// 重要誠實揭露:opendw **沒有**實作戰鬥結算(to-hit / 傷害 / HP 扣減 / 死亡),
//   其 C 碼只到「載入怪物圖 + 動畫」(check_random_encounter_timer / update_monster_animation)。
//   因此 21 bytes 屬性的逐欄語意(HP/攻擊/AC/傷害骰)無 oracle 可對;本工具忠實搬「位元組」,
//   不臆測欄位語意。語意推斷留待後續(見 verify_combat 註解與回報)。
//
// 用法:extract_monsters <data_dir> <out_dir>
//   產出:<out_dir>/monsters/monsters.bin  (固定格式,見下 MonsterBlob)
//        <out_dir>/monsters/manifest.json
//
// monsters.bin 格式(little-endian,自包含,執行期不需 DATA1):
//   magic   "DWMON\0"  (6 bytes)
//   version u16 = 1
//   count   u16
//   repeated count 次:
//     attr[21]    21 bytes 原始屬性(record+0x00..+0x20,byte-for-byte)
//     name_len    u8
//     name[name_len]  UTF-8 名字(含 escape 還原後的字面;不含結尾 0)
//
// 此格式把「原始 21 bytes」與「已解碼名字」一起保存,讓 game::combat 載入時
// 不需再帶 5-bit 解碼器,且仍可 byte-for-byte 驗證屬性。

#include "resource/archive.hpp"
#include "resource/text_codec.hpp"

#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

namespace {

constexpr std::size_t kFirstRecord = 0x022C;  // 第一筆怪物 record 起點
constexpr std::size_t kAttrLen = 21;          // record+0x00..+0x20
constexpr std::size_t kNameOff = 0x21;        // 名字在 record+0x21
constexpr int kMaxRecords = 40;               // 安全上限(實際 25 筆)

void put_u16(std::vector<std::uint8_t>& v, std::uint16_t x) {
  v.push_back(static_cast<std::uint8_t>(x & 0xFF));
  v.push_back(static_cast<std::uint8_t>((x >> 8) & 0xFF));
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 3) {
    std::fprintf(stderr, "usage: %s <data_dir> <out_dir>\n", argv[0]);
    return 2;
  }
  const std::filesystem::path data_dir = argv[1];
  const std::filesystem::path out_dir = argv[2];

  auto arc = dw::res::Archive::open(data_dir);
  if (!arc) {
    std::fprintf(stderr, "extract_monsters: cannot open DATA1 in %s\n", argv[1]);
    return 1;
  }
  auto res31 = arc->load(31);
  if (!res31) {
    std::fprintf(stderr, "extract_monsters: load/decompress res31 failed\n");
    return 1;
  }
  if (res31->size() != 2177) {
    std::fprintf(stderr,
                 "extract_monsters: WARNING res31 len=%zu (oracle=2177)\n",
                 res31->size());
  }

  struct Rec {
    std::array<std::uint8_t, kAttrLen> attr{};
    std::string name;
  };
  std::vector<Rec> recs;

  std::size_t rec = kFirstRecord;
  for (int i = 0; i < kMaxRecords; ++i) {
    if (rec + kNameOff >= res31->size()) break;
    auto [name, next] = dw::text::decode(*res31, rec + kNameOff);
    if (name.empty()) break;  // 表尾雜訊 → 停止(與 doc 一致)
    Rec r;
    for (std::size_t k = 0; k < kAttrLen; ++k) r.attr[k] = (*res31)[rec + k];
    r.name = std::move(name);
    recs.push_back(std::move(r));
    rec = next;  // 名字結束處 = 下一筆 record 起點
  }

  // 組 blob。
  std::vector<std::uint8_t> blob;
  const char magic[6] = {'D', 'W', 'M', 'O', 'N', '\0'};
  blob.insert(blob.end(), magic, magic + 6);
  put_u16(blob, 1);
  put_u16(blob, static_cast<std::uint16_t>(recs.size()));
  for (const auto& r : recs) {
    blob.insert(blob.end(), r.attr.begin(), r.attr.end());
    blob.push_back(static_cast<std::uint8_t>(r.name.size()));
    blob.insert(blob.end(), r.name.begin(), r.name.end());
  }

  std::error_code ec;
  std::filesystem::create_directories(out_dir / "monsters", ec);
  const auto bin_path = out_dir / "monsters" / "monsters.bin";
  if (std::FILE* f = std::fopen(bin_path.string().c_str(), "wb")) {
    std::fwrite(blob.data(), 1, blob.size(), f);
    std::fclose(f);
  } else {
    std::fprintf(stderr, "extract_monsters: cannot write %s\n",
                 bin_path.string().c_str());
    return 1;
  }

  // manifest(人讀;含逐筆 name + sprite 索引基底,供核對)。
  std::string mf = "{\n  \"format\": \"opendw-monsters/1\",\n";
  mf += "  \"source\": \"DATA1 res31 (decompressed 2177B), record-aligned\",\n";
  mf += "  \"oracle\": \"opendw src/tools/monster_info.cpp + docs/26_MONSTERS_AND_SPRITES.md\",\n";
  mf += "  \"attr_bytes\": 21,\n";
  mf += "  \"known_fields\": { \"0x0B\": \"sprite resource index base; sprite_res=(b<<1)+0x8A\" },\n";
  mf += "  \"unknown_fields_note\": \"HP/attack/AC/dice semantics NOT decoded by opendw (no combat resolution in oracle C); raw bytes preserved verbatim\",\n";
  mf += "  \"count\": " + std::to_string(recs.size()) + ",\n";
  mf += "  \"monsters\": [\n";
  for (std::size_t i = 0; i < recs.size(); ++i) {
    char hex[8];
    std::snprintf(hex, sizeof(hex), "0x%02X", recs[i].attr[0x0B]);
    // JSON-escape 名字裡的反斜線與引號。
    std::string nm;
    for (char c : recs[i].name) {
      if (c == '\\' || c == '"') nm.push_back('\\');
      nm.push_back(c);
    }
    mf += "    { \"index\": " + std::to_string(i) + ", \"name\": \"" + nm +
          "\", \"sprite_base\": \"" + hex + "\" }";
    mf += (i + 1 < recs.size()) ? ",\n" : "\n";
  }
  mf += "  ]\n}\n";
  const auto mf_path = out_dir / "monsters" / "manifest.json";
  if (std::FILE* f = std::fopen(mf_path.string().c_str(), "wb")) {
    std::fwrite(mf.data(), 1, mf.size(), f);
    std::fclose(f);
  }

  std::printf("extract_monsters: wrote %zu monsters -> %s (%zu bytes)\n",
              recs.size(), bin_path.string().c_str(), blob.size());
  for (std::size_t i = 0; i < recs.size(); ++i) {
    std::printf("  %2zu  sprite_base=0x%02X  '%s'\n", i, recs[i].attr[0x0B],
                recs[i].name.c_str());
  }
  return 0;
}
