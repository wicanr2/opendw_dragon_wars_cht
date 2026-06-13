#include "strings.hpp"

#include <cstdio>
#include <string>

namespace dw::i18n {

std::optional<Strings> Strings::load(const std::filesystem::path& tsv) {
  std::FILE* f = std::fopen(tsv.string().c_str(), "rb");
  if (!f) return std::nullopt;
  Strings s;
  std::string line;
  int c;
  auto flush = [&] {
    if (line.empty() || line[0] == '#') { line.clear(); return; }
    auto tab = line.find('\t');
    if (tab != std::string::npos)
      s.map_[line.substr(0, tab)] = line.substr(tab + 1);
    line.clear();
  };
  while ((c = std::fgetc(f)) != EOF) {
    if (c == '\n') flush();
    else if (c != '\r') line.push_back(static_cast<char>(c));
  }
  flush();
  std::fclose(f);
  return s;
}

bool Strings::merge(const std::filesystem::path& tsv) {
  auto other = load(tsv);
  if (!other) return false;
  for (auto& [k, v] : other->map_) map_[k] = v;  // 新檔覆寫
  return true;
}

std::string Strings::tr(const std::string& english) const {
  auto it = map_.find(english);
  return it != map_.end() ? it->second : english;  // 回退英文
}

}  // namespace dw::i18n
