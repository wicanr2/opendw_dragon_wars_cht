// extract_items — 從 DATA1 萃取一組「byte-grounded 物品樣本」,寫成自包含 bundle。
//
// 目的:取得真實 23B 物品記錄,供 verify_equipment 對拍解碼(類型/AV/AC/名稱合理)。
//
// 為何用「curated 偏移」而非全檔掃描:
//   - default_party.bin 的物品欄全空(起始隊伍裝備為原版 runtime 給予,未存於 blob)。
//   - DATA1 內多數商店/物品記錄藏在壓縮資源裡;裸 DATA1 可直接定位的物品集中在
//     0x2C59 起的「召喚物/法術物品表」(Air Talons / Earth Shield / Fire Shield…)
//     與少數任務物品(Dragon Stone @0x5369)。
//   - 這些偏移已逐筆對 fraterrisus 規格(docs/44 §2)人工驗證解碼合理,故以
//     curated 清單萃取(每筆記下 DATA1 偏移 + 期望解碼,可追溯)。
//
// 萃取的是 11B header + 物品名(讀到高位元終止,補滿至 12B),即一格 23B 物品欄格式。
//
// 用法:extract_items <data_dir> <out_dir>
//   產出:<out_dir>/items/items.bin     (DWITM/1 格式,見下)
//        <out_dir>/items/manifest.json (人讀,逐筆 name + DATA1 偏移 + 解碼摘要)
//
// items.bin 格式(little-endian,自包含,執行期不需 DATA1):
//   magic   "DWITM\0" (6 bytes)
//   version u16 = 1
//   count   u16
//   repeated count 次:
//     data1_off u32   (該物品在 DATA1 的 byte 偏移,供追溯/對拍)
//     rec[23]         (11B header + 12B 名;名不足以 0 填補)

#include <array>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

namespace {

// curated 樣本:DATA1 內已驗證的物品 11B header 偏移(name 緊接其後)。
// 每筆附「期望」摘要,純供 manifest 人讀比對(verify_equipment 才做斷言)。
struct Sample {
  std::uint32_t off;       // DATA1 內 11B header 起點
  const char* expect_name; // 期望物品名(對拍)
  const char* note;        // 期望解碼摘要(人讀)
};

// 來源:dw_lc/data1(DOS 版);偏移經 fraterrisus 規格逐筆驗證(見檔頭)。
const Sample kSamples[] = {
    {0x2C78, "Air Talons",   "type=two_hander(0x06), primary 1d12"},
    {0x2C8D, "Air Armor",    "type=cuir_bouilli(0x10), AVmod=-8"},
    {0x2CC8, "Earth Shield", "type=shield(0x01), AVmod=-10"},
    {0x2D40, "Fire Shield",  "type=shield(0x01), AVmod=-15"},
    {0x2DA5, "Aura Shield",  "type=shield(0x01), AVmod=-12"},
    {0x2D04, "Water Wings",  "type=cuir_bouilli(0x10), AVmod=-11"},
    {0x5369, "Dragon Stone", "type=general(0x00), price=250, magic=restore_power"},
};
constexpr int kSampleCount = sizeof(kSamples) / sizeof(kSamples[0]);

void put_u16(std::vector<std::uint8_t>& v, std::uint16_t x) {
  v.push_back(static_cast<std::uint8_t>(x & 0xFF));
  v.push_back(static_cast<std::uint8_t>(x >> 8));
}
void put_u32(std::vector<std::uint8_t>& v, std::uint32_t x) {
  for (int i = 0; i < 4; ++i) v.push_back(static_cast<std::uint8_t>((x >> (8 * i)) & 0xFF));
}

// 讀 DATA1 原始檔(不經 Archive 資源解碼;物品在 raw DATA1 偏移處)。
std::vector<std::uint8_t> read_file(const std::filesystem::path& p) {
  std::vector<std::uint8_t> v;
  std::FILE* f = std::fopen(p.string().c_str(), "rb");
  if (!f) return v;
  std::fseek(f, 0, SEEK_END);
  long n = std::ftell(f);
  std::fseek(f, 0, SEEK_SET);
  if (n > 0) {
    v.resize(static_cast<std::size_t>(n));
    if (std::fread(v.data(), 1, v.size(), f) != v.size()) v.clear();
  }
  std::fclose(f);
  return v;
}

// 高位元終止字串(物品名)。
std::string read_name(const std::vector<std::uint8_t>& d, std::size_t off) {
  std::string s;
  for (std::size_t i = 0; i < 12 && off + i < d.size(); ++i) {
    std::uint8_t b = d[off + i];
    char c = static_cast<char>(b & 0x7F);
    if (c >= 0x20 && c < 0x7F) s.push_back(c);
    if ((b & 0x80) == 0) break;
  }
  return s;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 3) {
    std::fprintf(stderr, "usage: %s <data_dir> <out_dir>\n", argv[0]);
    return 2;
  }
  const std::filesystem::path data_dir = argv[1];
  const std::filesystem::path out_dir = argv[2];

  // 直接讀 raw DATA1(物品在裸偏移處,非壓縮資源)。
  std::vector<std::uint8_t> d1 = read_file(data_dir / "data1");
  if (d1.empty()) d1 = read_file(data_dir / "DATA1");
  if (d1.empty()) {
    std::fprintf(stderr, "extract_items: cannot read DATA1 in %s\n", argv[1]);
    return 1;
  }

  struct Out { std::uint32_t off; std::array<std::uint8_t, 23> rec; std::string name; };
  std::vector<Out> outs;
  for (const auto& s : kSamples) {
    if (s.off + 11 > d1.size()) {
      std::fprintf(stderr, "extract_items: offset 0x%X out of range\n", s.off);
      continue;
    }
    Out o;
    o.off = s.off;
    o.rec.fill(0);
    for (int i = 0; i < 11; ++i) o.rec[i] = d1[s.off + i];
    o.name = read_name(d1, s.off + 11);
    // 名字補入 rec[11..]，高位元終止格式(末位元組高位元清除),不足以 0 填。
    for (std::size_t i = 0; i < o.name.size() && i < 12; ++i) {
      std::uint8_t b = static_cast<std::uint8_t>(o.name[i]);
      if (i + 1 < o.name.size()) b |= 0x80;  // 除末位元組外皆設高位元
      o.rec[11 + i] = b;
    }
    outs.push_back(std::move(o));
    std::printf("  0x%05X '%s' (%s)\n", s.off, outs.back().name.c_str(), s.note);
  }

  // 組 blob。
  std::vector<std::uint8_t> blob;
  const char magic[6] = {'D', 'W', 'I', 'T', 'M', '\0'};
  blob.insert(blob.end(), magic, magic + 6);
  put_u16(blob, 1);
  put_u16(blob, static_cast<std::uint16_t>(outs.size()));
  for (const auto& o : outs) {
    put_u32(blob, o.off);
    blob.insert(blob.end(), o.rec.begin(), o.rec.end());
  }

  std::error_code ec;
  std::filesystem::create_directories(out_dir / "items", ec);
  const auto bin_path = out_dir / "items" / "items.bin";
  if (std::FILE* f = std::fopen(bin_path.string().c_str(), "wb")) {
    std::fwrite(blob.data(), 1, blob.size(), f);
    std::fclose(f);
  } else {
    std::fprintf(stderr, "extract_items: cannot write %s\n", bin_path.string().c_str());
    return 1;
  }

  // manifest(人讀)。
  std::string mf = "{\n  \"format\": \"opendw-items/1\",\n";
  mf += "  \"source\": \"DATA1 (DOS) curated item offsets, fraterrisus-verified\",\n";
  mf += "  \"spec\": \"docs/44_DATA_FORMATS_AND_MECHANICS.md §2 (fraterrisus Equipment Format)\",\n";
  mf += "  \"slot_bytes\": 23,\n";
  mf += "  \"count\": " + std::to_string(outs.size()) + ",\n";
  mf += "  \"items\": [\n";
  for (std::size_t i = 0; i < outs.size(); ++i) {
    char hex[16];
    std::snprintf(hex, sizeof hex, "0x%05X", outs[i].off);
    std::string nm;
    for (char c : outs[i].name) { if (c == '\\' || c == '"') nm.push_back('\\'); nm.push_back(c); }
    mf += "    { \"name\": \"" + nm + "\", \"data1_off\": \"" + hex +
          "\", \"expect\": \"" + kSamples[i].note + "\" }";
    mf += (i + 1 < outs.size()) ? ",\n" : "\n";
  }
  mf += "  ]\n}\n";
  const auto mf_path = out_dir / "items" / "manifest.json";
  if (std::FILE* f = std::fopen(mf_path.string().c_str(), "wb")) {
    std::fwrite(mf.data(), 1, mf.size(), f);
    std::fclose(f);
  }

  std::printf("extract_items: wrote %zu items -> %s\n", outs.size(), bin_path.string().c_str());
  return 0;
}
