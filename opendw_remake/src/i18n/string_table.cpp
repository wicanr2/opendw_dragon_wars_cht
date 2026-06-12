#include "string_table.hpp"

#include <cstdio>
#include <cstdlib>

namespace dw::i18n {

static std::string unescape(const std::string& s) {
  std::string out;
  for (std::size_t i = 0; i < s.size(); ++i) {
    if (s[i] == '\\' && i + 1 < s.size()) {
      char n = s[++i];
      out.push_back(n == 'r' ? '\r' : n == 'n' ? '\n' : n);
    } else {
      out.push_back(s[i]);
    }
  }
  return out;
}

std::optional<StringTable> StringTable::load(const std::filesystem::path& tsv) {
  std::FILE* f = std::fopen(tsv.string().c_str(), "rb");
  if (!f) return std::nullopt;
  StringTable t;
  std::string line;
  int c;
  auto flush = [&] {
    if (!line.empty() && line[0] != '#') {
      auto tab = line.find('\t');
      if (tab != std::string::npos)
        t.map_[static_cast<std::uint32_t>(std::strtoul(line.substr(0, tab).c_str(), nullptr, 10))] =
            unescape(line.substr(tab + 1));
    }
    line.clear();
  };
  while ((c = std::fgetc(f)) != EOF) {
    if (c == '\n') flush();
    else if (c != '\r') line.push_back(static_cast<char>(c));
  }
  flush();
  std::fclose(f);
  return t;
}

std::string StringTable::at(std::uint32_t offset) const {
  auto it = map_.find(offset);
  return it != map_.end() ? it->second : std::string{};
}

}  // namespace dw::i18n
