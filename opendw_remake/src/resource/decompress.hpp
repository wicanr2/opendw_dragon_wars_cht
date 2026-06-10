// decompress — Dragon Wars 資源解壓(Huffman 樹字典,對照 opendw compress.c)。
//
// Deep module:對外只露「壓縮 bytes → 解壓 bytes」,內部隱藏建樹 + 逐 bit 走樹。
#pragma once
#include <cstdint>
#include <span>
#include <vector>

namespace dw::res {

// 輸入 = 壓縮 section 原始 bytes(開頭 2 bytes 為 LE 解壓後大小,其後為壓縮流)。
// 對照 resource.c:uncompressed_sz = get16le; decompress_data1(rest, sz)。
std::vector<std::uint8_t> decompress(std::span<const std::uint8_t> compressed);

}  // namespace dw::res
