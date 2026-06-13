// verify_areaswitch — 地圖換場/同區傳送的確定性 PASS/FAIL 驗證。
//
// 對拍 opendw 機制(逆向證據見 docs,probe_areaswitch):
//   事件腳本(op_71→run_level_script→run_script)用 op_12/op_11 寫 gs[0]=入口X、
//   gs[1]=入口Y、gs[2]=新 area、gs[3]=朝向;opendw refresh_viewport→load_level_resources
//   每幀比對 gs[2] vs gs[0x57],變了就 resource_load(area+0x46) 重載。
//   邊界 wrap(flag&2)與兩張已載入地圖互換在 opendw 為 exit(1) 未實作 → 此處明確跳過。
//
// 本工具獨立重現 main.cpp 的 enter_map + run_event + sync_relocation 流程(無 SDL),
// 從已知會觸發的格子起步,斷言 area/x/y/facing 變成預期值,逐案 PASS/FAIL。
//
// 用法:verify_areaswitch <bundle_dir>
#include <cstdio>
#include <cstdlib>
#include <optional>
#include <string>
#include <vector>
#include "../../src/resource/level.hpp"
#include "../../src/resource/provider.hpp"
#include "../../src/vm/interpreter.hpp"

using namespace dw;

namespace {

// 重現 main.cpp 的換場狀態機(最小可驗證版本)。
struct World {
  std::string bundle;
  res::BundleProvider prov;
  std::optional<res::Level> level;
  int current_area = -1, level_res = -1;
  int px = 0, py = 0, dir = 1;
  std::array<std::uint8_t, 256> gs{};

  explicit World(std::string b) : bundle(std::move(b)), prov(bundle) {}

  bool enter_map(int area) {
    level = res::Level::load_file(bundle + "/maps/" + std::to_string(area) + ".lvl");
    if (!level) return false;
    level_res = area + 0x46;
    current_area = area;
    px = py = 0; dir = 1;
    for (int y = 0; y < level->h && py == 0 && px == 0; ++y)
      for (int x = 0; x < level->w; ++x)
        if (level->tile(x, y) == 1) { px = x; py = y; y = level->h; break; }
    gs[0] = (std::uint8_t)px; gs[1] = (std::uint8_t)py;
    gs[2] = (std::uint8_t)area; gs[3] = (std::uint8_t)dir;
    return true;
  }

  // 跑事件腳本(對拍 main.cpp run_event):seed 位置 + 跨資源 provider(bundle + level-self)。
  std::string run_event(std::uint8_t tv) {
    if (!level) return "";
    std::uint16_t pc = level->script_pc(tv);
    if (pc == 0 || pc >= level->data().size()) return "";
    vm::VmState st;
    st.script = level->data();
    st.data_bytes = level->data();
    st.script_res = level_res;
    st.data_res = level_res;
    st.pc = pc;
    st.game_state = gs;
    st.game_state[0] = (std::uint8_t)px; st.game_state[1] = (std::uint8_t)py;
    st.game_state[2] = (std::uint8_t)current_area; st.game_state[3] = (std::uint8_t)dir;
    st.resource_provider = [&](int tag) -> std::optional<std::vector<std::uint8_t>> {
      if (tag == level_res) return level->data();
      return prov.load(tag);
    };
    vm::Interpreter ip(st);
    std::string out;
    ip.set_message_sink([&](std::size_t, const std::string& s) {
      if (!out.empty()) out += ' ';
      out += s;
    });
    ip.run();
    gs = st.game_state;
    return out;
  }

  // 對拍 load_level_resources 的 poll(同 main.cpp sync_relocation)。
  // 回傳:0=無變化、1=同區傳送、2=換 area、-1=wrap 跳過。
  int sync_relocation() {
    int new_area = gs[2];
    int gx = gs[0], gy = gs[1], gf = gs[3] & 3;
    if (new_area == current_area) {
      if (gx != px || gy != py || gf != dir) { px = gx; py = gy; dir = gf; return 1; }
      return 0;
    }
    auto dst = res::Level::load_file(bundle + "/maps/" + std::to_string(new_area) + ".lvl");
    if (!dst) { gs[2] = (std::uint8_t)current_area; return -1; }
    if (dst->flags & 0x2) { gs[2] = (std::uint8_t)current_area; return -1; }  // wrap 未實作
    if (!enter_map(new_area)) { gs[2] = (std::uint8_t)current_area; return -1; }
    px = gx; py = gy; dir = gf;
    gs[0] = (std::uint8_t)px; gs[1] = (std::uint8_t)py; gs[3] = (std::uint8_t)dir;
    return 2;
  }
};

int g_pass = 0, g_fail = 0;

void check(const std::string& name, bool ok, const std::string& detail) {
  std::printf("[%s] %s%s%s\n", ok ? "PASS" : "FAIL", name.c_str(),
              detail.empty() ? "" : " — ", detail.c_str());
  if (ok) ++g_pass; else ++g_fail;
}

// 同區傳送案例:在 area 起步,把玩家放到 (sx,sy)(該格 tile 值觸發事件),跑事件 + relocate,
// 斷言 reloc 型別 + 落點。
//
// 注意:這些樓梯腳本以 gs[0x40] 之類「已觸發」旗標 gate(op_48 測 <0x80 → set bit,
//   op_45 jnz 跳過傳送)。對拍 opendw:每次踩到新格 op_71→op_73 會清旗標。
//   驗證時用 fresh World(每案獨立 game_state)= 模擬「首次踩到此格」語意,確定性。
void case_intra(const std::string& bundle, int area, int sx, int sy, int reloc_expect,
                int ex, int ey, const std::string& name) {
  World w(bundle);
  if (!w.enter_map(area)) { check(name, false, "enter_map failed"); return; }
  w.px = sx; w.py = sy;
  int tv = w.level->tile(sx, sy);
  w.run_event((std::uint8_t)tv);
  int r = w.sync_relocation();
  bool ok = (r == reloc_expect) && (w.current_area == area) && (w.px == ex) && (w.py == ey);
  char buf[160];
  std::snprintf(buf, sizeof buf,
                "tile 0x%02X @(%d,%d) reloc=%d(exp %d) -> area %d (%d,%d) exp (%d,%d)",
                tv, sx, sy, r, reloc_expect, w.current_area, w.px, w.py, ex, ey);
  check(name, ok, buf);
}

}  // namespace

int main(int argc, char** argv) {
  std::string bundle = (argc >= 2) ? argv[1] : "assets/bundle";

  std::printf("== 地圖換場 / 同區傳送驗證(bundle=%s)==\n\n", bundle.c_str());

  // ── 案例 1-6:area 27「Depths of Nisir」內部成對樓梯傳送(reloc=1,同區,落點 in-bounds)──
  // 逆向(probe_areaswitch):成對來回。每案 fresh World(首次踩到此格語意)。
  case_intra(bundle, 27, 14, 15, 1, 24, 17, "area27 樓梯 0x1B (14,15)->(24,17)");
  case_intra(bundle, 27, 24, 17, 1, 14, 15, "area27 樓梯 0x1C (24,17)->(14,15) 回程");
  case_intra(bundle, 27,  9, 17, 1, 22, 30, "area27 樓梯 0x1E (9,17)->(22,30)");
  case_intra(bundle, 27, 22, 30, 1,  9, 17, "area27 樓梯 0x1F (22,30)->(9,17) 回程");
  case_intra(bundle, 27, 19, 10, 1, 18, 21, "area27 樓梯 0x20 (19,10)->(18,21)");
  case_intra(bundle, 27, 18, 21, 1, 19, 10, "area27 樓梯 0x21 (18,21)->(19,10) 回程");

  // ── 案例 7:area 23「Mystic Wood」tile 0x04@(5,7) — 林間空地敘述事件(非換場)──
  // 腳本序列:op_73 → op_62(scan_char dl=0x20 cl=01)→ op_42(jc)→ op_74/7D/78/89。
  // batch12 起 op_62 為「忠實掃描」(對拍 opendw byte-identical):party 無成員的
  //   property[0x20] >= 1 → 未命中 → 迴圈結束僅設 cpu.cf=1、**不寫 word_3AE6**;
  //   故 op_42(讀 word_3AE6 的 carry bit)**不跳轉** → 落入空地敘述文字分支,
  //   gs[2] 維持 23(不換場)。此與 oracle 逐指令一致(見 build_trace_oracle_batch10.sh
  //   對拍:op_62→op_42 兩側 word_3AE6 皆 0、皆不跳)。
  // 註:舊版此案例 exp gs[2]=0,係依賴「op_62 stub 永遠 flags|=carry」的錯誤行為使
  //   op_42 誤跳到換場分支;補完忠實 op_62 後修正為 gs[2]=23(不換場)。
  {
    World w(bundle);
    if (w.enter_map(23)) {
      w.px = 5; w.py = 7;
      std::string msg = w.run_event(w.level->tile(5,7));
      int wrote_area = w.gs[2];                        // relocate 前:腳本是否改 area
      int r = w.sync_relocation();
      bool ok = (wrote_area == 23) && (r == 0) && (w.current_area == 23);
      char buf[220]; std::snprintf(buf,sizeof buf,
        "敘述事件不換場:gs[2]=%d(exp 23,op_62 未命中→op_42 不跳);reloc=%d(exp 0);"
        "current_area=%d(exp 23) emit=\"%.30s\"", wrote_area, r, w.current_area, msg.c_str());
      check("area23 空地敘述事件(op_62 忠實→不誤換場)", ok, buf);
    } else check("area23 空地敘述事件(op_62 忠實→不誤換場)", false, "enter_map");
  }

  // ── 案例 8:普通地面不誤觸發 relocate(area 1 Purgatory 的 tile==1 地面格 → reloc=0)──
  {
    World w(bundle);
    if (w.enter_map(1)) {
      // 找一個 tile==1 的普通地面格(Purgatory 非 wrap,必有)
      int fx=-1,fy=-1; for(int y=0;y<w.level->h&&fx<0;++y)for(int x=0;x<w.level->w;++x)
        if(w.level->tile(x,y)==1){fx=x;fy=y;break;}
      if (fx>=0) {
        w.px=fx; w.py=fy;
        w.run_event((std::uint8_t)w.level->tile(fx,fy));   // tile 1 無 script → 回空
        int r=w.sync_relocation();
        bool ok=(r==0)&&(w.current_area==1)&&(w.px==fx)&&(w.py==fy);
        char buf[120]; std::snprintf(buf,sizeof buf,"地面格(%d,%d) reloc=%d(exp 0) 位置不變",fx,fy,r);
        check("普通地面不誤觸發換場", ok, buf);
      } else check("普通地面不誤觸發換場", false, "no ground tile in area 1");
    } else check("普通地面不誤觸發換場", false, "enter_map");
  }

  std::printf("\n== %d PASS, %d FAIL ==\n", g_pass, g_fail);
  std::printf("%s\n", g_fail == 0 ? "PASS: area-switch verification" : "FAIL: area-switch verification");
  return g_fail == 0 ? 0 : 1;
}
