// i18n/strings — 遊戲文字在地化查表。
//
// 以英文原文為鍵(由 text_codec 解出的字串),回傳繁中譯文。譯表載自 TSV
// (english<TAB>chinese),源自 docs/15_TRANSLATION_DRAFT.md / CONTEXT.md。
#pragma once
#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>

namespace dw::i18n {

class Strings {
public:
  static std::optional<Strings> load(const std::filesystem::path& tsv);
  // 查譯文;無對應則回傳原文(回退英文)。
  std::string tr(const std::string& english) const;
  std::size_t size() const { return map_.size(); }

private:
  std::unordered_map<std::string, std::string> map_;
};

}  // namespace dw::i18n
