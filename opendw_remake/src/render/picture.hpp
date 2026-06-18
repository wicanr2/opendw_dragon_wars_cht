// picture — 全螢幕圖解碼進 framebuffer。兩種來源格式:
//
//  1) DOS(res 24–29,解壓後 32000B):title_adjust(垂直 XOR delta 去交錯)+
//     title_build(nibble→像素)。對照 opendw main.c。→ decode_fullscreen()。
//  2) Amiga(themes/amiga/*.pic,解壓後 35996B = 32B palette + 32000B planar):
//     [16 big-endian 0x0RGB words][320×200 4 bitplane,plane 連續(非 Amiga 交錯)]。
//     見 tools_build/amiga_pic_extract.py(可重現解碼)。→ decode_amiga_planar()。
#pragma once
#include <array>
#include <cstdint>
#include <span>
#include "framebuffer.hpp"

namespace dw::render {

// DOS:data = 解壓後的全螢幕圖 bytes(預期 32000)。先做 title_adjust 還原再填 framebuffer。
void decode_fullscreen(Framebuffer& fb, std::span<const std::uint8_t> data);

// Amiga:讀 .pic 開頭 32B 的 16 色 palette(big-endian 0x0RGB words,每 nibble ×17 → 8-bit)。
//   data 須至少 32B;不足時缺項回退黑。回傳 16×Rgb 供 SdlVideo::set_palette()。
std::array<Rgb, 16> read_amiga_palette(std::span<const std::uint8_t> data);

// Amiga:把 .pic(32B palette + 4-plane planar)的像素層解碼進 framebuffer(indexed 0–15)。
//   不套色:framebuffer 只存索引,顯示時由呼叫端用 read_amiga_palette() 取得的 palette
//   交給 SdlVideo。data 不足時缺像素回退 0。
void decode_amiga_planar(Framebuffer& fb, std::span<const std::uint8_t> data);

}  // namespace dw::render
