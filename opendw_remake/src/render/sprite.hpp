// sprite — 從 asset bundle 載入 sprite(.spr indexed 格式),畫進 framebuffer。
//
// 這是 ResourceProvider/BundleProvider 路線:sprite 走 bundle 檔案,執行期**不依賴 DATA1**。
// 換 sprite = 換 .spr/.png 檔(見 docs/adr/0001)。.spr 自帶調色盤(DOS 16 色;
// 未來 X68000/PC-9801 高彩美術用自有調色盤,由 remaster 渲染模式處理)。
#pragma once
#include <cstdint>
#include <filesystem>
#include <optional>
#include <vector>
#include "framebuffer.hpp"

namespace dw::render {

struct Sprite {
  std::uint16_t w = 0, h = 0;
  std::vector<Rgb> palette;          // 自帶調色盤
  std::vector<std::uint8_t> idx;     // w*h 個調色盤索引

  // 載入 .spr(magic DWSP)。
  static std::optional<Sprite> load(const std::filesystem::path& spr);

  // 畫進 framebuffer(像素 px,py)。transparent_index <0 表示不透明全畫;
  // 否則該索引視為透明不畫(預設 6 = encounter 棕色背景)。
  void blit(Framebuffer& fb, int px, int py, int transparent_index = -1) const;
};

}  // namespace dw::render
