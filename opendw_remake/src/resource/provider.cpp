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
