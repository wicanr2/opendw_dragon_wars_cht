#include "paragraphs.hpp"
#include <cstdio>
#include <string>
#include <vector>

namespace dw::res {

// 還原 build_paragraph_bundle.py 的轉義:字面 "\\n" → 換行、"\\\\" → '\'。
static std::string unescape(const std::string& s) {
  std::string out;
  out.reserve(s.size());
  for (std::size_t i = 0; i < s.size(); ++i) {
    if (s[i] == '\\' && i + 1 < s.size()) {
      char c = s[i + 1];
      if (c == 'n') { out.push_back('\n'); ++i; continue; }
      if (c == '\\') { out.push_back('\\'); ++i; continue; }
    }
    out.push_back(s[i]);
  }
  return out;
}

std::optional<ParagraphBook> ParagraphBook::load(const std::string& dir,
                                                 const std::string& locale) {
  std::string path = dir + "/" + locale + "/paragraphs.tsv";
  std::FILE* f = std::fopen(path.c_str(), "rb");
  if (!f) return std::nullopt;
  ParagraphBook book;
  std::string line;
  int c;
  auto flush = [&]() {
    if (line.empty()) return;
    auto tab = line.find('\t');
    if (tab != std::string::npos) {
      int n = std::atoi(line.substr(0, tab).c_str());
      if (n > 0) book.map_[n] = unescape(line.substr(tab + 1));
    }
    line.clear();
  };
  while ((c = std::fgetc(f)) != EOF) {
    if (c == '\n') { flush(); }
    else if (c != '\r') line.push_back((char)c);
  }
  flush();
  std::fclose(f);
  if (book.map_.empty()) return std::nullopt;
  return book;
}

std::optional<std::string> ParagraphBook::text(int n) const {
  auto it = map_.find(n);
  if (it == map_.end()) return std::nullopt;
  return it->second;
}

}  // namespace dw::res
