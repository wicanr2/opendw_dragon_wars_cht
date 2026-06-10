// text_codec — Dragon Wars 5-bit 壓縮字串編解碼。
//
// Deep module:對外只露 decode/encode,內部隱藏 5-bit 取位、0x1E 大小寫切換、
// 0xAF/0xDC escape(變數代入)等複雜度。對照 opendw 的 compress.c(bit_extract /
// extract_letter)與 engine.c(extract_string)。
#pragma once
#include <cstdint>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace dw::text {

// 從 data 的 byte_offset 處解一條字串。
// 回傳 { utf8 內容, 下一條字串的起始 byte offset }。
// next_offset 來自原版 bit_extractor 的 offset(= 下一條字串起點),用於連續解碼。
std::pair<std::string, std::size_t>
decode(std::span<const std::uint8_t> data, std::size_t byte_offset);

// 將文字反向編成 5-bit 格式(回寫 patch 用;ASCII 子集,中文另循 Big5 路徑)。
std::vector<std::uint8_t> encode(std::string_view ascii);

}  // namespace dw::text
