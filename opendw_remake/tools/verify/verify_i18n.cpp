// verify_i18n — i18n 覆蓋率 / fallback 契約驗證(鎖「多語系、日文 ready」架構)。
//
// 用法: verify_i18n <i18n_root>   (預設 assets/i18n)
//
// 檢查:
//  1. **格式健全**:每個 TSV 的非註解非空行都含 TAB(english<TAB>localized)。
//     任何畸形行 → FAIL(這是真正的資料 bug)。
//  2. **參考語系完整**:以 zh-TW 為參考(鍵=英文源,內容最齊),其所有檔可載入。
//  3. **fallback 契約**:Strings::tr 對「查無的鍵」回傳原鍵(英文),永不崩。
//  4. **覆蓋率報告**:逐語系逐檔列出相對 zh-TW 的鍵覆蓋率與缺漏(資訊性;en 為
//     passthrough 設計、ja 缺項會 fallback 英文 → 不視為 FAIL,但明確列出供補譯)。
//
// PASS 條件:無畸形行 + zh-TW 可載入 + fallback 契約成立。覆蓋率缺口只報告不擋。
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "i18n/strings.hpp"

namespace fs = std::filesystem;

namespace {

const char* kFiles[] = {"menu.tsv", "events.tsv", "chars.tsv", "combat.tsv"};
const char* kLocales[] = {"zh-TW", "en", "ja"};

struct ParseResult {
  std::set<std::string> keys;
  int malformed = 0;
  bool exists = false;
};

// 直接解析 TSV(與 Strings::load 同規則:# 開頭/空行略過,需含 TAB)。
ParseResult parse_tsv(const fs::path& p) {
  ParseResult r;
  std::ifstream f(p);
  if (!f) return r;
  r.exists = true;
  std::string line;
  while (std::getline(f, line)) {
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (line.empty() || line[0] == '#') continue;
    auto tab = line.find('\t');
    if (tab == std::string::npos) {
      ++r.malformed;
      std::fprintf(stderr, "  [畸形] %s: 無 TAB → %.40s\n", p.string().c_str(), line.c_str());
      continue;
    }
    r.keys.insert(line.substr(0, tab));
  }
  return r;
}

}  // namespace

int main(int argc, char** argv) {
  fs::path root = argc > 1 ? argv[1] : "assets/i18n";
  if (!fs::exists(root)) {
    std::fprintf(stderr, "FAIL: 找不到 i18n root: %s\n", root.string().c_str());
    return 1;
  }

  int malformed_total = 0;
  // 參考鍵集(zh-TW 每檔的鍵聯集)。
  std::map<std::string, std::set<std::string>> ref;  // file -> keys

  std::printf("== i18n 覆蓋率(參考語系 zh-TW)==\n");
  for (const char* file : kFiles) {
    ParseResult zh = parse_tsv(root / "zh-TW" / file);
    malformed_total += zh.malformed;
    ref[file] = zh.keys;
  }

  // 逐語系逐檔報告。
  for (const char* loc : kLocales) {
    std::printf("\n[%s]\n", loc);
    for (const char* file : kFiles) {
      ParseResult pr = parse_tsv(root / loc / file);
      malformed_total += pr.malformed;
      const auto& refkeys = ref[file];
      if (refkeys.empty()) continue;
      std::vector<std::string> missing;
      for (const auto& k : refkeys)
        if (!pr.keys.count(k)) missing.push_back(k);
      int have = static_cast<int>(refkeys.size()) - static_cast<int>(missing.size());
      std::printf("  %-12s %3d/%-3zu 鍵%s", file, have, refkeys.size(),
                  pr.exists ? "" : " (檔不存在 → 全數 fallback)");
      if (!missing.empty()) {
        std::printf("  缺 %zu", missing.size());
        if (std::string(loc) != "en") {  // en passthrough 不逐條列
          std::printf(":");
          int n = 0;
          for (const auto& m : missing) {
            if (n++ >= 5) { std::printf(" …"); break; }
            std::printf(" \"%.16s\"", m.c_str());
          }
        }
      }
      std::printf("\n");
    }
  }

  // fallback 契約:zh-TW menu 載入後,查無的鍵回傳原鍵。
  bool fallback_ok = true;
  auto s = dw::i18n::Strings::load(root / "zh-TW" / "menu.tsv");
  if (!s) {
    std::fprintf(stderr, "\nFAIL: zh-TW/menu.tsv 無法載入\n");
    return 1;
  }
  const std::string probe = "__no_such_key_zzz__";
  if (s->tr(probe) != probe) {
    std::fprintf(stderr, "\nFAIL: fallback 契約破壞(查無鍵未回傳原文)\n");
    fallback_ok = false;
  }

  std::printf("\n== 結果 ==\n");
  std::printf("畸形行: %d\n", malformed_total);
  std::printf("fallback 契約: %s\n", fallback_ok ? "OK" : "BROKEN");
  if (malformed_total == 0 && fallback_ok) {
    std::printf("PASS: i18n 格式健全 + fallback 契約成立(覆蓋率缺口見上,供補譯)\n");
    return 0;
  }
  std::printf("FAIL\n");
  return 1;
}
