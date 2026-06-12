// verify_provider — BundleProvider 載入的資源,與 Data1Provider(原始 DATA1)對拍 byte-for-byte。
// 用法: verify_provider <data_dir> <bundle_dir> <id...>
#include <cstdio>
#include "../../src/resource/provider.hpp"
using namespace dw::res;
int main(int argc,char**argv){
  if(argc<4){std::fprintf(stderr,"usage: %s <data_dir> <bundle_dir> <id...>\n",argv[0]);return 2;}
  auto d1=Data1Provider::open(argv[1]);
  if(!d1){std::fprintf(stderr,"open data1 failed\n");return 1;}
  BundleProvider bun(argv[2]);
  int fail=0;
  for(int i=3;i<argc;++i){
    int id=std::atoi(argv[i]);
    auto a=d1->load(id); auto b=bun.load(id);
    if(!a){std::printf("  id %d: DATA1 載入失敗\n",id);++fail;continue;}
    if(!b){std::printf("  id %d: bundle 缺檔\n",id);++fail;continue;}
    if(*a==*b) std::printf("  id %d: ✅ bundle == DATA1 (%zu bytes)\n",id,b->size());
    else {std::printf("  id %d: ❌ 不一致 (DATA1 %zu / bundle %zu)\n",id,a->size(),b->size());++fail;}
  }
  std::printf(fail?"\n%d 項不一致\n":"\n全部一致 — BundleProvider 可取代 DATA1(未編輯資源)\n",fail);
  return fail?1:0;
}
