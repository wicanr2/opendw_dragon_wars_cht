// string_table — 從 asset bundle 載入某 section 的字串表(offset → 英文原文)。
//
// 執行期 VM 的字串輸出可改成「以 (section,offset) 查此表」取字,**不需 DATA1**;
// 再經 i18n::Strings 換中文。文字徹底脫離 DATA1(見 docs/adr/0001)。
#pragma once
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>

namespace dw::i18n {

class StringTable {
public:
  static std::optional<StringTable> load(const std::filesystem::path& tsv);
  // 查 offset 的英文原文;無則回空字串。\r/\n 已還原。
  std::string at(std::uint32_t offset) const;
  bool has(std::uint32_t offset) const { return map_.count(offset) != 0; }
  std::size_t size() const { return map_.size(); }

private:
  std::unordered_map<std::uint32_t, std::string> map_;
};

}  // namespace dw::i18n
