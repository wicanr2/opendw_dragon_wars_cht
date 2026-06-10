// archive — 讀取原始 DATA1/DATA2 資源檔。
//
// Deep module:對外只露「給我 resource N 的 bytes」,內部隱藏 768-byte header、
// section offset 累加、DATA1↔DATA2 fallback、壓縮判定。對照 opendw 的 resource.c,
// 並內建本專案發現的 DATA2 修正(DATA1 header[N] >= 0xFF00 時改讀 DATA2)。
#pragma once
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <vector>

namespace dw::res {

using ResourceId = int;

class Archive {
public:
  // dir 內需有 data1(必要)與 data2(可選)。
  static std::optional<Archive> open(const std::filesystem::path& dir);

  // 載入 resource id 的(必要時解壓後)bytes。section > 0x17 視為壓縮。
  std::optional<std::vector<std::uint8_t>> load(ResourceId id) const;

  // 不解壓,回傳原始 section bytes(round-trip 驗證用)。
  std::optional<std::vector<std::uint8_t>> load_raw(ResourceId id) const;

private:
  Archive() = default;
  struct Header {                       // 768 bytes = 384 個 uint16 section 大小
    std::array<std::uint16_t, 384> sizes{};
  };
  // 回傳 (來源檔資料, section 在該檔的 byte offset, section 大小)。
  struct Located { const std::vector<std::uint8_t>* file; std::size_t offset; std::uint16_t size; };
  std::optional<Located> locate(ResourceId id) const;

  std::vector<std::uint8_t> data1_;
  std::vector<std::uint8_t> data2_;     // 可能為空
  Header hdr1_{}, hdr2_{};
  bool has_data2_ = false;
};

}  // namespace dw::res
