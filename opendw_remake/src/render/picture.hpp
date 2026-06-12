// picture — 全螢幕圖(res 24–29,解壓後 32000B)解碼進 framebuffer。
//
// 對照 opendw main.c title_adjust(垂直 XOR delta 去交錯)+ title_build(nibble→像素)。
#pragma once
#include <cstdint>
#include <span>
#include "framebuffer.hpp"

namespace dw::render {

// data = 解壓後的全螢幕圖 bytes(預期 32000)。先做 title_adjust 還原再填 framebuffer。
void decode_fullscreen(Framebuffer& fb, std::span<const std::uint8_t> data);

}  // namespace dw::render
