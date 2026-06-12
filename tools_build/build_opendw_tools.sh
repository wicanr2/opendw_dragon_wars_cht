#!/bin/bash
# 重建 opendw 工具(sprite_dump/resextract/monster_info)於 docker。
# 自動套用 shim:DATA2 patch + 編譯錯誤驅動補 opcode stub/前向宣告 + ui.c 修正。
# 用法:在含 dwtools image 的機器上 bash tools_build/build_opendw_tools.sh <opendw路徑> <輸出dir>
set -e
OPENDW=${1:-/home/anr2/tmp/longcat/opendw}
OUT=${2:-/tmp/dwbuild/out}
SRC=/tmp/dwsrc
TB="$(cd "$(dirname "$0")" && pwd)"

rm -rf "$SRC"; cp -r "$OPENDW" "$SRC"
cp "$TB/sprite_dump.cpp" "$SRC/src/tools/sprite_dump.cpp"

python3 - "$SRC" <<'PY'
import re,sys,subprocess,os
SRC=sys.argv[1]; lib=f"{SRC}/src/lib"
eng=f"{lib}/engine.c"; ui=f"{lib}/ui.c"; res=f"{lib}/resource.c"

# --- ui.c:draw_right_pillar 重複定義 + line49 static 前向宣告 ---
s=open(ui).read()
s=s.replace('static void draw_right_pillar();\n','',1)  # 刪 static 前向宣告(ui.h 已宣告)
s=s.replace('''static void draw_right_pillar()
{
  draw_rect.x = 1;''','''static void draw_right_pillar_unused() __attribute__((unused));
static void draw_right_pillar_unused()
{
  draw_rect.x = 1;''',1)
# sprite_dump 需要的 exported 包裝
if 'render_encounter_sprite_only' not in s:
    s=s.replace('// 0x4D10\nvoid viewport_restore()',
      'void render_encounter_sprite_only(struct resource *r){ memset(viewport_memory,0x66,viewport_mem_sz); draw_random_encounter_graphic(viewport_memory,r); }\n\n// 0x4D10\nvoid viewport_restore()',1)
open(ui,'w').write(s)

# --- resource.c:DATA2 fallback(內嵌,等同 resource_data2.patch)---
s=open(res).read()
if 'header_rdr2' not in s:
    s=s.replace('static unsigned char data1_hdr[768];\nstatic struct buf_rdr *header_rdr = NULL;',
      'static unsigned char data1_hdr[768];\nstatic struct buf_rdr *header_rdr = NULL;\n'
      'static unsigned char data2_hdr[768];\nstatic struct buf_rdr *header_rdr2 = NULL;')
    load2='''static void load_data2_header(void){
  FILE *fp=fopen("data2","rb"); if(!fp) return;
  size_t n=fread(data2_hdr,1,sizeof(data2_hdr),fp); fclose(fp);
  if(n==sizeof(data2_hdr)) header_rdr2=buf_rdr_init(data2_hdr,sizeof(data2_hdr));
}

static int
load_data1_header(void)'''
    s=s.replace('static int\nload_data1_header(void)',load2,1)
    s=s.replace('  return load_data1_header();','  { int rc=load_data1_header(); load_data2_header(); return rc; }')
    old='''  buf_reset(header_rdr);
  for (i = 0; i < sec; i++) {
    uint16_t header_val = buf_get16le(header_rdr);
    if (header_val < 0xFF00) {
      offset += header_val;
    }
  }
  len = buf_get16le(header_rdr);'''
    new='''  buf_reset(header_rdr);
  uint16_t d1val=0xFFFF; for(i=0;i<=sec;i++){ d1val=buf_get16le(header_rdr); }
  const char *fname="data1"; struct buf_rdr *hdr=header_rdr;
  if(d1val>=0xFF00 && header_rdr2!=NULL){ fname="data2"; hdr=header_rdr2; }
  offset=sizeof(data1_hdr); buf_reset(hdr);
  for (i = 0; i < sec; i++) {
    uint16_t header_val = buf_get16le(hdr);
    if (header_val < 0xFF00) offset += header_val;
  }
  len = buf_get16le(hdr);'''
    s=s.replace(old,new).replace('fp = fopen("data1", "rb");\n  if (fp == NULL) {\n    fprintf(stderr, "Failed to open data1 file.\\n");\n    return NULL;\n  }',
      'fp = fopen(fname, "rb");\n  if (fp == NULL) { fprintf(stderr, "Failed to open %s\\n", fname); return NULL; }')
    open(res,'w').write(s)

# --- engine.c:補 opcode(已驗證的硬編 shim:stub 未定義者 + 前向宣告定義在後者 + sub_2AEE)---
s=open(eng).read()
marker='struct op_call_table targets[] = {'
assert s.count(marker)>=1, "engine.c marker 不存在"
if 'static void sub_2AEE' not in s:
    # targets[] 引用裸名 op_XX,但真實實作用不同名(opendw 半重構殘留)。
    # 有真實實作者→別名;opendw 未實作者(43/5F/60/63)→空 stub(無 oracle)。
    shim=('// build shim\n'
      'static void sub_2AEE(){}\n'
      '#define op_4C op_clc\n#define op_4D op_prng\n#define op_55 op_peek_and_pop\n'
      '#define op_5B op_5B_unused\n#define op_62 op_scan_for_char\n'
      'static void op_56(void); static void op_59(void); static void op_5C(void); static void op_72(void);\n'
      'static void op_43(void){} static void op_5F(void){} static void op_60(void){} static void op_63(void){}\n')
    s=s.replace(marker, shim+marker, 1)
    open(eng,'w').write(s)
print("shim 套用完成")
PY
echo "=== docker 編譯 ==="
mkdir -p "$OUT"
docker run --rm -v "$SRC":/app -v "$OUT":/out -w /app dwtools bash -c '
mkdir -p /tmp/obj
for f in src/lib/*.c; do gcc -c -O1 -w -Isrc/lib "$f" -o "/tmp/obj/$(basename ${f%.c}).o" 2>/dev/null; done
ar rcs /tmp/obj/libdragon.a /tmp/obj/*.o 2>/dev/null
for spec in "resextract:src/tools/resextract.cpp" "monster_info:src/tools/monster_info.cpp" "sprite_dump:src/tools/sprite_dump.cpp"; do
  t=${spec%%:*}; src=${spec#*:}
  g++ -O1 -w -std=c++14 -Isrc/lib "$src" /tmp/obj/libdragon.a -o /out/$t 2>/dev/null && echo "OK $t" || echo "FAIL $t"
done'
