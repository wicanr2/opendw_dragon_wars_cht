#include "text_codec.hpp"

namespace dw::text {
namespace {

// 5-bit 索引 → 字元(OR 0x80),對照 compress.c 的 alphabet[]。
constexpr std::uint8_t kAlphabet[] = {
    0xa0, 0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xeb, 0xec,
    0xed, 0xee, 0xef, 0xf0, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf9, 0xae,
    0xa2, 0xa7, 0xac, 0xa1, 0x8d, 0xea, 0xf1, 0xf8, 0xfa, 0xb0, 0xb1, 0xb2,
    0xb3, 0xb4, 0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0x30, 0x31, 0x32, 0x33, 0x34,
    0x35, 0x36, 0x37, 0x38, 0x39, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47,
    0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f, 0x50, 0x51, 0x52, 0x53,
    0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0xa8, 0xa9, 0xaf, 0xdc, 0xa3,
    0xaa, 0xbf, 0xbc, 0xbe, 0xba, 0xbb, 0xad, 0xa5};

// bit_extractor:對照 compress.c 的 struct bit_extractor + bit_extract()。
struct BitReader {
  std::span<const std::uint8_t> data;
  std::size_t offset;
  std::uint8_t num_bits = 0;
  std::uint8_t buffer = 0;
  std::uint8_t upper = 0;  // 大小寫切換位(0x1E escape)

  // 取 n 個 bit(MSB first,經 carry 累積),對照 bit_extract()。
  std::uint8_t bits(int n) {
    std::uint8_t al = 0;
    for (int i = 0; i < n; ++i) {
      if (num_bits == 0) {
        buffer = offset < data.size() ? data[offset] : 0;
        num_bits = 8;
        ++offset;
      }
      std::uint8_t tmp = buffer;
      buffer = static_cast<std::uint8_t>(buffer << 1);
      --num_bits;
      std::uint8_t carry = tmp > buffer ? 1 : 0;  // rcl 取進位
      al = static_cast<std::uint8_t>((al << 1) + carry);
    }
    return al;
  }

  // 取一個字母,對照 extract_letter()。回傳 0 = 字串結束。
  std::uint8_t letter() {
    for (;;) {
      std::uint8_t ret = bits(5);
      if (ret == 0) return 0;
      if (ret == 0x1E) {  // 大寫切換
        upper = static_cast<std::uint8_t>((upper >> 1) + 0x80);
        continue;
      }
      if (ret > 0x1E) {           // 擴充:再讀 6 bit
        ret = static_cast<std::uint8_t>(bits(6) + 0x1E);
      }
      if (ret == 0 || ret - 1 >= static_cast<int>(sizeof(kAlphabet)))
        return 0;
      std::uint8_t al = kAlphabet[ret - 1];
      upper = static_cast<std::uint8_t>(upper >> 1);
      if (upper >= 0x40 && al >= 0xE1 && al <= 0xFA)
        al = static_cast<std::uint8_t>(al & 0xDF);  // 轉大寫
      return al;
    }
  }
};

}  // namespace

// 對照 engine.c extract_string,含 0xAF/0xDC escape(變數代入)的忠實重現。
std::pair<std::string, std::size_t>
decode(std::span<const std::uint8_t> data, std::size_t byte_offset) {
  BitReader br{data, byte_offset};
  std::string out;
  for (;;) {
    std::uint8_t ret = br.letter();
    if (ret == 0) return {out, br.offset};
    ret &= 0x7F;  // 第一字 hotkey 機制淨效果:輸出 7-bit
    if (ret == (0xAF & 0x7F) || ret == (0xDC & 0x7F)) {
      std::uint8_t opener = ret;
      bool emit = (ret == (0xAF & 0x7F));
      for (;;) {
        ret = br.letter();
        if (ret == 0) return {out, br.offset};
        if (ret == opener) break;
        if (ret == (0xDC & 0x7F) || ret == (0xAF & 0x7F)) continue;
        if (emit) out.push_back(static_cast<char>(ret & 0x7F));
      }
    } else {
      out.push_back(static_cast<char>(ret & 0x7F));
    }
  }
}

std::vector<std::uint8_t> encode(std::string_view) {
  // R0 先不實作回寫;待 R3 patch 階段補(固定欄位/間接字串表策略)。
  return {};
}

}  // namespace dw::text
