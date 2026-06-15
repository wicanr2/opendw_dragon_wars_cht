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
  // VM emit 的事件字串常帶內嵌 '\r'(換行/段落分隔),但 TSV 載入時(load())
  // 已把每行的 '\r' 全濾掉,故鍵不含 '\r'。查詢前同樣濾掉 '\r' 使兩側一致,
  // 否則帶前導/內嵌 '\r' 的 emit(如 "\rA breeze crawls...")永遠查不到鍵 →
  // 靜默回退英文(舊 bug:部分 events.tsv 鍵在實機顯英文)。
  if (english.find('\r') == std::string::npos) {
    auto it = map_.find(english);
    return it != map_.end() ? it->second : english;
  }
  std::string norm;
  norm.reserve(english.size());
  for (char c : english)
    if (c != '\r') norm.push_back(c);
  auto it = map_.find(norm);
  return it != map_.end() ? it->second : english;  // 回退原文(保留原 \r)
}

}  // namespace dw::i18n
