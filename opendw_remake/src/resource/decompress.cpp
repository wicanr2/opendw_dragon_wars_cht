#include "decompress.hpp"

namespace dw::res {
namespace {

// 對照 buf_rdr(big-endian 16-bit / byte 讀取)。
struct Reader {
  std::span<const std::uint8_t> data;
  std::size_t offset = 0;
  std::uint8_t get8() { return offset < data.size() ? data[offset++] : 0; }
  std::uint16_t get16be() {
    std::uint16_t hi = get8();
    std::uint16_t lo = get8();
    return static_cast<std::uint16_t>((hi << 8) | lo);
  }
};

// 對照 struct compress_ctx。dict 為 2048-byte 樹緩衝(每節點 4 bytes)。
struct Ctx {
  int counter = 0;
  std::uint16_t dx = 0;
  int output_idx = 0;
  std::vector<std::uint8_t> dict = std::vector<std::uint8_t>(2048, 0);
  std::size_t dict_ptr = 0;  // 相對 dict_base 的位移(對照 dict_ptr - dict_base)
};

// 逐字對照 compress.c build_dictionary(),保留原 16-bit 運算與優先序。
void build_dictionary(Reader& in, Ctx& ctx) {
  std::uint16_t res;
  std::uint8_t al;
  std::uint16_t ax = 0;
  auto* d = ctx.dict.data();

  ctx.counter--;
  if (ctx.counter < 0) {
    ctx.dx = in.get16be();
    ctx.counter = 15;
  }

  res = static_cast<std::uint16_t>(ctx.dx << 1);
  if (res < ctx.dx) {  // shift overflow → 葉節點
    ctx.dx = res;
    if (ctx.counter == 0) {
      al = in.get8();
      ax = static_cast<std::uint16_t>(ax & 0xFF00);
      ax = static_cast<std::uint16_t>(ax + al);
    } else {
      if (ctx.counter >= 8) {
        ctx.counter -= 8;
      } else {
        al = in.get8();
        ax = static_cast<std::uint16_t>((al << 8 & 0xFF00) + (ctx.counter & 0xFF00 >> 8));
        ax = static_cast<std::uint16_t>(ax >> (ctx.counter & 0xFF));
        ctx.dx = static_cast<std::uint16_t>(ctx.dx | ax);
      }
      ax = static_cast<std::uint16_t>(ax & 0xFF00);
      ax = static_cast<std::uint16_t>(ax + ((ctx.dx & 0xFF00) >> 8));
      ctx.dx = static_cast<std::uint16_t>((ctx.dx & 0x00FF) << 8);
      ctx.dx = static_cast<std::uint16_t>(ctx.dx + ((ctx.counter & 0xFF00) >> 8));
    }
    ax = static_cast<std::uint16_t>(ax & 0x00FF);
    ax = static_cast<std::uint16_t>(ax + (ctx.counter & 0xFF00));
    d[ctx.dict_ptr + 2] = ax & 0xff;
    d[ctx.dict_ptr + 3] = (ax & 0xff00) >> 8;
    ax = static_cast<std::uint16_t>(ax & 0xFF00);
    ax = static_cast<std::uint16_t>(ax + ((ax & 0xFF00) >> 8));
    d[ctx.dict_ptr] = ax & 0xff;
    d[ctx.dict_ptr + 1] = (ax & 0xff00) >> 8;
    return;
  } else {  // 內部節點 → 遞迴建左右子樹
    std::size_t save_offset;
    ctx.dx = res;
    ctx.output_idx += 4;
    d[ctx.dict_ptr] = ctx.output_idx & 0xff;
    d[ctx.dict_ptr + 1] = (ctx.output_idx & 0xff00) >> 8;
    save_offset = ctx.dict_ptr;
    ctx.dict_ptr = static_cast<std::size_t>(ctx.output_idx);
    build_dictionary(in, ctx);
    ctx.dict_ptr = save_offset;
    ctx.output_idx += 4;
    d[ctx.dict_ptr + 2] = ctx.output_idx & 0xff;
    d[ctx.dict_ptr + 3] = (ctx.output_idx & 0xff00) >> 8;
    save_offset = ctx.dict_ptr;
    ctx.dict_ptr = static_cast<std::size_t>(ctx.output_idx);
    build_dictionary(in, ctx);
    ctx.dict_ptr = save_offset;
  }
}

// 對照 compress.c decompress():從根(bx=0)逐 bit 走樹,遇葉輸出。
void decompress_tree(Reader& in, Ctx& ctx, int size, std::vector<std::uint8_t>& out) {
  std::uint16_t res, ax;
  int bx = 0;
  int bp = ctx.counter;
  const auto* d = ctx.dict.data();
  ctx.counter = size;
  while (ctx.counter > 0) {
    const std::uint8_t* ptr = d + bx;
    ax = static_cast<std::uint16_t>(ptr[0] + (ptr[1] << 8));
    if (ax != 0) {  // 內部節點
      bp--;
      if (bp < 0) {
        ctx.dx = in.get16be();
        bp = 15;
      }
      res = static_cast<std::uint16_t>(ctx.dx << 1);
      if (res < ctx.dx) {  // carry → 右
        ctx.dx = res;
        bx = ptr[2] + (ptr[3] << 8);
      } else {  // no carry → 左
        ctx.dx = res;
        bx = ax;
      }
    } else {  // 葉節點 → 輸出
      out.push_back(ptr[2]);
      ctx.counter--;
      bx = 0;
    }
  }
}

}  // namespace

std::vector<std::uint8_t> decompress(std::span<const std::uint8_t> compressed) {
  Reader in{compressed, 0};
  std::uint16_t lo = in.get8();
  std::uint16_t hi = in.get8();
  int size = lo | (hi << 8);  // 開頭 2 bytes LE = 解壓後大小
  Ctx ctx;
  build_dictionary(in, ctx);
  std::vector<std::uint8_t> out;
  out.reserve(static_cast<std::size_t>(size));
  decompress_tree(in, ctx, size, out);
  return out;
}

}  // namespace dw::res
