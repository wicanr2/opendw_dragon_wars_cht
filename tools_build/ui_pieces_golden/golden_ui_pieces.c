/* golden_ui_pieces.c — 產生遊戲內 UI chrome 的 oracle golden PPM。
 *
 * 獨立於 remake:直接讀 DRAGON.COM(v1.1),用 opendw src/lib/ui.c 的 draw_ui_piece()@546
 * 解碼 + ui_draw()@744 / draw_right_pillar()@720 / ui_header_draw()@762 的繪製序列,
 * 渲染 320×200 framebuffer → 輸出 P6 PPM(DOS 16 色)。
 *
 * 這是「真值」:ui_pieces 偏移表 @ com 0x6AE0(43 筆 LE u16),各片 4-byte header
 * (w,h,offset_delta,y_pos)+ w*h nibble bitmap(byte=2px,hi=左 lo=右)。
 *
 * verify_ui_pieces_golden 用 remake 的 UiPieces::draw_chrome 重建同一畫面對拍本檔輸出。
 *
 * 用法:golden_ui_pieces <dragon.com> <out.ppm>
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#define COM_ORG 0x100
#define TABLE_COM 0x6AE0
#define COUNT 0x2B            /* UI_PIECE_COUNT */
#define RIGHT_PILLAR 9        /* UI_RIGHT_PILLAR */
#define BRICK_FIRST 0x17      /* UI_BRICK_FIRST_PICTURE */
#define FBW 320
#define FBH 200

/* DOS 16 色(與 framebuffer.hpp kDosPalette / sprite_dump 一致)。 */
static const unsigned char DOS[16][3] = {
    {0,0,0},{0,0,0xAA},{0,0xAA,0},{0,0xAA,0xAA},
    {0xAA,0,0},{0xAA,0,0xAA},{0xAA,0x55,0},{0xAA,0xAA,0xAA},
    {0x55,0x55,0x55},{0x55,0x55,0xFF},{0x55,0xFF,0x55},{0x55,0xFF,0xFF},
    {0xFF,0x55,0x55},{0xFF,0x55,0xFF},{0xFF,0xFF,0x55},{0xFF,0xFF,0xFF}};

static unsigned char *com;
static long com_len;
static unsigned char fb[FBH][FBW];

/* 取得 com 影像中 [com_off−0x100, +sz);回傳指標或 NULL。 */
static const unsigned char *com_slice(size_t com_off, size_t sz) {
  if (com_off < COM_ORG) return NULL;
  size_t fo = com_off - COM_ORG;
  if ((long)(fo + sz) > com_len) return NULL;
  return com + fo;
}

/* 忠實 port draw_ui_piece(ui.c:546):x=dx*4, y=y_pos, byte=2px(hi 左, lo 右)。 */
static void draw_piece(int w, int h, int dx, int y, const unsigned char *data) {
  int x0 = dx * 4;
  const unsigned char *src = data;
  for (int row = 0; row < h; ++row) {
    int py = y + row;
    int px = x0;
    for (int col = 0; col < w; ++col) {
      unsigned char al = *src++;
      int hi = (al >> 4) & 0xF, lo = al & 0xF;
      if (px >= 0 && px < FBW && py >= 0 && py < FBH) fb[py][px] = hi;
      ++px;
      if (px >= 0 && px < FBW && py >= 0 && py < FBH) fb[py][px] = lo;
      ++px;
    }
  }
}

int main(int argc, char **argv) {
  if (argc < 3) { fprintf(stderr, "usage: %s <dragon.com> <out.ppm>\n", argv[0]); return 2; }
  FILE *f = fopen(argv[1], "rb");
  if (!f) { fprintf(stderr, "cannot open %s\n", argv[1]); return 1; }
  fseek(f, 0, SEEK_END); com_len = ftell(f); fseek(f, 0, SEEK_SET);
  com = malloc(com_len);
  if (fread(com, 1, com_len, f) != (size_t)com_len) { fprintf(stderr, "read fail\n"); return 1; }
  fclose(f);

  const unsigned char *tbl = com_slice(TABLE_COM, COUNT * 2);
  if (!tbl) { fprintf(stderr, "offset table OOB\n"); return 1; }
  uint16_t offs[COUNT];
  for (int i = 0; i < COUNT; ++i) offs[i] = tbl[i*2] | (tbl[i*2+1] << 8);

  /* 版本校驗:v1.1 第一筆偏移 == 表尾。 */
  if (offs[0] != (TABLE_COM + COUNT * 2)) {
    fprintf(stderr, "offset[0]=0x%04X != table_end 0x%04X (wrong DRAGON.COM version?)\n",
            offs[0], TABLE_COM + COUNT * 2);
    return 1;
  }

  /* 載入各片 header + data。 */
  struct { int w, h, dx, y; const unsigned char *data; } pc[COUNT];
  for (int i = 0; i < COUNT; ++i) {
    const unsigned char *hdr = com_slice(offs[i], 4);
    if (!hdr) { fprintf(stderr, "piece %d header OOB\n", i); return 1; }
    pc[i].w = hdr[0]; pc[i].h = hdr[1]; pc[i].dx = hdr[2]; pc[i].y = hdr[3];
    size_t dl = (size_t)pc[i].w * pc[i].h;
    pc[i].data = dl ? com_slice(offs[i] + 4, dl) : NULL;
    if (dl && !pc[i].data) { fprintf(stderr, "piece %d data OOB\n", i); return 1; }
  }

  memset(fb, 0, sizeof(fb));
  /* ui_draw() 靜態邊框 pieces 0..8(含 logo piece 5)。 */
  for (int i = 0; i < 9; ++i)
    if (pc[i].data) draw_piece(pc[i].w, pc[i].h, pc[i].dx, pc[i].y, pc[i].data);
  /* 右 pillar(piece 9)。 */
  if (pc[RIGHT_PILLAR].data)
    draw_piece(pc[RIGHT_PILLAR].w, pc[RIGHT_PILLAR].h, pc[RIGHT_PILLAR].dx,
               pc[RIGHT_PILLAR].y, pc[RIGHT_PILLAR].data);
  /* 頂端石磚橫條(ui_header_draw:i ∈ [4,0x14) → pieces[0x17+i])。 */
  for (int i = 4; i < 0x14; ++i) {
    int idx = i + BRICK_FIRST;
    if (pc[idx].data) draw_piece(pc[idx].w, pc[idx].h, pc[idx].dx, pc[idx].y, pc[idx].data);
  }

  /* 輸出 P6 PPM。 */
  FILE *o = fopen(argv[2], "wb");
  if (!o) { fprintf(stderr, "cannot write %s\n", argv[2]); return 1; }
  fprintf(o, "P6\n%d %d\n255\n", FBW, FBH);
  for (int y = 0; y < FBH; ++y)
    for (int x = 0; x < FBW; ++x) {
      unsigned char c = fb[y][x] & 0xF;
      fputc(DOS[c][0], o); fputc(DOS[c][1], o); fputc(DOS[c][2], o);
    }
  fclose(o);
  fprintf(stderr, "wrote %s (320x200)\n", argv[2]);
  free(com);
  return 0;
}
