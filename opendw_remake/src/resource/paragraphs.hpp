// resource/paragraphs — 「Read paragraph N」段落書(防拷手冊)查表。
//
// 自包含資產:assets/bundle/paragraphs/<locale>/paragraphs.tsv(由
// tools/extract/build_paragraph_bundle.py 從 docs/34_READ_PARAGRAPHS.md 產出)。
// 每行 `N<TAB>繁中全文`,段落內換行以字面 \n 轉義(載入時還原)。
//
// 防拷流程:VM 事件 script 走 op_78(emit「Read paragraph 」)+ op_81(印段落號 N);
// 引擎攔到 N → ParagraphBook::text(N) 取繁中原文顯示(找不到回退「Read paragraph N」)。
// locale 可擴(ja / en 各放一個子目錄即可,隨 --locale 選用)。
#pragma once
#include <optional>
#include <string>
#include <unordered_map>

namespace dw::res {

class ParagraphBook {
public:
  // 從 bundle 段落目錄載指定 locale 的段落書。
  // bundle_paragraphs_dir 例:assets/bundle/paragraphs;locale 例:zh-TW。
  static std::optional<ParagraphBook> load(const std::string& bundle_paragraphs_dir,
                                           const std::string& locale);

  // 取段落 N 的全文(段落內換行已還原為 '\n');無此段回 nullopt。
  std::optional<std::string> text(int n) const;
  std::size_t size() const { return map_.size(); }

private:
  std::unordered_map<int, std::string> map_;
};

}  // namespace dw::res
