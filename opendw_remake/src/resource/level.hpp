// level — 解析原版關卡(.lvl,= resource_load(area+0x46) 的解壓資料)。
//
// 依 opendw read_level_metadata:前 4 byte = 高/寬/旗標 + 變長 section + 關卡名 offset
// + tile 格(column-major 反序,每格 3 byte:word_11C6 牆屬性 + word_11C8 tile 型/事件)。
// tile 型(word_11C8):0=void/牆、1=可走地面、2..F=特殊格(水/建築/事件/門…)。
#pragma once
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace dw::res {

class Level {
public:
  int h = 0, w = 0, flags = 0;
  std::string name;

  static std::optional<Level> load_file(const std::filesystem::path& lvl);   // 讀 .lvl
  static std::optional<Level> from_bytes(std::vector<std::uint8_t> bytes);    // 直接給 bytes

  // tile 型(word_11C8 第 3 byte):0=void、1=地面、其他=特殊/事件。
  std::uint8_t tile(int x, int y) const;
  // 覆寫 in-memory grid 的一格 tile value(不動 .lvl 檔;供 remake 還原 overlay,見
  //   restore_phoebus_entrance)。越界則 no-op。
  void set_tile(int x, int y, std::uint8_t v);
  // ── remake 還原:菲巴斯(area 6)世界圖入口 ───────────────────────────────
  //   原版 DATA1 area 0 定義了菲巴斯入口 tile 0x07(worldmap_dest=6)卻**未放置在 grid**
  //   (唯一未放置的城市 tile;bundle==DATA1 byte-for-byte 確認)。但地圖(Dilmun)與
  //   placed 的菲巴斯歡迎事件 tile 0x24 @(10,5)(首訪播段落 25/26)都標示菲巴斯在太陽島此處。
  //   本還原把入口 tile 0x07 放到歡迎事件旁的陸地格 (10,4),讓玩家走到太陽島即可進菲巴斯
  //   (補回原版疏漏的「進城格」)。誠實標示:remake 設計還原,非原版資料原狀;只改 in-memory
  //   grid、不動 .lvl(bundle==DATA1 不破)。僅 area 0 呼叫。
  void restore_phoebus_entrance();
  // 牆屬性(word_11C6,2 byte)。
  std::uint16_t wall(int x, int y) const;
  bool in_bounds(int x, int y) const { return x >= 0 && y >= 0 && x < w && y < h; }
  bool walkable(int x, int y) const { return in_bounds(x, y) && tile(x, y) != 0; }

  // ── wrap 邊界(.lvl flags bit1 = 0x2)────────────────────────────────────
  // opendw `check_map_boundary_x/y`(engine.c:5147/5176)在「座標越界 && flag&2」
  // 時走 `exit(1)`(未實作)。前後文(非 wrap 分支對 x>=W 夾到 W-1、x<0 夾到 0,
  // 並設 blocked 旗標)透露 wrap 分支本應做「modular 環繞」:走出東緣→西緣、
  // 走出南緣→北緣。此處以該環繞慣例補上(誠實標示:opendw oracle 未實作)。
  bool wraps() const { return (flags & 0x2) != 0; }
  // 把座標折回有效範圍(僅 wrap 關卡;非 wrap 關卡原樣回傳,由呼叫端 in_bounds 判定)。
  int wrap_x(int x) const { return (w > 0) ? ((x % w) + w) % w : x; }
  int wrap_y(int y) const { return (h > 0) ? ((y % h) + h) % h : y; }
  // wrap 關卡:座標先 modular 環繞再查 tile(永遠在界內,故可走 = tile!=0)。
  // 非 wrap 關卡:退回一般 walkable(越界即不可走)。
  bool walkable_wrap(int x, int y) const {
    if (!wraps()) return walkable(x, y);
    return tile(wrap_x(x), wrap_y(y)) != 0;
  }

  // 關卡 bytecode(level 資源本身也是 script);供 VM 執行事件腳本。
  const std::vector<std::uint8_t>& data() const { return b_; }
  // tile 格起點 offset(= read_level_metadata 解析完 header 後的 di;= data_5A04 基準)。
  std::size_t grid() const { return grid_; }
  // 原始 byte 存取(供 viewport_compose port 直接模擬 data_5521[di])。
  std::uint8_t byte_at(std::size_t i) const { return i < b_.size() ? b_[i] : 0; }
  std::size_t size() const { return b_.size(); }
  // 特殊格事件腳本入口(對拍 opendw op_71/run_level_script):
  //   script 表起點 = grid + w*3*h(= data_5A04[0]);entry = base + (tile_value+1)*2;
  //   該處 16-bit = script PC(level bytecode 內)。
  std::size_t script_table() const { return grid_ + (std::size_t)w * 3 * h; }
  std::uint16_t script_pc(std::uint8_t tile_value) const {
    std::size_t e = script_table() + (std::size_t)(tile_value + 1) * 2;
    return (e + 1 < b_.size()) ? (std::uint16_t)(b_[e] | (b_[e + 1] << 8)) : 0;
  }

  // ── 世界圖 / 樞紐「踩格進區」轉移目標(DRAGON.COM 反組譯反推)─────────────
  //
  // 來源:Dilmun 世界圖(area 0)上的城鎮/地點格,事件腳本固定為
  //   58 08 06 00 | <NN> 30 <MM> | <op∈{60,68,70}> <IDX> | <座標 tail...>
  // 即 op_58 呼叫共享資源 8 @off=6(世界圖地點處理常式),其後第 1 個 byte
  // <IDX> = 目的地 area id(0..39)。符合此頭的 27 格中,26 格 IDX 落在合法 area
  // 範圍,且與攻略(38/39)世界圖地點 1:1(波卡城=1、奴隸營=2、塔斯=5、
  // 黃泥蟾蜍=8、自由港=17、京雄城=25、龍谷=32…);唯 tile 0x1C @(27,7) 的 IDX=0x89
  // 超出範圍(特殊/非換區格,排除)。注意:菲巴斯(6)/拜占儂(9) **不在** area 0
  // 世界圖 26 格直連表中 —— 它們是「由母區踩格進入」的子區(菲巴斯←黃泥蟾蜍 8、
  // 拜占儂←軍營 29),其進入鏈卡在未實作 opcode(op_79 等),見 docs/gameplay/54。
  //
  // 重要誠實標示:**opendw 對此路徑無實作**(resource 8 的 op_58 子常式 +
  //   op_68/op_70 在 opendw targets[] 標 NULL → exit/未逆出)。本對映是
  //   **直接從 DRAGON.COM 16-bit 反組譯 + 0.lvl bytecode 靜態反推**,經攻略
  //   地點交叉驗證(7/7 已知 area 名吻合);入口座標/朝向因受 resource 8
  //   runtime 控制流阻而**未能靜態逆出**,故進區後落點採目標關卡第一可走格
  //   (連通正確,非 byte-exact 入口)。詳見 docs/assessment/49。
  //
  // 回傳:目的地 area id(0..39),或 -1(非世界圖換區格)。
  int worldmap_dest(std::uint8_t tile_value) const {
    std::uint16_t pc = script_pc(tile_value);
    if (pc == 0 || (std::size_t)pc + 9 > b_.size()) return -1;
    // 樣式頭:58 08 06 00(op_58 tag=0x08 off=0x0006)
    if (!(b_[pc] == 0x58 && b_[pc + 1] == 0x08 &&
          b_[pc + 2] == 0x06 && b_[pc + 3] == 0x00))
      return -1;
    // 其後第 4 byte 起為 <NN> 30 <MM> <op> <IDX>:op 必為 0x60/0x68/0x70。
    std::uint8_t op = b_[pc + 7];
    if (op != 0x60 && op != 0x68 && op != 0x70) return -1;
    // <IDX> 的 **bit7 是「需確認(op_8C Y/N)」旗標**,目的地 area = IDX & 0x7F。
    //   證據(動態 trace + DRAGON.COM 反組譯):resource 8 @off=6 的目的地解碼段
    //   @0x01ad 為 `38 7F`(op_38 AND 0x7F)→ `12 02`(op_12 gs[2]=r2)。即 VM
    //   實際把 IDX 低 7 bit 寫進 gs[2]。area 0 唯一 bit7-set 格是 tile 0x1C
    //   IDX=0x89 → 0x89 & 0x7F = **9 = Byzanople 拜占儂**(舊版誤判 0x89>39 為
    //   「非換區格」而漏掉此邊;實機按 Y 即進拜占儂)。其餘 27 格 bit7=0,遮罩後值不變。
    //   交叉驗證:trace_subarea_dyn area 0 --yes 對 tile 0x1C 跑出 gs2:0->9。
    int idx = b_[pc + 8] & 0x7F;
    if (idx < 0 || idx > 39) return -1;
    return idx;
  }

  // ── 子區(dungeon/地下/水下)relocate 目標(DRAGON.COM + resource 5 反組譯逆出)──
  //
  // 來源:子區進入格的事件腳本固定樣式
  //   1A 41 <X> | 1A 43 <Y> | 1A 45 <AREA> | 58 05 <off16>
  // 即 op_1A 把 var 0x41/0x43/0x45 設成常數,再 op_58 呼叫共享資源 5(relocate
  // 確認 handler)。resource 5 任一入口(off=0/3/6/9)都匯流到同一段 Y/N 提示
  // (op_8C「Do you wish to enter …?」)+ 「Yes」分支(@0x005F):
  //   19 41 00  → gs[0]   = gs[0x41]   (入口 X)
  //   19 43 01  → gs[1]   = gs[0x43]   (入口 Y)
  //   19 45 02  → gs[2]   = gs[0x45]   (目的地 area)
  // (op_19 = gs[dst]=gs[src],bytes `19 <src> <dst>`)。即 var 0x41/0x43/0x45 =
  // (X, Y, area)。「No」分支(op_45 JNZ → 0x006D)直接 op_59 返回不換區。
  //
  // 與 worldmap_dest 不同處:此鏈是「玩家確認後」才換區(op_8C 提示),且**入口
  //   座標 byte-exact 逆出**(= gs[0x41]/gs[0x43]),非哨兵。relocate 真正寫 gs[2]
  //   的時點是 resource 5 的 Yes 分支(headless 無鍵盤 → 取 No,故動態跑不到;
  //   本解碼器**靜態**讀 1A 45 <AREA> 即得目的地)。
  //
  // 誠實標示:resource 5 的 relocate 段以 DRAGON.COM dispatch + 動態 trace 反組譯
  //   逆出(opendw targets[] 對 op_58 子常式路徑無 C oracle);三常數 → (X,Y,area)
  //   的對映由「res5 Yes 分支 19 41 00 / 19 43 01 / 19 45 02」直接證實,信心高。
  //
  // 掃描:在整個 level bytecode 找 `1A 41 .. 1A 43 .. 1A 45 .. 58 05` 連續樣式;
  //   回傳所有 (dest_area, X, Y)。dest_area 須落在 0..39。
  struct SubareaReloc { int area, x, y; std::size_t at; };
  std::vector<SubareaReloc> subarea_relocs() const {
    std::vector<SubareaReloc> out;
    for (std::size_t i = 0; i + 12 < b_.size(); ++i) {
      if (b_[i] == 0x1A && b_[i + 1] == 0x41 && b_[i + 3] == 0x1A &&
          b_[i + 4] == 0x43 && b_[i + 6] == 0x1A && b_[i + 7] == 0x45 &&
          b_[i + 9] == 0x58 && b_[i + 10] == 0x05) {
        int x = b_[i + 2], y = b_[i + 5], area = b_[i + 8];
        if (area >= 0 && area <= 39) out.push_back({area, x, y, i});
      }
    }
    return out;
  }

  // ── 直接寫 gs[2] 的「城/區進入」relocate(動態 trace 逆出的第三種機制)──────
  //
  // 來源:母區進入格 / 城門格的事件腳本固定樣式(對照 area 29 Siege Camp tile 0x0D)
  //   1A 00 <X>     op_1A:gs[0]  = X(入口 X 座標)
  //   1A 01 <Y>     op_1A:gs[1]  = Y(入口 Y 座標)
  //   1A 02 <AREA>  op_1A:gs[2]  = 目的地 area  ← **直接換區**(gs[2]=current area var)
  // 即與 subarea_relocs 同形,但 **目的地寫進 gs[0/1/2]**(立即生效的 area 變數),
  // 不經 resource 5 的 gs[0x41/43/45]→gs[0/1/2] 中轉。多數此類格前面有 op_8C
  //   (「Do you wish to enter <城>?」Y/N 提示),玩家答 Y 才落到這三行 → headless
  //   無鍵盤預設取 No,故動態跑不到,但注入 'Y'(trace_subarea_dyn --yes)即跑出
  //   gs2:src->AREA。本解碼器靜態讀 1A 02 <AREA> 即得目的地、1A 00/01 即得入口座標。
  //
  // **關鍵發現(逆出 Byzanople 拜占儂 9 進城)**:area 29 @0x04FA 為 `1A 00 07 /
  //   1A 01 09 / 1A 02 09`(op_8C-gated),即 Siege Camp 軍營踩該格答 Y → 進拜占儂
  //   (gs[2]=9,入口 (7,9))。**舊版兩套機制(worldmap_dest off=6、subarea_relocs
  //   1A 45)都掃不到此邊**(無 1A 45 09),這就是 docs/gameplay/54 §2.3 所述「第三種未識別
  //   機制」。動態 trace(trace_subarea_dyn 29 --yes)實證 gs2:29->9。
  //
  // 誠實標示:目的地 + 入口座標 byte-exact(直接讀 1A 02/1A 00/1A 01 的 immediate);
  //   「是否 Y/N 門控」由 8c_gated 標記(掃描前置 op_8C),不影響連通(答 Y 必經此格)。
  struct AreaEntry { int area, x, y; std::size_t at; bool gated; };
  std::vector<AreaEntry> area_entry_relocs() const {
    std::vector<AreaEntry> out;
    for (std::size_t i = 0; i + 9 < b_.size(); ++i) {
      if (b_[i] == 0x1A && b_[i + 1] == 0x00 && b_[i + 3] == 0x1A &&
          b_[i + 4] == 0x01 && b_[i + 6] == 0x1A && b_[i + 7] == 0x02) {
        int x = b_[i + 2], y = b_[i + 5], area = b_[i + 8];
        if (area < 0 || area > 39) continue;
        // 是否被 op_8C(prompt_no_yes)門控:回看前 12 byte 是否出現 0x8C。
        bool gated = false;
        for (std::size_t j = (i >= 12 ? i - 12 : 0); j < i; ++j)
          if (b_[j] == 0x8C) { gated = true; break; }
        out.push_back({area, x, y, i, gated});
      }
    }
    return out;
  }

private:
  std::vector<std::uint8_t> b_;
  std::size_t grid_ = 0;
  std::size_t off(int x, int y) const {
    return grid_ + (std::size_t)(w - 1 - x) * 3 * h + (std::size_t)y * 3;
  }
};

}  // namespace dw::res
