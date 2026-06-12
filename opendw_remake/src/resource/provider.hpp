// provider — ResourceProvider 抽象(對照 docs/adr/0001)。
//
// 兩種來源:
//   Data1Provider   讀原始 DATA1/DATA2(萃取來源 + 對拍 oracle)
//   BundleProvider  讀 assets/bundle(remake 執行期用,**不需 DATA1**)
// 兩者對「未編輯資源」須回傳 byte-for-byte 相同的 bytes。
#pragma once
#include <cstdint>
#include <filesystem>
#include <optional>
#include <vector>
#include "archive.hpp"

namespace dw::res {

class ResourceProvider {
public:
  virtual ~ResourceProvider() = default;
  // 載入 resource id 的(解壓後)bytes。
  virtual std::optional<std::vector<std::uint8_t>> load(int id) const = 0;
};

// 讀原始 DATA1/DATA2(需 data1[/data2])。
class Data1Provider : public ResourceProvider {
public:
  static std::optional<Data1Provider> open(const std::filesystem::path& data_dir);
  std::optional<std::vector<std::uint8_t>> load(int id) const override;
private:
  explicit Data1Provider(Archive a) : arc_(std::move(a)) {}
  Archive arc_;
};

// 讀 assets/bundle(scripts/<id>.bin = 解壓後 bytecode/資源)。
class BundleProvider : public ResourceProvider {
public:
  explicit BundleProvider(std::filesystem::path bundle_dir) : dir_(std::move(bundle_dir)) {}
  std::optional<std::vector<std::uint8_t>> load(int id) const override;
private:
  std::filesystem::path dir_;
};

}  // namespace dw::res
