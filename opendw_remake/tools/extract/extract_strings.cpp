// extract_strings — 把某 script section 的內嵌字串萃進 bundle 字串表。
// 掃 op_77/78/7B,在其後 byte 對齊處用 text_codec 解碼,輸出 "offset<TAB>string"。
// 用法: extract_strings <data_dir> <section_id> <out.tsv>
#include <cstdio>
#include <string>
#include "../../src/resource/archive.hpp"
#include "../../src/resource/text_codec.hpp"
using namespace dw;
int main(int argc,char**argv){
  if(argc<4){std::fprintf(stderr,"usage: %s <data_dir> <section> <out.tsv>\n",argv[0]);return 2;}
  auto arc=res::Archive::open(argv[1]);
  if(!arc){std::fprintf(stderr,"open failed\n");return 1;}
  int sid=std::atoi(argv[2]);
  auto sec=arc->load(sid);
  if(!sec){std::fprintf(stderr,"load section %d failed\n",sid);return 1;}
  std::FILE*f=std::fopen(argv[3],"wb");
  std::fprintf(f,"# section %d 內嵌字串(offset<TAB>english);由 op_77/78/7B 解出\n",sid);
  int n=0;
  for(std::size_t p=0;p+1<sec->size();++p){
    std::uint8_t op=(*sec)[p];
    if(op!=0x77&&op!=0x78&&op!=0x7B) continue;
    auto[txt,next]=text::decode(*sec,p+1); (void)next;
    int printable=0; for(char c:txt) if(c>=32&&c<127) ++printable;
    if(txt.size()>=4 && printable*100/(int)txt.size()>85){
      // 跳脫 \r/\n/\t 與控制字元為可見記號(供 TSV 單行儲存)
      std::string esc; for(char c:txt){ if(c=='\r')esc+="\\r"; else if(c=='\n')esc+="\\n"; else if(c=='\t')esc+=' '; else if((unsigned char)c<32)continue; else esc+=c; }
      std::fprintf(f,"%zu\t%s\n",p+1,esc.c_str()); ++n;
    }
  }
  std::fclose(f);
  std::fprintf(stderr,"section %d: %d strings -> %s\n",sid,n,argv[3]);
  return 0;
}
