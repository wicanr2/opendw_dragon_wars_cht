#include "savegame.hpp"

#include <cstdio>
#include <cstring>
#include <system_error>

namespace dw::game {

namespace {

// little-endian 定長寫入(確定性,跨機器一致)。
void put_u16(std::vector<std::uint8_t>& b, std::uint16_t v) {
  b.push_back(static_cast<std::uint8_t>(v & 0xFF));
  b.push_back(static_cast<std::uint8_t>((v >> 8) & 0xFF));
}
void put_u32(std::vector<std::uint8_t>& b, std::uint32_t v) {
  b.push_back(static_cast<std::uint8_t>(v & 0xFF));
  b.push_back(static_cast<std::uint8_t>((v >> 8) & 0xFF));
  b.push_back(static_cast<std::uint8_t>((v >> 16) & 0xFF));
  b.push_back(static_cast<std::uint8_t>((v >> 24) & 0xFF));
}
void put_i32(std::vector<std::uint8_t>& b, std::int32_t v) {
  put_u32(b, static_cast<std::uint32_t>(v));
}

// little-endian 定長讀取(附界限檢查;不足回 false)。
struct Reader {
  const std::uint8_t* p;
  std::size_t n;
  std::size_t i = 0;
  bool need(std::size_t k) const { return i + k <= n; }
  bool get_u16(std::uint16_t& v) {
    if (!need(2)) return false;
    v = static_cast<std::uint16_t>(p[i] | (p[i + 1] << 8));
    i += 2;
    return true;
  }
  bool get_u32(std::uint32_t& v) {
    if (!need(4)) return false;
    v = static_cast<std::uint32_t>(p[i] | (p[i + 1] << 8) |
                                   (p[i + 2] << 16) | (p[i + 3] << 24));
    i += 4;
    return true;
  }
  bool get_i32(std::int32_t& v) {
    std::uint32_t u;
    if (!get_u32(u)) return false;
    v = static_cast<std::int32_t>(u);
    return true;
  }
  bool get_bytes(std::uint8_t* dst, std::size_t k) {
    if (!need(k)) return false;
    std::memcpy(dst, p + i, k);
    i += k;
    return true;
  }
};

}  // namespace

bool save(const SaveState& st, const std::filesystem::path& path) {
  std::vector<std::uint8_t> b;
  // 檔頭:魔數(4) + 版本(2)。
  b.insert(b.end(), kSaveMagic, kSaveMagic + 4);
  put_u16(b, kSaveVersion);
  // 玩家位置/朝向/區域。
  put_i32(b, st.area);
  put_i32(b, st.x);
  put_i32(b, st.y);
  put_i32(b, st.facing);
  // 完整 game_state[256]。
  b.insert(b.end(), st.game_state.begin(), st.game_state.end());
  // 隊伍:人數(u16) + 每名 512B 原始 record。
  put_u16(b, static_cast<std::uint16_t>(st.party_records.size()));
  for (const auto& rec : st.party_records)
    b.insert(b.end(), rec.begin(), rec.end());

  // 自動建立上層目錄(如 save/)。
  std::error_code ec;
  if (path.has_parent_path())
    std::filesystem::create_directories(path.parent_path(), ec);

  std::FILE* f = std::fopen(path.string().c_str(), "wb");
  if (!f) {
    std::fprintf(stderr, "savegame: open for write failed: %s\n",
                 path.string().c_str());
    return false;
  }
  std::size_t w = std::fwrite(b.data(), 1, b.size(), f);
  std::fclose(f);
  if (w != b.size()) {
    std::fprintf(stderr, "savegame: short write: %s\n", path.string().c_str());
    return false;
  }
  return true;
}

bool load(const std::filesystem::path& path, SaveState& out) {
  std::FILE* f = std::fopen(path.string().c_str(), "rb");
  if (!f) return false;
  std::fseek(f, 0, SEEK_END);
  long sz = std::ftell(f);
  std::fseek(f, 0, SEEK_SET);
  if (sz <= 0) {
    std::fclose(f);
    return false;
  }
  std::vector<std::uint8_t> b(static_cast<std::size_t>(sz));
  std::size_t r = std::fread(b.data(), 1, b.size(), f);
  std::fclose(f);
  if (r != b.size()) return false;

  Reader rd{b.data(), b.size()};
  // 校驗魔數。
  std::uint8_t magic[4];
  if (!rd.get_bytes(magic, 4) || std::memcmp(magic, kSaveMagic, 4) != 0) {
    std::fprintf(stderr, "savegame: bad magic: %s\n", path.string().c_str());
    return false;
  }
  // 校驗版本。
  std::uint16_t ver;
  if (!rd.get_u16(ver) || ver != kSaveVersion) {
    std::fprintf(stderr, "savegame: unsupported version %u: %s\n", ver,
                 path.string().c_str());
    return false;
  }

  SaveState st;
  if (!rd.get_i32(st.area) || !rd.get_i32(st.x) || !rd.get_i32(st.y) ||
      !rd.get_i32(st.facing))
    return false;
  if (!rd.get_bytes(st.game_state.data(), st.game_state.size())) return false;

  std::uint16_t party_n;
  if (!rd.get_u16(party_n)) return false;
  st.party_records.resize(party_n);
  for (auto& rec : st.party_records)
    if (!rd.get_bytes(rec.data(), rec.size())) return false;

  out = std::move(st);
  return true;
}

}  // namespace dw::game
