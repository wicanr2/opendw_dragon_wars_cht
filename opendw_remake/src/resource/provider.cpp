#include "provider.hpp"

#include <cstdio>

namespace dw::res {

std::optional<Data1Provider> Data1Provider::open(const std::filesystem::path& data_dir) {
  auto a = Archive::open(data_dir);
  if (!a) return std::nullopt;
  return Data1Provider(std::move(*a));
}

std::optional<std::vector<std::uint8_t>> Data1Provider::load(int id) const {
  return arc_.load(id);
}

std::optional<std::vector<std::uint8_t>> BundleProvider::load(int id) const {
  char name[32];
  // 關卡自身資源(level-self,tag = area+0x46,area 0..39 → 0x46..0x6D)直接由
  // maps/<area>.lvl 提供:該 .lvl 與 DATA1 resource_load(area+0x46) byte-for-byte 相同
  // (已驗證),故 op_58 跨資源 call 進「關卡自身」時不必把 40 份 .lvl 重複塞進 scripts/。
  // 這讓 BundleProvider 對 level-self 也自包含,不再需要呼叫端的 `tag == level_res` 特例。
  if (id >= 0x46 && id <= 0x6D) {
    std::snprintf(name, sizeof(name), "maps/%d.lvl", id - 0x46);
    std::filesystem::path lp = dir_ / name;
    if (std::FILE* lf = std::fopen(lp.string().c_str(), "rb")) {
      std::fseek(lf, 0, SEEK_END);
      long ln = std::ftell(lf);
      std::fseek(lf, 0, SEEK_SET);
      std::vector<std::uint8_t> lbuf(ln > 0 ? static_cast<std::size_t>(ln) : 0);
      if (!lbuf.empty() && std::fread(lbuf.data(), 1, lbuf.size(), lf) != lbuf.size()) {
        std::fclose(lf);
        return std::nullopt;
      }
      std::fclose(lf);
      return lbuf;
    }
    // 沒有對應 .lvl 就落回 scripts/<id>.bin(維持彈性)。
  }
  std::snprintf(name, sizeof(name), "scripts/%d.bin", id);
  std::filesystem::path p = dir_ / name;
  std::FILE* f = std::fopen(p.string().c_str(), "rb");
  if (!f) return std::nullopt;
  std::fseek(f, 0, SEEK_END);
  long n = std::ftell(f);
  std::fseek(f, 0, SEEK_SET);
  std::vector<std::uint8_t> buf(n > 0 ? static_cast<std::size_t>(n) : 0);
  if (!buf.empty() && std::fread(buf.data(), 1, buf.size(), f) != buf.size()) {
    std::fclose(f);
    return std::nullopt;
  }
  std::fclose(f);
  return buf;
}

}  // namespace dw::res
