// main — OpenDW Remake app(雙層渲染:像素層 + 高解析 TTF 文字層)。
//
// 雙層架構(見 docs/adr/0002-two-layer-cjk-rendering.md):
//   像素層:320×200 indexed framebuffer(viewport/sprite/scene/地圖 tile)→ 整數倍
//           nearest 放大到視窗(預設 3× = 960×600),維持與原版像素級對拍。
//   文字層:UI/選單/事件/段落/標題等 CJK + 在地化文字 → SdlVideo::text()(TextLayer),
//           用 SDL2_ttf 載 wqy-zenhei 在視窗高解析原生繪製,疊在像素層之上,永不縮放。
//
// 已完成小段:
//   B  bundle bytecode → VM op_78 → i18n 繁中 → 文字層 → SDL 顯示(在地化選單)
//   A  --sprite NAME:從 bundle 載 .spr 美術顯示在視窗(不碰 DATA1)
//   C  互動骨架:poll() → Input 事件
//   D  快捷字母選單 + 狀態分支,操作與說明書一致(見 docs/engine/CONTROLS.md):
//      B=開始新遊戲、C=繼續舊遊戲;↑↓/Enter 為輔助;Esc 返回 / Q 離開。
//
// 像素資產來自 bundle(自包含:dw8x8.bin + sprites/scenes/maps);文字字型用 host TTF。
//
// 用法:opendw_remake [--bundle DIR] [--font RAW] [--menu TSV] [--scale N]
//                     [--font-ttf PATH] [--pc N] [--sprite NAME] [--frames N]
//                     [--dump PPM] [--press CH] [--map N] [--fp] [--at X Y]
//                     [--title] [--no-splash]   開機 title splash(火龍之戰 art);詳見 docs/engine/CONTROLS.md
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <map>
#include <optional>
#include <set>
#if defined(_WIN32)
#include <direct.h>
#define DWR_CHDIR _chdir
#else
#include <unistd.h>
#define DWR_CHDIR chdir
#endif
#include <string>
#include <vector>
#include "resource/provider.hpp"
#include "vm/interpreter.hpp"
#include "render/font.hpp"
#include "render/framebuffer.hpp"
#include "render/sprite.hpp"
#include "render/picture.hpp"
#include "render/viewport.hpp"
#include "render/viewport_amiga.hpp"
#include "render/viewport_compose.hpp"
#include "render/minimap.hpp"
#include "render/worldmap.hpp"
#include "render/ui_pieces.hpp"
#include "render/ui_theme.hpp"
#include "render/sdl_video.hpp"
#include "audio/sound.hpp"
#include "resource/level.hpp"
#include "resource/paragraphs.hpp"
#include "i18n/strings.hpp"
#include "game/party.hpp"
#include "game/chargen.hpp"
#include "game/savegame.hpp"
#include "game/combat.hpp"
#include "game/combat_loop.hpp"
#include "game/spells.hpp"
#include "game/seen_map.hpp"
#include "game/terrain_state.hpp"
#include "game/terrain.hpp"
#include "game/real_terrain.hpp"
#include "game/progression.hpp"
#include "game/shop.hpp"
#include "game/recruit.hpp"
using namespace dw;

static std::vector<std::string> lines_of(const std::string& s) {
  std::vector<std::string> o; std::string c;
  for (char ch : s) { if (ch == '\r' || ch == '\n') { o.push_back(c); c.clear(); } else c.push_back(ch); }
  o.push_back(c); return o;
}

struct Opt { char hot; std::string label; std::string en; };  // 快捷字母 + 在地化文字 + 英文源(F4 重譯用)

// MsgViewer — 可分頁捲動的訊息/段落檢視器(對齊原版「看完中央訊息後按 Esc 繼續」)。
//
// Deep module:對外只露 open/advance/close/active + 取當前頁文字行。內部隱藏
//   自動換行(走 TextLayer::wrap,CJK 逐字 / ASCII 逐詞)+ 依框高的可見行數切頁。
//
// 框落在 320×200 虛擬座標(由 main 的 draw 函式畫底框 + 邊框,並逐行 add 文字層)。
// 翻頁鍵 Space/Enter/↓/I 下一頁;最後一頁再按 → 關閉;Esc → 直接關閉。
// F4 切語系時 main 以新文字 reflow(),停在同一頁號(夾在有效範圍)。
struct MsgViewer {
  bool active = false;
  int page = 0;                              // 當前頁(0-based)
  int lines_per_page = 1;                    // 每頁可見行數(由框高算)
  int max_vw = 0, body_px = 0;               // 換行寬度(虛擬)+ 字級(視窗 px)
  std::vector<std::string> lines;            // 全文換行後的所有行

  // 開啟檢視器:以 wrap 後的行 + 每頁行數初始化(回到第 1 頁)。
  void open(std::vector<std::string> wrapped, int per_page) {
    lines = std::move(wrapped);
    lines_per_page = per_page < 1 ? 1 : per_page;
    page = 0;
    active = true;
  }
  void close() { active = false; page = 0; lines.clear(); }

  int page_count() const {
    if (lines.empty()) return 1;
    return (int)((lines.size() + lines_per_page - 1) / lines_per_page);
  }
  bool has_more() const { return page + 1 < page_count(); }   // 還有下一頁 → 顯示 ▼

  // 翻頁:非最後頁 → page++ 並回傳 true(續顯示);最後頁 → 關閉並回傳 false(回遊戲)。
  bool advance() {
    if (has_more()) { ++page; return true; }
    close();
    return false;
  }

  // 取當前頁要顯示的行(切片)。
  std::vector<std::string> page_lines() const {
    std::vector<std::string> out;
    int start = page * lines_per_page;
    for (int i = start; i < (int)lines.size() && i < start + lines_per_page; ++i)
      out.push_back(lines[i]);
    return out;
  }

  // F4 重排:以新換行後的行重建,夾住頁號(語系變了,總頁數可能不同)。
  void reflow(std::vector<std::string> wrapped) {
    lines = std::move(wrapped);
    int pc = page_count();
    if (page >= pc) page = pc - 1;
    if (page < 0) page = 0;
  }
};

// ParaViewer — Read Paragraph 長段落捲動檢視器(scrollable overlay 子狀態)。
//
// 與 MsgViewer 的差異(為什麼另立一個 deep module 而非塞進 MsgViewer):
//   MsgViewer 是「下半部 ~3 行的分頁訊息框」,對齊原版中央訊息;段落(防拷手冊)動輒
//   1000+ 字、wrap 後 50+ 行,需要一個近全螢幕、可逐行/逐頁平滑捲動的覆蓋面板,且要顯示
//   標題「段落 N」與捲動位置。介面語意(行偏移捲動 vs 翻頁切片)不同 → 分開,各自窄介面。
//
// 對外只露 open/close/active + scroll_line/scroll_page + 取「可見行切片」與位置資訊;
// 內部隱藏 wrap 後的全行、可視行數、行偏移 top_ 的夾制。框與文字由 main 的 draw 函式繪製。
//
// 操作:↑↓ 逐行捲動;PgUp/PgDn / Space / Enter 逐頁;Esc 關閉回遊戲。
// F4 切語系時 main 以新文字 reflow();段落僅 zh-TW 有資料,缺其他語系時 main 回退 zh-TW 全文。
struct ParaViewer {
  bool active = false;
  int para_n = 0;                 // 段落號 N(標題顯示「段落 N」)
  int top = 0;                    // 視窗頂端對應的行索引(0-based;捲動狀態)
  int visible_lines = 1;          // 可視行數(由框高算)
  std::vector<std::string> lines; // wrap 後的全部行

  void open(int n, std::vector<std::string> wrapped, int vis) {
    para_n = n;
    lines = std::move(wrapped);
    visible_lines = vis < 1 ? 1 : vis;
    top = 0;
    active = true;
  }
  void close() { active = false; top = 0; lines.clear(); }

  int total_lines() const { return (int)lines.size(); }
  // 可捲動的最大頂端行(讓最後一頁剛好填滿;不足一頁則為 0)。
  int max_top() const {
    int m = total_lines() - visible_lines;
    return m < 0 ? 0 : m;
  }
  bool at_top() const { return top <= 0; }
  bool at_bottom() const { return top >= max_top(); }
  // 當前頁碼 / 總頁數(以可視行數切;捲動位置提示用)。
  int page_count() const {
    if (lines.empty()) return 1;
    return (total_lines() + visible_lines - 1) / visible_lines;
  }
  int cur_page() const { return visible_lines > 0 ? top / visible_lines + 1 : 1; }

  void clamp() { if (top < 0) top = 0; if (top > max_top()) top = max_top(); }
  void scroll_line(int d) { top += d; clamp(); }                 // ↑↓ 逐行
  void scroll_page(int d) { top += d * visible_lines; clamp(); } // PgUp/PgDn 逐頁

  // 取目前可見的行切片(top .. top+visible_lines)。
  std::vector<std::string> visible() const {
    std::vector<std::string> out;
    for (int i = top; i < total_lines() && i < top + visible_lines; ++i)
      out.push_back(lines[i]);
    return out;
  }

  // F4 重排:語系變了用新 wrap 行重建;盡量維持捲動位置(夾住 top)。
  void reflow(std::vector<std::string> wrapped) {
    lines = std::move(wrapped);
    clamp();
  }
};

// CharSheet — 角色屬性表檢視子狀態(對齊原版手冊 V=查看人物特質 / X=屬性畫面)。
//
// Deep module:對外只露 open/close/active/select + 取當前角色 index。內部隱藏
//   「選哪名角色」狀態;版面(框 + 各屬性列)由 main 的 draw 函式統一以 MsgViewer
//   風格的底框 + 文字層繪製。多語(F4)即時重排:畫面每幀重繪 → 自動套用新語系標籤。
//
// 進入:in-game / 選單按 V(或數字 1-4 直接選該角色)。
// 切換:↑↓ 或數字 1-4 換角色;Esc 關閉。
struct CharSheet {
  bool active = false;
  int idx = 0;           // 當前檢視的角色(0-based)
  int count = 0;         // 隊伍人數(夾住 idx)
  bool show_inventory = false;  // false=屬性表;true=物品欄(背包)。E 鍵切換。
  // ── 成長操作子模式(手冊 X=屬性分配畫面 / U=使用物品 / 裝備穿脫)──
  bool alloc_mode = false;  // 屬性表中按 X → 進 AP 配點模式(↑↓ 選項目、+ 加點)。
  int alloc_cursor = 0;     // 配點游標:0-3=STR/DEX/INT/SPI、4-6=包紮/開鎖/徒手技能。
  int inv_cursor = 0;       // 物品欄游標(U 使用 / Enter 裝備穿脫的目標格,0-based「已顯示」序)。
  // ── 物品轉移子模式(手冊 Item:Transfer)──
  bool transfer_mode = false;  // 物品欄按 T → 進「選目標隊員」模式(↑↓ 選、Enter 確認、Esc 取消)。
  int transfer_slot = -1;      // 待轉移的真實 slot(0..12;進入子模式時鎖定)。
  int target_cursor = 0;       // 目標隊員游標(0-based,跳過自己)。
  // ── 刪除人物確認子模式(手冊 D)──
  bool delete_confirm = false; // 屬性表按 D → 進刪除確認(Y 確認 / N/Esc 取消)。
  // ── 改名輸入子模式(手冊 R)──
  bool rename_mode = false;    // 屬性表按 R → 進改名輸入(TTF 文字輸入;Enter 確認 / Esc 取消)。
  std::string rename_buf;      // 改名輸入緩衝。
  std::string flash;        // 暫時提示行(配點/用物品/裝備結果)。

  static constexpr int kAllocCount = 7;  // 配點可選項目數(4 屬性 + 3 高價值技能)

  void open(int n, int start = 0) {
    count = n < 1 ? 0 : n;
    idx = start;
    if (count > 0) { if (idx < 0) idx = 0; if (idx >= count) idx = count - 1; }
    active = count > 0;
    show_inventory = false;
    alloc_mode = false; alloc_cursor = 0; inv_cursor = 0; flash.clear();
    clear_submodes();
  }
  // 重置所有次要指令子模式(轉移 / 刪除確認 / 改名)。
  void clear_submodes() {
    transfer_mode = false; transfer_slot = -1; target_cursor = 0;
    delete_confirm = false; rename_mode = false; rename_buf.clear();
  }
  void toggle_view() {            // 屬性表 ⇄ 物品欄(離開配點模式)
    show_inventory = !show_inventory;
    alloc_mode = false; inv_cursor = 0; flash.clear();
    clear_submodes();
  }
  void close() { active = false; show_inventory = false; alloc_mode = false; flash.clear(); clear_submodes(); }
  void prev() {
    if (count > 0) idx = (idx - 1 + count) % count;
    inv_cursor = 0; alloc_cursor = 0; flash.clear(); clear_submodes();
  }
  void next() {
    if (count > 0) idx = (idx + 1) % count;
    inv_cursor = 0; alloc_cursor = 0; flash.clear(); clear_submodes();
  }
  // 數字鍵 1-count 直選;越界忽略。回傳是否命中。
  bool select(int n) {
    if (n >= 1 && n <= count) { idx = n - 1; inv_cursor = 0; alloc_cursor = 0; flash.clear(); clear_submodes(); return true; }
    return false;
  }
};

// ShopUi — 商店買賣子狀態(踩商店格 / 按 P / headless --shop)。
//
// Deep module:對外只露 open/close/active + 游標 + 買賣分頁狀態;買賣規則全委派
//   game::Shop(shop.hpp)。版面走 char-sheet 風格底框 + 文字層。
//   買在「商店庫存」清單上操作;賣在「當前角色背包」清單上操作。
//   付款/收款方 = 隊伍第 0 名(主角;remake 設計,原版以隊伍共用金幣)。
struct ShopUi {
  bool active = false;
  bool sell_mode = false;   // false=買(瀏覽庫存);true=賣(瀏覽背包)。Tab 切換。
  int cursor = 0;           // 當前頁的游標(買=庫存 index;賣=背包 present 序)。
  std::string flash;        // 操作結果提示行。
  void open() { active = true; sell_mode = false; cursor = 0; flash.clear(); }
  void close() { active = false; flash.clear(); }
  void toggle_mode() { sell_mode = !sell_mode; cursor = 0; flash.clear(); }
};

// TavernUi — 酒館招募子狀態(踩酒館格 / 按 T / headless --recruit)。
//
// Deep module:對外只露 open/close/active + 游標;招募規則委派 game::recruit_npc
//   (recruit.hpp)。清單 = RecruitRoster::roster();已在隊伍者標 (in party) 不可再招。
struct TavernUi {
  bool active = false;
  int cursor = 0;           // 招募名冊游標。
  std::string flash;
  void open() { active = true; cursor = 0; flash.clear(); }
  void close() { active = false; flash.clear(); }
};

// ReorderUi — 重排隊伍子狀態(手冊 / CONTROLS:O 重排隊伍順序;S_GAME 期間)。
//
// Deep module:對外只露 open/close/active + 游標 + 「已抓起」的成員。操作:↑↓ 移游標,
//   Enter / Space「抓起 / 放下」當前成員(抓起後 ↑↓ 把它與相鄰成員對調,即 Party::move)。
//   重排影響戰鬥站位(第 0 名 = 主角 / 施法者)與右側面板顯示順序。
//   真值層級:remake 設計(grounded 手冊「O 重排隊伍」)。
struct ReorderUi {
  bool active = false;
  int cursor = 0;        // 當前游標位置(0-based)。
  int grabbed = -1;      // 已抓起的成員索引(-1 = 未抓起;= cursor 時隨游標移動)。
  std::string flash;
  void open() { active = true; cursor = 0; grabbed = -1; flash.clear(); }
  void close() { active = false; grabbed = -1; flash.clear(); }
};

// ExploreCast — 戰鬥外探索施法子狀態(手冊 C=施法;S_GAME 期間)。
//
// Deep module:對外只露 open/close/active + 游標 + 可施法清單(隊伍第 0 名
//   castable_spells)。地形法術(Soften Stone / Disarm Trap / Sense Traps)的效果
//   委派 game::apply_terrain_spell(terrain.hpp);其餘法術(戰鬥傷害/治療類)在
//   探索時施放只扣 Power(無探索效果,提示「沒有任何效果」)。誠實標示:探索施法
//   結算為 remake 設計(opendw 探索施法 op 未反編;見 docs/gameplay/57_DOORS_TRAPS_TERRAIN.md)。
struct ExploreCast {
  bool active = false;
  int cursor = 0;
  std::vector<std::uint8_t> spellbook;  // 隊伍第 0 名 castable_spells
  std::string flash;                    // 施法結果提示
  void open() { active = true; cursor = 0; flash.clear(); }
  void close() { active = false; flash.clear(); }
};

// 配點項目 → progression 操作的對映(0-3 屬性、4-6 技能 index)。
// 回傳 {is_attr, target, label_en}:is_attr=true 時 target 為 AttrTarget,否則 skills index。
struct AllocTarget { bool is_attr; int target; const char* label_en; };
static AllocTarget alloc_target_at(int cursor) {
  using namespace dw::game::progression;
  switch (cursor) {
    case 0: return {true,  kAttrStr, "Strength"};
    case 1: return {true,  kAttrDex, "Dexterity"};
    case 2: return {true,  kAttrInt, "Intel"};
    case 3: return {true,  kAttrSpi, "Spirit"};
    case 4: return {false, kBandage,        "skill_bandage"};
    case 5: return {false, kLockpick,       "skill_lockpick"};
    case 6: return {false, 0x2B - 0x24,     "skill_fist"};   // 徒手 0x2B
    default: return {true, kAttrStr, "Strength"};
  }
}

// CharGenUi — 新遊戲建角流程(手冊選單 B → 建立人物)子狀態(S_CREATE)。
//
// Deep module:對外只露 phase/draft/done_records + 操作介面(輸入名/配點/性別/
//   完成本員/開始遊戲)。內部隱藏:兩段子流程(命名 → 配點)、已建角色累積、
//   配點游標。建角規則 / 序列化全委派給 game::DraftCharacter(grounded,見 chargen.hpp)。
//
// 流程(對齊手冊 33 第 6/7 頁):
//   PhName  輸入角色名(TTF 文字輸入;Enter 確認 → PhAttr;Esc 取消回選單)。
//   PhAttr  配點畫面:↑↓ 選屬性、+/− 或 ←→ 調整、G 切性別、Enter 完成本員
//           (寫進 done_records),最多 4 名;再按 B/Enter(無餘額或滿員)開始遊戲。
//           Esc 回名字輸入。
// 名字寫進 record [00-11] 高位元終止格式(由 DraftCharacter::serialize 處理)。
struct CharGenUi {
  enum Phase { PhName, PhAttr } phase = PhName;
  bool active = false;
  dw::game::DraftCharacter draft;                 // 當前正在建立的角色草稿
  int cursor = 0;                                 // 配點游標(0..3 = STR/DEX/INT/SPI)
  std::vector<std::array<std::uint8_t, 512>> done_records;  // 已完成的角色 records(組隊用)

  static constexpr int kMaxParty = 4;

  void start() {
    active = true; phase = PhName; cursor = 0;
    draft = dw::game::DraftCharacter{};
    done_records.clear();
  }
  // 開始建立「下一名」角色(沿用已完成清單)。回傳是否還能再建(未滿員)。
  bool begin_next() {
    if ((int)done_records.size() >= kMaxParty) return false;
    draft = dw::game::DraftCharacter{};
    phase = PhName; cursor = 0;
    return true;
  }
  // 完成本員:草稿合法(名字有效)→ 序列化推進清單。回傳是否成功。
  bool commit_current() {
    if (!draft.name_valid()) return false;
    done_records.push_back(draft.serialize());
    return true;
  }
  void close() { active = false; done_records.clear(); }
};

// tile 型(word_11C8)→ framebuffer 顏色:0=void/牆、1=地面、其他=特殊/事件格。
static std::uint8_t tile_color(std::uint8_t t) {
  if (t == 0) return 8;            // 牆/void = 灰
  if (t == 1) return 1;            // 地面 = 深藍
  return (std::uint8_t)(t & 0x0F); // 特殊格 = 以 tile 值當調色盤索引(各類各色)
}

int main(int argc, char** argv) {
  // DWR_ASSET_DIR:指向「含 assets/ 的目錄」(Android 把 APK assets 解壓到 internal storage
  //   後設此環境變數;桌面啟動器亦可用)。設了就 chdir 過去,讓相對路徑 assets/ 生效。
  if (const char* ad = std::getenv("DWR_ASSET_DIR"); ad && *ad) {
    if (DWR_CHDIR(ad) != 0) std::fprintf(stderr, "DWR_ASSET_DIR chdir failed: %s\n", ad);
  }
  std::string bundle = "assets/bundle";
  std::string start_theme;   // --theme NAME:啟動即套指定 UI 主題(dos/amiga/x68000;headless 驗證用)
  std::string font_raw = "assets/fonts/dw8x8.bin";
  // 多國語系:F4 即時循環切換。清單固定 {zh-TW, en, ja};--locale 設定起始語系。
  // 加語言 = 在此清單加一項 + 加 assets/i18n/<locale>/ 資料夾,不改邏輯。
  const std::vector<std::string> locales = {"zh-TW", "en", "ja"};
  std::string locale = "zh-TW";   // i18n 取 assets/i18n/<locale>/(可 --locale 改起始)
  std::string menu_tsv;           // 空 = 由 locale 推導
  // 文字層 host TTF(雙層渲染);可 --font-ttf 覆寫(為日後日文/Noto 留路)。
  std::string font_ttf = "/usr/share/fonts/truetype/wqy/wqy-zenhei.ttc";
  int scale = 3;                  // --scale N:視窗 = 320*N × 200*N(預設 3 → 960×600,CJK≈36px 原生)
  bool win640 = false;            // --win640 / --mode 640x480:640×480 letterbox 模式(像素層 320×200 ×2 置中,字級固定 CJK24/UI16/標題48)
  int start_pc = 20, max_frames = -1, press = 0, map_area = -1;
  // --keys SEQ:headless 逐幀注入合成輸入序列(驗證 F1/F8/F10/ESC 確認流程等)。
  //   token 以逗號分隔,每 token 注入一幀:F1 F4 F8 F10 ESC ENTER SPACE UP DOWN LEFT RIGHT
  //   PGUP PGDN Y N 或單一字母(A-Z)。例:--keys F10,Y(請求離開→確認)。
  std::string keys_seq;
  int dump_frame = -1;            // --dump-frame N:在迴圈第 N 幀(--keys 處理後)再 dump 一次(驗證覆蓋層)
  int at_x = -1, at_y = -1;       // --at x y:把玩家放到指定格(headless 驗證事件文字)
  int msg_page = 0;               // --msg-page N:訊息檢視器先翻到第 N 頁再 dump(headless 驗證分頁)
  int read_para = -1;             // --read-para N:直接開段落 N 進捲動 overlay(headless 驗證長段落)
  int para_scroll = 0;            // --para-scroll N:dump 前先「逐頁」下捲 N 次(headless 驗證跨頁無遺漏)
  int char_sheet = -1;            // --char-sheet N:直接開第 N 名(1-based)角色屬性表(headless 驗證)
  bool show_inventory = false;    // --inventory:配合 --char-sheet 直接開物品欄(背包)子畫面
  bool alloc_open = false;        // --alloc:配合 --char-sheet 直接進 X 配點模式(headless 出圖)
  bool shop_open = false;         // --shop:headless 直接開商店買賣子畫面
  bool recruit_open = false;      // --recruit:headless 直接開酒館招募子畫面
  int grant_gold = -1;            // --gold N:啟動時把隊伍第 0 名 gold[81] 設為 N(商店 demo/截圖)
  bool demo_grow = false;         // --demo-grow:給角色 0 注入 AP + 範例可用/可裝備物品(成長 UI 出圖)
  int automap_area = -1;          // --automap N:headless 直接開第 N 區俯視平面地圖(`?` 鍵功能)
  int mm_seed = 0;                // --mm-seed:0=全圖探索 1=只玩家格 2=不 seed(測試/展示)
  bool mm_seed_set = false;       // 是否顯式給 --mm-seed;否則遊戲內用真實 fog of war
  std::string dump, sprite_name, scene_name;
  std::string save_path = "save/slot0.sav";  // 存/讀檔路徑;未給 --save-path 時於 main 改解析到「可寫使用者目錄」
  bool save_path_set = false;   // 是否顯式 --save-path(否則用 resolve_save_dir,避免 AppImage 唯讀 cwd 寫檔失敗)
  std::string load_path;        // --load <path>:啟動即讀檔還原(進遊戲)
  bool selftest_save = false;   // --selftest-save:headless round-trip 自測(印 PASS/FAIL)
  bool viewport_mode = false;   // --viewport:顯示原版第一人稱 viewport 靜態框架
  bool fp_mode = true;          // 預設第一人稱 3D viewport(火龍之戰本作視角);--map2d 切回俯視彩格
  int encounter_id = -1;        // --encounter N:直接進遭遇畫面(怪物表 index N)
  unsigned combat_seed = 0x1234;// --combat-seed N:結算 RNG 種子(確定性)
  int combat_rounds = 0;        // --combat-rounds N:dump 前自動打 N 回合(headless 驗證戰報)
  int combat_count = 6;         // --combat-count N:怪群數量(預設 6;沿用怪物表領頭怪)
  std::string combat_special;   // --combat-special <type>:headless 隊伍第 0 名用特殊攻擊一回合(驗證)
                                //   type ∈ mighty|disarm|advance|quick|dodge(grounded 手冊)
  int cast_spell_id = -1;       // --cast <spellId>:headless 在遭遇中施放該法術一次(驗證)
  bool cast_force = false;      // --cast-force:即使該角色未習得也施放(僅供驗證效果套用)
  int terrain_cast_id = -1;     // --terrain-cast <id>:headless 在 S_GAME 對前方/當前格施放地形法術一次(驗證)
  bool trap_probe = false;      // --trap-probe:headless 把隊伍移到本關最近真實陷阱格並觸發(驗證真陷阱接線)
  // ── 終戰 Namtar + 結局序列(可通關收官)──
  bool fight_namtar = false;    // --fight-namtar:用(預設/讀檔)隊伍 vs Namtar Boss 戰鬥(combat_loop)
  bool show_ending = false;     // --ending:headless 直接進結局序列(demo / 截圖,不打 Namtar)
  int ending_at = -1;           // --ending-idx N:結局序列直接從第 N 張過場場景起(截圖驗證每張)
  bool namtar_blessed = true;   // --no-bless:關閉「自由之劍受祝福」加成(預設套用,讓可勝)
  // ── 音效(PC speaker 風格方波;預設可關)──
  //   --mute 或環境變數 DWR_MUTE=1 → 靜音模式(CI/headless 不依賴音效裝置)。
  bool mute = (std::getenv("DWR_MUTE") != nullptr);
  // ── 建角流程(新遊戲 / 建立人物;手冊選單 B)──
  bool newgame = false;         // --newgame:啟動直接進建角畫面(S_CREATE)
  // ── 開機 title splash(顯示火龍之戰 dragon art → 按任意鍵進主選單;對齊 DOS「先 art 後選單」)──
  bool want_title = false;      // --title:強制顯示 title splash(即使其他旗標想跳過)
  bool no_splash = false;       // --no-splash:略過 splash 直接進主選單(headless / 自動化)
  std::string newgame_demo;     // --newgame-demo SPEC:headless 腳本化建角 + 出圖/驗證(見下方解析)
  std::string newgame_screen;   // --newgame-screen SPEC:停在配點畫面供截圖(同 SPEC 格式,只取第一員)
  for (int i = 1; i < argc; ++i) {
    auto eq = [&](const char* f) { return !std::strcmp(argv[i], f); };
    if (eq("--bundle") && i + 1 < argc) bundle = argv[++i];
    else if (eq("--theme") && i + 1 < argc) start_theme = argv[++i];
    else if (eq("--font") && i + 1 < argc) font_raw = argv[++i];
    else if (eq("--font-ttf") && i + 1 < argc) font_ttf = argv[++i];   // 文字層 host TTF
    else if (eq("--scale") && i + 1 < argc) scale = std::atoi(argv[++i]);  // 視窗整數倍率
    else if (eq("--win640")) win640 = true;                                // 640×480 letterbox 模式
    else if (eq("--mode") && i + 1 < argc) {                               // --mode 640x480
      std::string m = argv[++i];
      if (m == "640x480" || m == "640") win640 = true;
      else std::fprintf(stderr, "unknown --mode %s (use 640x480)\n", m.c_str());
    }
    else if (eq("--menu") && i + 1 < argc) menu_tsv = argv[++i];
    else if (eq("--locale") && i + 1 < argc) locale = argv[++i];   // 切語系(zh-TW / ja / …)
    else if (eq("--pc") && i + 1 < argc) start_pc = std::atoi(argv[++i]);
    else if (eq("--frames") && i + 1 < argc) max_frames = std::atoi(argv[++i]);
    else if (eq("--max-frames") && i + 1 < argc) max_frames = std::atoi(argv[++i]);  // --frames 別名(防誤用無限空轉)
    else if (eq("--dump") && i + 1 < argc) dump = argv[++i];
    else if (eq("--sprite") && i + 1 < argc) sprite_name = argv[++i];
    else if (eq("--scene") && i + 1 < argc) scene_name = argv[++i];
    else if (eq("--map") && i + 1 < argc) map_area = std::atoi(argv[++i]);   // 直接進某區地圖
    else if (eq("--automap") && i + 1 < argc) automap_area = std::atoi(argv[++i]);  // headless 開俯視地圖
    else if (eq("--mm-seed") && i + 1 < argc) { mm_seed = std::atoi(argv[++i]); mm_seed_set = true; } // 探索旗標 seeding
    else if (eq("--at") && i + 2 < argc) { at_x = std::atoi(argv[++i]); at_y = std::atoi(argv[++i]); }  // 玩家落點(測試)
    else if (eq("--press") && i + 1 < argc) press = std::toupper((unsigned char)argv[++i][0]);  // 模擬按鍵(測試)
    else if (eq("--keys") && i + 1 < argc) keys_seq = argv[++i];   // headless 逐幀注入輸入序列
    else if (eq("--dump-frame") && i + 1 < argc) dump_frame = std::atoi(argv[++i]);  // 迴圈第 N 幀再 dump
    else if (eq("--msg-page") && i + 1 < argc) msg_page = std::atoi(argv[++i]);   // 訊息檢視先翻到第 N 頁再 dump
    else if (eq("--read-para") && i + 1 < argc) read_para = std::atoi(argv[++i]); // 直接開段落 N 進捲動 overlay
    else if (eq("--para-scroll") && i + 1 < argc) para_scroll = std::atoi(argv[++i]); // dump 前逐頁下捲 N 次
    else if (eq("--char-sheet") && i + 1 < argc) char_sheet = std::atoi(argv[++i]); // 直接開第 N 名角色屬性表
    else if (eq("--inventory")) show_inventory = true;                               // 配合 --char-sheet 開物品欄
    else if (eq("--alloc")) alloc_open = true;                                       // 配合 --char-sheet 進 X 配點模式
    else if (eq("--shop")) shop_open = true;                                         // headless 開商店買賣
    else if (eq("--recruit")) recruit_open = true;                                   // headless 開酒館招募
    else if (eq("--gold") && i + 1 < argc) grant_gold = std::atoi(argv[++i]);        // 設隊伍第 0 名 gold[81]
    else if (eq("--demo-grow")) demo_grow = true;                                    // 注入 AP + 範例物品(成長 UI 出圖)
    else if (eq("--load") && i + 1 < argc) load_path = argv[++i];        // 啟動讀檔還原
    else if (eq("--save-path") && i + 1 < argc) { save_path = argv[++i]; save_path_set = true; }   // 覆寫存/讀檔路徑
    else if (eq("--selftest-save")) selftest_save = true;               // round-trip 自測
    else if (eq("--viewport")) viewport_mode = true;   // 顯示原版 viewport 靜態框架
    else if (eq("--fp")) fp_mode = true;               // 第一人稱 viewport(預設;保留旗標相容)
    else if (eq("--map2d")) fp_mode = false;           // 切回俯視彩格 overview(除錯/對拍用)
    else if (eq("--encounter") && i + 1 < argc) encounter_id = std::atoi(argv[++i]);  // 進遭遇畫面(怪物 index)
    else if (eq("--combat-seed") && i + 1 < argc) combat_seed = (unsigned)std::strtoul(argv[++i], nullptr, 0);
    else if (eq("--combat-rounds") && i + 1 < argc) combat_rounds = std::atoi(argv[++i]);  // dump 前自動打 N 回合
    else if (eq("--combat-count") && i + 1 < argc) combat_count = std::atoi(argv[++i]);    // 怪群數量
    else if (eq("--combat-special") && i + 1 < argc) combat_special = argv[++i];           // 特殊攻擊驗證
    else if (eq("--cast") && i + 1 < argc) cast_spell_id = (int)std::strtoul(argv[++i], nullptr, 0);  // headless 施放法術
    else if (eq("--cast-force")) cast_force = true;     // 即使未習得也施放(驗證效果)
    else if (eq("--terrain-cast") && i + 1 < argc) terrain_cast_id = (int)std::strtoul(argv[++i], nullptr, 0);  // headless 探索地形施法
    else if (eq("--trap-probe")) trap_probe = true;                                  // headless 觸發真實陷阱(驗證)
    else if (eq("--newgame")) newgame = true;           // 啟動即進建角畫面
    else if (eq("--newgame-demo") && i + 1 < argc) newgame_demo = argv[++i];  // 腳本化建角(headless)
    else if (eq("--newgame-screen") && i + 1 < argc) newgame_screen = argv[++i];  // 停在配點畫面截圖
    else if (eq("--fight-namtar")) fight_namtar = true;   // 終戰 Namtar(隊伍 vs Boss)
    else if (eq("--ending")) show_ending = true;          // 直接進結局序列(demo)
    else if (eq("--ending-idx") && i + 1 < argc) { show_ending = true; ending_at = std::atoi(argv[++i]); }  // 結局從第 N 張過場起(截圖)
    else if (eq("--no-bless")) namtar_blessed = false;    // 關閉自由之劍祝福加成
    else if (eq("--mute")) mute = true;                   // 靜音(關音效;CI/headless 安全)
    else if (eq("--title")) want_title = true;            // 強制顯示開機 title splash(火龍之戰 art)
    else if (eq("--no-splash")) no_splash = true;         // 略過 splash 直接進主選單
  }
  if (scale < 1) scale = 1;
  // 安全保險:給了 --dump 卻沒給 frame 限制(--frames/--max-frames)時,headless 會在
  //   dummy SDL 下無限 poll 空轉(曾造成多個 70% CPU 殭屍程序)。預設只跑到 dump 幀 +1 就退。
  if (!dump.empty() && max_frames < 0) {
    max_frames = (dump_frame >= 0 ? dump_frame + 1 : 2);
    std::fprintf(stderr, "note: --dump without --frames → 自動設 max_frames=%d(防無限空轉)\n", max_frames);
  }

  // 存檔目錄解析:未顯式 --save-path 時,改存到「可寫使用者目錄」,而非 cwd 相對 "save/"。
  //   AppImage / macOS .app 的 cwd 是唯讀掛載(squashfs / bundle)→ 寫 cwd 會失敗導致「離開不存檔」。
  //   優先序:DWR_SAVE_DIR → 平台慣例(XDG/APPDATA/App Support)→ HOME/.local/share → cwd "save"。
  if (!save_path_set) {
    namespace fs = std::filesystem;
    std::string base;
    auto env = [](const char* k) -> const char* { const char* v = std::getenv(k); return (v && *v) ? v : nullptr; };
    if (const char* d = env("DWR_SAVE_DIR")) base = d;
#if defined(_WIN32)
    else if (const char* a = env("APPDATA")) base = std::string(a) + "/opendw-remake";
#elif defined(__APPLE__)
    else if (const char* h = env("HOME")) base = std::string(h) + "/Library/Application Support/opendw-remake";
#else
    else if (const char* x = env("XDG_DATA_HOME")) base = std::string(x) + "/opendw-remake";
    else if (const char* h = env("HOME")) base = std::string(h) + "/.local/share/opendw-remake";
#endif
    else base = "save";   // 無 HOME(極少數)→ 回退 cwd
    std::error_code ec; fs::create_directories(base, ec);
    save_path = base + "/slot0.sav";
    std::fprintf(stderr, "save path: %s\n", save_path.c_str());
  }

  // 音效子系統:RAII 開啟(靜音模式不碰實體裝置)。play() 在任何情況皆安全 no-op,
  //   絕不導致初始化失敗或卡住(headless / CI 不依賴音效裝置)。
  //   非靜音時從 bundle/audio 載真實 PCM 取樣(Amiga data5/6、X68000 DW.SND);缺檔退回方波。
  //   真值層級見 src/audio/sound.hpp 檔頭(func_5060 索引/dx/bx = oracle 真值;事件↔樣本對映 = remake 設計)。
  audio::Sound g_sound;
  g_sound.open(mute, bundle + "/audio");

  auto font = render::Font8x8::load_table(font_raw);
  if (!font) { std::fprintf(stderr, "font load failed: %s\n", font_raw.c_str()); return 1; }
  const bool scene_mode = !scene_name.empty();
  const bool sprite_mode = !sprite_name.empty();
  const bool encounter_mode = encounter_id >= 0 || fight_namtar;
  const bool automap_mode = automap_area >= 0;
  const bool ending_mode = show_ending;   // --ending:直接進結局序列(不打 Namtar)
  const bool menu_mode = !scene_mode && !sprite_mode && !viewport_mode &&
                         !encounter_mode && map_area < 0 && !automap_mode &&
                         !ending_mode;
  render::Framebuffer fb;

  // 多國語系:i18n 字串表由 locale 推導,F4 可即時重載切換。
  // 文字渲染走 SDL2_ttf 高解析文字層(雙層,wqy-zenhei 通吃中/日 kana+kanji)。
  //   zh-TW:繁中 TSV;en:passthrough(TSV 無條目 → tr() 回退英文源);
  //   ja:示範日文 TSV(其餘回退英文)。缺檔不崩潰,退回空表(全英文 passthrough)。
  const bool locale_overridden = !menu_tsv.empty();   // 顯式 --menu 則不隨 F4 改
  // 目前語系字串表(tr() 查無 → 回退英文)+ 段落書,皆隨 locale 重載。
  i18n::Strings tr;
  std::optional<res::ParagraphBook> book;
  std::string locale_tag;   // 角落指示用(繁中 / EN / 日)
  std::string lang_label = "繁中";   // 視窗標題用語系短標(無括號)
  // 找起始 locale 在清單中的索引(--locale 指定);找不到視為自訂,從 0 開始循環。
  int locale_idx = 0;
  for (std::size_t i = 0; i < locales.size(); ++i)
    if (locales[i] == locale) { locale_idx = (int)i; break; }

  // 載入指定 locale 的字串表(menu + events)+ 段落書。供啟動與 F4 重載共用。
  auto load_locale = [&](const std::string& loc) {
    locale = loc;
    std::string mtsv = locale_overridden ? menu_tsv : ("assets/i18n/" + loc + "/menu.tsv");
    auto loaded = i18n::Strings::load(mtsv);
    tr = loaded ? *loaded : i18n::Strings{};          // 缺檔 → 空表(全英文 passthrough)
    std::string etsv = "assets/i18n/" + loc + "/events.tsv";
    if (tr.merge(etsv))
      std::fprintf(stderr, "i18n: merged %s (total %zu)\n", etsv.c_str(), tr.size());
    std::string ctsv = "assets/i18n/" + loc + "/chars.tsv";   // 角色屬性表標籤(V/X 畫面)
    if (tr.merge(ctsv))
      std::fprintf(stderr, "i18n: merged %s (total %zu)\n", ctsv.c_str(), tr.size());
    std::string fbtsv = "assets/i18n/" + loc + "/combat.tsv";  // 戰鬥畫面 UI + 怪物名
    if (tr.merge(fbtsv))
      std::fprintf(stderr, "i18n: merged %s (total %zu)\n", fbtsv.c_str(), tr.size());
    std::string sptsv = "assets/i18n/" + loc + "/spells.tsv";  // 法術名 + 施法訊息
    if (tr.merge(sptsv))
      std::fprintf(stderr, "i18n: merged %s (total %zu)\n", sptsv.c_str(), tr.size());
    std::string ittsv = "assets/i18n/" + loc + "/items.tsv";  // 物品類型名 + 背包 UI
    if (tr.merge(ittsv))
      std::fprintf(stderr, "i18n: merged %s (total %zu)\n", ittsv.c_str(), tr.size());
    std::string shtsv = "assets/i18n/" + loc + "/shop.tsv";   // 商店買賣 + 酒館招募 UI + curated 物品名
    if (tr.merge(shtsv))
      std::fprintf(stderr, "i18n: merged %s (total %zu)\n", shtsv.c_str(), tr.size());
    // Read paragraph 段落書(隨 locale);缺檔則回退「Read paragraph N」。
    book = res::ParagraphBook::load(bundle + "/paragraphs", loc);
    if (book) std::fprintf(stderr, "paragraphs: loaded %zu (locale=%s)\n", book->size(), loc.c_str());
    else std::fprintf(stderr, "paragraphs: none for locale=%s (fallback to 'Read paragraph N')\n", loc.c_str());
    // 語系可讀短標(視窗標題用,無括號)+ 角落指示(相容保留)。
    if (loc == "zh-TW") lang_label = "繁中";
    else if (loc == "en") lang_label = "EN";
    else if (loc == "ja") lang_label = "日";
    else lang_label = loc;
    locale_tag = "[" + lang_label + "]";
    std::fprintf(stderr, "locale = %s %s\n", loc.c_str(), locale_tag.c_str());
  };
  load_locale(locales.empty() ? locale : locales[locale_idx]);

  std::string header, header_en;   // header_en = 提示英文源(F4 重譯)
  std::vector<Opt> opts;
  int sel = 0;
  enum { S_TITLE, S_MENU, S_BRANCH, S_GAME, S_COMBAT, S_MAP, S_CREATE, S_ENDING } state = S_MENU;

  // ── UI 主題(theme-aware presentation;見 render/ui_theme.hpp)──
  //   title art / 戰鬥 backdrop / 訊息框配色等隨主題切換。目前 DOS 唯一主題;
  //   未來 PC-98 / Amiga / X68000 各自一個主題實例 + 對應資產,呼叫端不需改。
  //   theme 為**值複製**(非 const&),F8 重設 theme_idx 後 reseat,各 draw_* lambda
  //   以 [&] 捕獲 theme 變數,reassign 即時生效(畫面下幀重繪自動套用)。
  int theme_idx = 0;                                  // 當前主題索引(state 記住;F8 循環)
  // --theme NAME:啟動即選定主題索引(否則預設 DOS=0)。供 headless 直接 dump Amiga 戰鬥等。
  if (!start_theme.empty()) {
    for (int ti = 0; ti < render::theme_count(); ++ti)
      if (render::theme_by_index(ti).name == start_theme) { theme_idx = ti; break; }
  }
  render::UiTheme theme = render::theme_by_index(theme_idx);
  int theme_toast = 0;                                // F8 後短暫顯示主題名的剩餘幀數(0=不顯示)
  // ── F1 Help 覆蓋層 / F10·ESC 離開確認 ──
  bool help_active = false;                           // F1 開啟的 Help 覆蓋層
  bool confirm_quit_active = false;                   // 離開確認視窗(F10 / 頂層 ESC 觸發,已自動存檔)
  // 開機 title splash 用的 dragon art。實際載入見下方 load_title_art lambda(需 vid.set_palette,
  //   故定義在 vid 建立後)。啟動載一次到獨立 framebuffer,splash 期間每幀 blit(不重解碼);
  //   F8 切主題時 reload(對應主題的 art + palette)。載失敗則 splash 退回藍底 + 標題字。
  render::Framebuffer title_fb;
  bool title_art_ok = false;
  // ── 結局過場序列狀態(S_ENDING)──
  //   結局先放主題的全螢幕過場場景(DOS res 24..28:Namtar 墜淵/慘叫/焚城/和平/The End),
  //   每張配在地化敘事字(疊在壓暗襯底條);場景間插入 bundled 段落捲動(ParaViewer)。
  //   ending_phase:0=場景過場、1=段落捲動、2=末張 The End。ending_idx=當前場景索引。
  render::Framebuffer ending_fb;       // 當前結局場景已解碼的像素層(每次推進重解碼)
  bool ending_fb_ok = false;           // 解碼成功(否則退黑底)
  int ending_idx = 0;                  // 當前結局場景索引(0-based)
  int ending_phase = 0;                // 0=場景過場,1=段落捲動,2=末張 The End
  std::array<render::Rgb, 16> ending_pal = render::kDosPalette;  // 末次載入的場景 palette(Amiga 用檔頭盤)
  std::string branch_label, branch_label_en;   // branch 英文源(F4 重譯)

  // F4 切語系後,用各 widget 暫存的英文源重新 tr() 在地化(選單/branch/事件)。
  auto relocalize = [&]() {
    if (!header_en.empty()) header = tr.tr(header_en);
    for (auto& o : opts) o.label = tr.tr(o.en);
    if (!branch_label_en.empty()) branch_label = tr.tr(branch_label_en);
    // 事件文字:重跑該關事件腳本(在地化來源已換)。其餘畫面即時重繪自動套用。
  };
  int px = 0, py = 0, dir = 1;     // 玩家位置/朝向(0=N,1=E,2=S,3=W)
  const int dx4[4] = {0, 1, 0, -1}, dy4[4] = {-1, 0, 1, 0};
  const char* dirch = "^>v<";
  std::optional<res::Level> level;
  // 預設 4 人隊伍(Muskels/Theb/Elendil/Cheetah),自包含 bundle 資產;進遊戲即顯示在右側面板。
  game::Party party = game::Party::load_default(bundle);
  // --demo-items:把 bundle/items/items.bin 的萃取物品注入角色 0 的物品欄(UI 展示用)。
  //   起始隊伍物品欄全空 → 無此旗標時背包 UI 正確顯示「無物品」;此旗標僅供出圖驗證。
  if (show_inventory && party.size() > 0) {
    std::string ip = bundle + "/items/items.bin";
    std::FILE* f = std::fopen(ip.c_str(), "rb");
    if (f) {
      std::fseek(f, 0, SEEK_END); long n = std::ftell(f); std::fseek(f, 0, SEEK_SET);
      std::vector<std::uint8_t> blob(n > 0 ? (size_t)n : 0);
      if (!blob.empty() && std::fread(blob.data(), 1, blob.size(), f) == blob.size() &&
          blob.size() >= 10 && blob[0]=='D'&&blob[1]=='W'&&blob[2]=='I'&&blob[3]=='T'&&blob[4]=='M') {
        std::uint16_t cnt = (std::uint16_t)(blob[8] | (blob[9] << 8));
        auto recs = party.raw_records();
        size_t off = 10;
        for (std::uint16_t k = 0; k < cnt && k < 13 && off + 4 + 23 <= blob.size(); ++k) {
          off += 4;  // skip data1_off
          int slot_base = 236 + (int)k * 23;
          for (int b = 0; b < 23; ++b) recs[0][slot_base + b] = blob[off + b];
          recs[0][slot_base + 0] |= 0x01;  // 標為已裝備(UI 展示「已裝備」標記)
          off += 23;
        }
        party = game::Party::from_raw_records(recs);
        std::fprintf(stderr, "demo-items: injected %u items into character 0 inventory\n", cnt);
      }
      std::fclose(f);
    }
  }
  // --demo-grow:給角色 0 注入成長點數 + 範例「可使用 / 可裝備」物品(成長 UI 出圖用)。
  //   AP=4(模擬升 2 級後可花);slot0=回法力藥水(Use 回 Power);slot1=劍(可裝備,AV+2)。
  //   bit 佈局對齊 equipment.cpp parse_item(byte0 equipped/charges、byte5 type、byte6/7 magic、byte3 av/ac)。
  if (demo_grow && party.size() > 0) {
    auto recs = party.raw_records();
    recs[0][59] = 4;  // AP=4
    auto put = [&](int slot, std::uint8_t b0, std::uint8_t type, std::uint8_t mhi,
                   std::uint8_t mlo, std::uint8_t avac, const char* name) {
      int base = 236 + slot * 23;
      for (int b = 0; b < 23; ++b) recs[0][base + b] = 0;
      recs[0][base + 0] = b0;            // bit0 equipped + bit3-7 charges
      recs[0][base + 3] = avac;          // bit24-27 av_mod + bit28-31 ac_mod
      recs[0][base + 5] = type;          // type 低 5 bit
      recs[0][base + 6] = mhi;           // magic hi
      recs[0][base + 7] = mlo;           // magic lo
      int n = 0; while (name[n]) ++n;
      for (int i = 0; i < n && i < 12; ++i) {
        std::uint8_t bb = (std::uint8_t)(name[i] & 0x7F);
        if (i + 1 < n && i + 1 < 12) bb |= 0x80;
        recs[0][base + 11 + i] = bb;
      }
    };
    put(0, 0x08 /*charges=1*/, 0x00, 0x84, 30, 0x00, "Power Potion");   // 回法力 30
    put(1, 0x00 /*未裝備*/,    0x05, 0x00, 0x00, 0x02, "Iron Sword");   // 劍,AV+2
    party = game::Party::from_raw_records(recs);
    std::fprintf(stderr, "demo-grow: char 0 AP=4 + Power Potion + Iron Sword\n");
  }
  // 怪物表(res31 萃取,oracle 對拍 25 筆);遭遇畫面用。
  std::vector<game::MonsterRecord> monsters = game::MonsterTable::load(bundle);
  int level_res = -1;             // 當前關卡資源 index(= area + 0x46;= word_3AE8)
  int current_area = -1;          // 當前所在區域(存檔用;= level_res - 0x46)
  // 俯視地圖 fog of war:per-area「已看過」格;玩家每步標記當前格(對齊
  // opendw refresh_viewport,engine.c:5688)。存檔保存;換 area 各關獨立。
  game::SeenMap seen;
  // 探索互動狀態(門開啟/密門粉碎/陷阱解除·觸發/陷阱感知);per-area(x,y)旗標。
  // 存檔保存;.lvl 只讀,不就地改 byte(同 SeenMap;見 docs/gameplay/57_DOORS_TRAPS_TERRAIN.md)。
  game::TerrainState terrain;
  // 真實陷阱格(per-area;路徑 B:VM 跑事件格 script→傷害訊息語意類 → 原版真陷阱座標)。
  //   每次進關由 enter_map 以當前 .lvl 重算(位置=原版真值,見 real_terrain.hpp)。
  game::RealTraps real_traps;
  // 持久 VM 遊戲狀態(對拍 opendw game_state.unknown[256]):跨事件保留,存檔/讀檔的核心欄位。
  // run_event 跑事件腳本時以此為初值並回寫,使旗標(門/開關/劇情)能持久累積。
  std::array<std::uint8_t, 256> game_state{};
  std::string event_msg;          // 踩到事件格時跑 script emit 的文字(原文,F4 重排用)
  int last_event_tile = -1;       // 對拍 op_71:tile 值變了才觸發
  // ── 寶箱開箱(grounded)──────────────────────────────────────────────────
  //   原版寶箱 = tile 事件 emit「locked chest」;開鎖→給物品在 script 11 深度糾纏
  //   (lock 機率 vs 物品 id 共用 gs[0x41],byte-exact 屬深層 RE)。grounded 設計:踩到
  //   寶箱格 → K 開鎖檢定(remake try_lockpick)→ 成功給一件**真實 Dragon Wars 物品**
  //   (items.bin 物品池,依位置決定性選一件)→ add_item 持久;每箱限開一次。
  std::vector<std::array<std::uint8_t, 23>> chest_pool;   // items.bin 物品記錄池
  std::set<long> opened_chests;   // 已開寶箱(key=area*1e6+x*1e3+y),避免重複給
  bool chest_here = false;        // 當前格是未開的寶箱(K 可開)
  {  // 載入 items.bin 物品池(寶箱給物品來源;與 --demo-items 同格式)。
    std::string ip = bundle + "/items/items.bin";
    if (std::FILE* f = std::fopen(ip.c_str(), "rb")) {
      std::fseek(f, 0, SEEK_END); long n = std::ftell(f); std::fseek(f, 0, SEEK_SET);
      std::vector<std::uint8_t> blob(n > 0 ? (std::size_t)n : 0);
      if (!blob.empty() && std::fread(blob.data(), 1, blob.size(), f) == blob.size() &&
          blob.size() >= 10 && blob[0]=='D'&&blob[1]=='W'&&blob[2]=='I'&&blob[3]=='T'&&blob[4]=='M') {
        std::uint16_t cnt = (std::uint16_t)(blob[8] | (blob[9] << 8));
        std::size_t off = 10;
        for (std::uint16_t k = 0; k < cnt && off + 4 + 23 <= blob.size(); ++k) {
          off += 4;  // skip data1_off
          std::array<std::uint8_t, 23> rec{};
          for (int b = 0; b < 23; ++b) rec[(std::size_t)b] = blob[off + b];
          chest_pool.push_back(rec);
          off += 23;
        }
      }
      std::fclose(f);
    }
  }

  // ── grounded quest 物品給予表(攻略驅動)──────────────────────────────────────
  //   取得邏輯藏在 op_8C 確認 + 未完整逆出的共享 script(op_68/op_70 NULL),無法可靠自動抽取,
  //   故依《軟體世界》攻略人工編目「地點 → 物品」(assets/bundle/quest/grants.tsv)。
  //   觸發:首次進入該區且隊伍尚未持有該物品 → 給真實 quest 物品(中文名走 items.tsv)並持久。
  //   誠實標示:給予為 grounded 設計,時機簡化為「進區即得」(原版多需先完成該區子任務)。
  struct QuestGrant { int tile; std::string name; };       // tile=-1:進區即給
  std::multimap<int, QuestGrant> quest_grants;             // area → grant(可多件)
  {
    std::ifstream gf(bundle + "/quest/grants.tsv");
    std::string line;
    while (std::getline(gf, line)) {
      if (line.empty() || line[0] == '#') continue;
      std::vector<std::string> f; std::string cur;
      for (char c : line) { if (c == '\t') { f.push_back(cur); cur.clear(); } else cur.push_back(c); }
      f.push_back(cur);
      if (f.size() >= 3 && !f[2].empty())
        quest_grants.emplace(std::atoi(f[0].c_str()), QuestGrant{std::atoi(f[1].c_str()), f[2]});
    }
    if (!quest_grants.empty())
      std::fprintf(stderr, "quest grants: %zu entries loaded\n", quest_grants.size());
  }
  // 把英文物品名編成 23B 物品記錄(header 全 0 = 一般物品 / 未裝備;name 走 7-bit 高位元終止編碼,
  //   對拍 read_item_name:非末字 |0x80、末字清高位元;offset 0x0B 為名首字 → 非 0 表示有物品)。
  int quest_grant_pending = -1;   // >=0:該 area 待檢查 quest 物品給予(換區後延一輪,避開事件框)
  auto make_quest_item = [](const std::string& name) -> std::array<std::uint8_t, 23> {
    std::array<std::uint8_t, 23> rec{};
    rec[6] = 0x80;   // magic_hi=0x80「無魔法效果」→ 11B header 非零 → parse_item present=true(否則背包不顯示);無副作用
    std::size_t n = std::min<std::size_t>(name.size(), 12);
    for (std::size_t i = 0; i < n; ++i) {
      std::uint8_t b = (std::uint8_t)(name[i] & 0x7F);
      if (i + 1 < n) b |= 0x80;                            // 非末字 → 還有字
      rec[11 + i] = b;
    }
    return rec;
  };

  MsgViewer msg;                  // 一般事件訊息檢視器(下半部分頁;active 時暫停移動)
  ParaViewer para;                // Read Paragraph 長段落捲動檢視器(全螢幕 overlay;active 時暫停移動)
  CharSheet sheet;                // 角色屬性表檢視子狀態(V / 數字 1-4 進;active 時暫停移動)
  CharGenUi cg;                   // 新遊戲建角流程(選單 B → S_CREATE;見 CharGenUi)
  ShopUi shop_ui;                 // 商店買賣子狀態(P / 踩商店格 / --shop;active 時暫停移動)
  TavernUi tavern_ui;             // 酒館招募子狀態(T / 踩酒館格 / --recruit;active 時暫停移動)
  ExploreCast cast_ui;            // 探索施法子狀態(C / --terrain-cast;active 時暫停移動)
  ReorderUi reorder_ui;           // 重排隊伍子狀態(O / --reorder;active 時暫停移動)
  game::Shop shop_data = game::Shop::load(bundle);  // 商店庫存(bundle/shop/stock.json;自包含)
  // 遊戲內 UI chrome(石磚邊框 + Dragon Wars logo + pillar)原版資源(bundle/viewport/ui_pieces.bin;
  // byte-for-byte 同 DRAGON.COM com 0x6AE0,對拍 opendw ui_load)。載入失敗 → 退回文字/實線近似。
  std::optional<render::UiPieces> ui_pieces =
      render::UiPieces::load(bundle + "/viewport/ui_pieces.bin");
  // run_event 攔到「Read paragraph N」時,把 N 寫進此處(>=0 表示本次事件是段落觸發);
  // main 偵測後改開 ParaViewer(長段落捲動)而非一般訊息框。-1 = 非段落事件。
  int event_para_n = -1;
  // 本次事件是否為「上鎖寶箱」(在 message_sink 比對英文原文設定,locale 無關;
  // 不可改用翻譯後的 event_msg 比對 "locked chest" —— zh-TW 下會永遠落空)。
  bool event_is_chest = false;

  // ── 遭遇 / 戰鬥畫面狀態(S_COMBAT)──
  // 怪物圖渲染對齊 oracle:畫進 160×136 viewport 區、blit 到 framebuffer (16,8)
  //   (對照 opendw show_random_encounter → draw_random_encounter_graphic →
  //    ui_update_viewport,ui.c:1254 / 517;viewport 0x50×0x88 @ get_line_offset+0x10)。
  // 結算數值維持「乾淨室模型」(combat.hpp 已誠實標示),非 oracle 移植。
  struct EncounterState {
    bool active = false;
    int monster_idx = -1;                 // monsters[] index
    std::optional<render::Sprite> sprite;  // 怪物圖(bundle .spr;無則畫空框)
    std::string mon_name_en;               // 怪物名英文原文(i18n 鍵)
    int sprite_res = -1;                    // 怪物 sprite 資源編號(MonsterRecord::sprite_res();F8 reload 用)
    game::Combatant hero, mon;             // 結算單位(hero=隊伍第 0 名)— 施法(C)路徑沿用
    game::CombatRng rng{0x1234};
    std::vector<std::string> log;          // 逐回合戰鬥訊息(英文鍵化;tr 在地化)
    bool fled = false;                     // 已逃跑
    bool over = false;                     // 戰鬥結束(怪死 / 英雄死 / 逃跑)
    // ── 完整戰鬥迴圈(4 人 vs 怪群、多回合、勝負、XP)──
    //   group=true 時走 CombatLoop(group_loop);false(施法 demo 等)走舊單怪 hero/mon。
    bool group = false;                    // 是否為怪群戰鬥(--encounter 預設 true)
    int mon_count = 1;                     // 怪群數量
    std::optional<game::CombatLoop> group_loop;  // 群戰結算迴圈
    std::size_t shown_events = 0;          // 已轉成戰報文字的事件數(逐回合追加)
    bool xp_awarded = false;               // 勝利 XP 是否已寫回(避免重複加)
    bool victory = false;                  // 勝利(怪群全滅)
    bool defeat = false;                   // 敗北(全隊昏倒)
    // ── 施法(C 鍵)──
    // hero 對應隊伍第 0 名;施法需 Power/STR/已習得法術 → 由該角色 record 帶入。
    int hero_power = 0;                    // 施法者當前法力(扣 Power 後寫回此處)
    int hero_str = 0;                      // 施法者 STR(PowerScaled/buff 結算)
    int hero_int = 0;                      // 施法者 INT(Zap 攻擊判定;docs/gameplay/58_MAGIC_REFERENCE.md 門檻 12+ranks+INT−DV)
    int hero_ranks = 0;                    // 魔法技能 ranks 估計(Zap 判定 + var. 上限 2×ranks)
                                           //   受阻:技能槽→法術系對映未反編出,以 level 為保守 proxy(誠實標示)
    std::vector<std::uint8_t> spellbook;   // 已習得且當前可施法的法術 id(castable_spells)
    bool casting = false;                  // 施法選單開啟中(C 進;上下選;Enter 施放;Esc 取消)
    int cast_sel = 0;                      // 施法選單游標
    bool is_namtar = false;                // 終戰 Namtar:勝利時進結局序列(非一般遭遇)
    // ── 怪物立繪動畫(idle 呼吸 + 受擊閃白)──
    //   Amiga / DOS sprite 均為單格靜態立繪(data4 多格佈局逆向結論:每資源僅 1 frame 真資料,
    //   其餘為 fill padding;見 docs/reference/61 §1.5)→ 無真動畫格可循環。
    //   故以單格做程序化「活化」:idle 微幅上下浮動(呼吸),命中時短暫閃白(受擊)。不偽造假格。
    int hit_flash = 0;                      // >0:受擊閃白剩餘幀數(每幀遞減;append 事件時設)
  } enc;

  // 事件腳本跨資源 call(op_58)的資源提供者:從 bundle 載(自包含,不需 DATA1)。
  // tag = DATA1 section;BundleProvider 讀 assets/bundle/scripts/<tag>.bin(解壓後)。
  // 事件 script 經 op_58 載入的 tag 聯集已預先抽進 bundle(見 manifest event_script_tags)。
  res::BundleProvider event_provider(bundle);

  // 段落書 book 已於 load_locale 載入(隨 F4 切語系重載)。

  // 踩到特殊格 → 跑該關事件腳本,回傳 emit 的文字(對拍 opendw op_71/run_level_script)。
  // 對齊 level_events:設 script_res/data_res = level_res,並掛 resource_provider 讓 op_58 能跑。
  auto run_event = [&](std::uint8_t tv) -> std::string {
    event_para_n = -1;            // 預設:本次非段落事件(命中 op_81 數字 sink 才設)
    event_is_chest = false;       // 預設:本次非寶箱(sink 比對英文原文才設)
    if (!level) return "";
    // ── 世界圖 / 樞紐「踩格進區」(DRAGON.COM 反組譯反推,opendw 未實作)──────
    //   area 0 Dilmun 世界圖的城鎮/地點格事件腳本固定走 op_58→資源8→op_6x<IDX>,
    //   其中 IDX = 目的地 area。resource 8 的 VM 子常式 remake 尚無法忠實執行
    //   (op_68/op_70 在 opendw 為 NULL,resource 8 控制流未完整逆出),故此處
    //   以靜態反推的 IDX 直接設 gs[2]=目的地 area,交給 sync_relocation 換場。
    //   入口座標未能靜態逆出 → 落點由 enter_map 取目標關卡第一可走格(連通正確)。
    //   只在世界圖類樞紐(wrap 關卡)套用,避免誤判一般關卡的事件格。
    if (level->wraps()) {
      int dest = level->worldmap_dest(tv);
      if (dest >= 0 && dest != current_area) {
        std::fprintf(stderr,
            "worldmap enter: area %d tile 0x%02X -> area %d "
            "(DRAGON.COM RE; opendw unimpl)\n", current_area, tv, dest);
        game_state[2] = (std::uint8_t)dest;   // sync_relocation 會載入並落到第一可走格
        game_state[0] = game_state[1] = 0;    // 入口座標未逆出 → 交 enter_map 取首格
        return "";
      }
    }
    std::uint16_t pc = level->script_pc(tv);
    if (pc == 0 || pc >= level->data().size()) return "";
    vm::VmState st;
    st.script = level->data();
    st.data_bytes = level->data();
    st.script_res = level_res;
    st.data_res = level_res;
    st.pc = pc;
    st.game_state = game_state;   // 以持久遊戲狀態為初值(旗標跨事件累積)
    // 對拍 op_71:腳本可能讀玩家位置/朝向(gs[0]/gs[1]/gs[3])來決定分支。
    // 進腳本前同步當前 px/py/dir 與 gs[2]=current_area,跑完由 sync_relocation 比對回寫。
    st.game_state[0] = (std::uint8_t)px; st.game_state[1] = (std::uint8_t)py;
    st.game_state[2] = (std::uint8_t)current_area; st.game_state[3] = (std::uint8_t)dir;
    // ── 角色狀態同步(事件 ↔ 隊伍 records)──────────────────────────────────────
    //   原版事件對「角色資料」的修改(op_64 給物品、op_5F 設祝福旗標)寫進 data_C960/
    //   data_CA4C;remake 的 party 真值存於各 512B record。先把 party records 載進 VM
    //   char_data(member i → record i,selector=i*2),並設角色 context(gs[6]/gs[0x1F])
    //   讓 op_64 的隊伍迴圈與定址正確;背包(record 內偏移 0xEC 起)鏡射進 char_ext
    //   (原版 data_CA4C = data_C960 + 0xEC 重疊)。事件跑完再同步回 party(見 ip.run() 後)。
    auto evt_recs = party.raw_records();
    const int evt_psz = (int)evt_recs.size();
    st.game_state[0x1F] = (std::uint8_t)evt_psz;     // 隊伍人數(op_64 等 party 迴圈用)
    st.game_state[6] = 0;                            // 當前角色預設第 0 名
    for (int i = 0; i < evt_psz && i < 7; ++i) {
      st.game_state[(0x0A + i) & 0xFF] = (std::uint8_t)(i * 2);   // selector = record_index*2
      for (int b = 0; b < 512; ++b)
        st.char_data[(std::size_t)i * 512 + b] = evt_recs[(std::size_t)i][(std::size_t)b];
    }
    // 背包重疊窗:char_ext[k] ≡ char_data[0xEC + k]。
    for (std::size_t k = 0; k + 0xEC < st.char_data.size() && k < st.char_ext.size(); ++k)
      st.char_ext[k] = st.char_data[k + 0xEC];
    // op_58 / 子 script / op_0F 跨資源讀:依 tag 從 bundle 載(自包含)。
    // 註:BundleProvider 現已能自行把 level-self tag(area+0x46)解析成 maps/*.lvl,
    //   所以下面的 `tag == level_res` 只是「直接用已載入的 level bytes」的快取捷徑
    //   (byte-for-byte 等同 event_provider.load(level_res)),省一次檔案讀取。
    st.resource_provider =
        [&](int tag) -> std::optional<std::vector<std::uint8_t>> {
      if (tag == level_res) return level->data();
      return event_provider.load(tag);
    };
    vm::Interpreter ip(st);
    std::string out;
    bool read_para_pending = false;   // 上一段 emit 是「Read paragraph 」前綴
    // 逐段 emit 個別在地化(tr 以單條英文原文為鍵;查不到回退英文),再以空白接起。
    // 對拍 op_71 的多條 emit:整句拼接前先翻譯,避免「拼好的長句」查不到鍵。
    //
    // 防拷段落內嵌:op_78 emit「Read paragraph 」字串 → op_81 emit 段落號 N
    // (offset == kNumberSink)。攔到 N → 從段落書取繁中原文取代整條訊息;
    // 無段落書或查無 N 則回退顯示「Read paragraph N」(對齊原版防拷字樣)。
    ip.set_message_sink([&](std::size_t offset, const std::string& s) {
      if (s.empty()) return;
      if (offset == vm::Interpreter::kNumberSink) {
        if (read_para_pending) {
          read_para_pending = false;
          int n = std::atoi(s.c_str());
          event_para_n = n;                                // 記下段落號(main 用以開捲動 overlay)
          std::optional<std::string> para;
          if (book) para = book->text(n);
          if (para) { out = *para; }                       // 顯示段落繁中原文
          else { out += s; }                               // 回退:「Read paragraph N」
          return;
        }
        if (!out.empty()) out += ' ';
        out += s;                                           // 一般數字輸出(非段落)
        return;
      }
      std::string t = tr.tr(s);
      // 偵測「Read paragraph 」前綴(原文判定,翻譯前):此後緊接的數字即段落號。
      if (s.rfind("Read paragraph", 0) == 0) read_para_pending = true;
      // 寶箱偵測走英文原文(locale 無關):事件 emit 含 "locked chest" → 本格為上鎖寶箱。
      if (s.find("locked chest") != std::string::npos) event_is_chest = true;
      if (!out.empty()) out += ' ';
      out += t;
    });
    // op_90(op_sound_effect)dispatch:func_5060 索引 → audio::SoundId → 播放。
    //   VM 不直接相依 audio;由此 sink 轉接(對照 opendw dispatch_sound_effect)。
    ip.set_sound_sink([&](int idx) {
      audio::SoundId id;
      if (audio::dispatch_index_to_sound(idx, id)) g_sound.play(id);
    });
    ip.run();
    game_state = st.game_state;   // 回寫:事件對遊戲狀態的修改持久保留
    // ── 角色狀態回寫(事件給物品 / 設祝福 → 持久進 party records)──────────────
    //   背包窗 char_ext → char_data[0xEC+],再逐欄比對寫回各 512B record;只有實際變動
    //   才重建 party(flavor 事件不動 char_data → 無變動 → 不重建,零開銷 / 無副作用)。
    {
      for (std::size_t k = 0; k + 0xEC < st.char_data.size() && k < st.char_ext.size(); ++k)
        st.char_data[k + 0xEC] = st.char_ext[k];
      bool char_changed = false;
      for (int i = 0; i < evt_psz && i < 7; ++i)
        for (int b = 0; b < 512; ++b)
          if (evt_recs[(std::size_t)i][(std::size_t)b] != st.char_data[(std::size_t)i * 512 + b]) {
            evt_recs[(std::size_t)i][(std::size_t)b] = st.char_data[(std::size_t)i * 512 + b];
            char_changed = true;
          }
      if (char_changed) {
        party = game::Party::from_raw_records(evt_recs);
        std::fprintf(stderr, "event: party char_data changed (item/blessing persisted)\n");
      }
    }
    return out;
  };

  // 取段落 N 的顯示全文,含跨語系回退:目前 locale 的段落書有 → 用之;沒有(段落
  // 目前僅 zh-TW 有資料)→ 回退載 zh-TW 段落書取全文。標題「段落 N」走 i18n,故即使
  // 內文是 zh-TW,在 en/ja 下標題仍會在地化。回傳空 = 連 zh-TW 都查無此段。
  std::optional<res::ParagraphBook> zh_book_fallback;  // 惰性載入的 zh-TW 段落書(回退用)
  auto para_text = [&](int n) -> std::string {
    if (book) { if (auto t = book->text(n)) return *t; }
    if (locale != "zh-TW") {
      if (!zh_book_fallback) zh_book_fallback = res::ParagraphBook::load(bundle + "/paragraphs", "zh-TW");
      if (zh_book_fallback) { if (auto t = zh_book_fallback->text(n)) return *t; }
    }
    return "";
  };

  // 進入某區地圖:載入真實關卡 .lvl + 找第一個可走格當起點
  auto enter_map = [&](int area) {
    level = res::Level::load_file(bundle + "/maps/" + std::to_string(area) + ".lvl");
    if (!level) { std::fprintf(stderr, "level load failed: area %d\n", area); return false; }
    if (area == 0) level->restore_phoebus_entrance();   // remake 還原:菲巴斯入口 tile 0x07 @(10,4)(原版疏漏未放置;見 docs/gameplay/54 §E)
    level_res = area + 0x46;       // 關卡資源 index(對拍 level_events:word_3AE8)
    current_area = area;
    px = py = 0; dir = 1;
    for (int y = 0; y < level->h && py == 0 && px == 0; ++y)
      for (int x = 0; x < level->w; ++x)
        if (level->tile(x, y) == 1) { px = x; py = y; y = level->h; break; }
    // 同步 VM game_state 的位置/區域欄位(對拍 opendw:gs[0]=X gs[1]=Y gs[2]=area gs[3]=facing)。
    // 換場偵測(sync_relocation)以 gs[2] 為真值,故進場時即建立一致狀態。
    game_state[0] = (std::uint8_t)px; game_state[1] = (std::uint8_t)py;
    game_state[2] = (std::uint8_t)area; game_state[3] = (std::uint8_t)dir;
    std::fprintf(stderr, "enter map area %d: \"%s\" %dx%d start=(%d,%d)\n",
                 area, level->name.c_str(), level->w, level->h, px, py);
    // 進場即把起始格標記 seen(對齊 opendw:進關後第一次 refresh_viewport 標記玩家格)。
    if (level) seen.mark(current_area, px, py, level->w, level->h);
    // 識別本關真實陷阱格(路徑 B:逐事件格跑 VM,傷害訊息語意類 → 原版真陷阱座標)。
    //   位置=原版真值;觸發傷害=remake 設計(見 real_terrain.hpp / docs/gameplay/57_DOORS_TRAPS_TERRAIN.md)。
    real_traps = game::RealTraps::identify(*level, area);
    if (real_traps.count() > 0)
      std::fprintf(stderr, "real traps: area %d -> %zu cell(s)\n", area, real_traps.count());
    quest_grant_pending = area;   // 進區 choke point → 延一輪檢查 grounded quest 物品給予(已持有則跳過)
    return true;
  };
  // 把玩家當前格標記為 seen(對齊 opendw refresh_viewport,engine.c:5688:
  // 每幀只標記玩家站的那一格,非整個視野)。每次移動 / 換場後呼叫。
  auto mark_seen_here = [&]() {
    if (level) seen.mark(current_area, px, py, level->w, level->h);
  };

  // sync_relocation — 跑完事件腳本後,對拍 opendw load_level_resources 的「poll」:
  //   事件腳本(op_71→run_level_script→run_script)用 op_12/op_11 寫 gs[2]=新 area、
  //   gs[0]/gs[1]=入口 X/Y、gs[3]=朝向(逆向證據:probe_areaswitch,area 23→0、area 27 內部傳送)。
  //   opendw 每幀 refresh_viewport→load_level_resources 比對 gs[2] vs gs[0x57],變了就
  //   resource_load(area+0x46)+ read_level_metadata 重載;此處等價地比對 gs[2] vs current_area。
  //
  // 回傳值:0=無變化、1=同區傳送(只挪 px/py/dir)、2=換 area(重載 .lvl)、-1=因 wrap 邊界跳過。
  //
  // 鐵則:opendw 對 boundary flag bit1(gs[0x23]&2)的 wrap 分支與兩張已載入地圖互換
  //   皆 exit(1) 未實作。remake 走乾淨版重載(跳過那個 decompile 缺口),但目標地圖
  //   若標記 wrap(flag&2)則明確跳過 + log,不假裝支援。
  auto sync_relocation = [&]() -> int {
    int old_area = current_area;
    int new_area = game_state[2];
    int gx = game_state[0], gy = game_state[1], gf = game_state[3] & 3;
    if (new_area == current_area) {
      // 同區:事件可能傳送玩家(area 27 樓梯/陷阱)。位置變了才挪。
      if (gx != px || gy != py || gf != dir) {
        if (level && level->in_bounds(gx, gy) ? true : false) { px = gx; py = gy; }
        else { px = gx; py = gy; }   // 越界值保留(對拍:gs 直接寫入,邊界檢查在移動時才夾)
        dir = gf;
        std::fprintf(stderr, "relocate (same area %d) -> (%d,%d) dir=%d\n", current_area, px, py, dir);
        return 1;
      }
      return 0;
    }
    // 換 area:先看目標地圖是否走 wrap 邊界(opendw 未實作 → 跳過)。
    auto dst = res::Level::load_file(bundle + "/maps/" + std::to_string(new_area) + ".lvl");
    if (!dst) {
      std::fprintf(stderr, "area switch %d->%d SKIPPED: target .lvl missing\n", current_area, new_area);
      game_state[2] = (std::uint8_t)current_area;   // 還原,避免反覆觸發
      return -1;
    }
    if (dst->flags & 0x2) {                          // gs[0x23] bit1 = wrap(opendw exit(1))
      // 過去:opendw 對 wrap 邊界 exit(1) 未實作 → 明確跳過。現在 remake 以
      // 標準 modular 環繞慣例支援 wrap 關卡載入/渲染/走動(誠實標示:非 oracle 真值)。
      // 仍重載目標 .lvl;wrap 環繞行為在移動(walkable_wrap)與 FOV(check_map_boundary)生效。
      std::fprintf(stderr, "area switch %d->%d: target is wrap boundary (flag&2); "
                   "loading with modular-wrap convention (opendw exit(1) unimplemented)\n",
                   current_area, new_area);
    }
    // 乾淨重載(等價 load_level_resources 的 resource_load(area+0x46) + read_level_metadata)。
    if (!enter_map(new_area)) {
      game_state[2] = (std::uint8_t)current_area;
      return -1;
    }
    // 套用事件指定的入口座標/朝向(enter_map 預設落在第一可走格,這裡覆寫成腳本值)。
    // 例外:世界圖樞紐換場入口座標未能靜態逆出(gs[0]=gs[1]=0 哨兵)→ 保留
    //   enter_map 取的第一可走格(連通正確,非 byte-exact 入口)。
    bool entry_unknown = (gx == 0 && gy == 0 && !(level && level->walkable(0, 0)));
    if (!entry_unknown) { px = gx; py = gy; }
    dir = gf;
    game_state[0] = (std::uint8_t)px; game_state[1] = (std::uint8_t)py; game_state[3] = (std::uint8_t)dir;
    last_event_tile = -1; event_msg.clear();         // 新區不立即重觸發進入格事件
    std::fprintf(stderr, "AREA SWITCH %d->%d entry=(%d,%d) dir=%d%s%s\n",
                 old_area, new_area, px, py, dir,
                 entry_unknown ? "  [entry coords un-RE'd → first walkable]" : "",
                 (level && !level->in_bounds(px, py)) ? "  [WARN entry out of bounds]" : "");
    return 2;
  };

  // ── 存檔/讀檔(對齊手冊 S=儲存遊戲 / C=繼續舊遊戲)──
  // 把目前完整可還原狀態打包成 SaveState(自包含)。
  auto capture_state = [&]() {
    game::SaveState s;
    s.area = current_area;
    s.x = px; s.y = py; s.facing = dir;
    s.game_state = game_state;
    s.party_records = party.raw_records();
    s.seen_blob = seen.serialize();   // fog of war 探索進度(per-area seen bitmap)
    s.terrain_blob = terrain.serialize();  // 探索互動進度(門/密門/陷阱 per-area 旗標)
    return s;
  };
  // 把 SaveState 套回目前遊戲(重載該 area + 還原位置/朝向/game_state/party)。回傳是否成功。
  auto apply_state = [&](const game::SaveState& s) {
    // 先還原 fog of war,再 enter_map(enter_map 會多標一格起始格,無害);
    // 用副本還原後 enter_map 內 seen.mark 仍會把該關起始格補上。
    if (!s.seen_blob.empty()) {
      if (!seen.deserialize(s.seen_blob.data(), s.seen_blob.size()))
        std::fprintf(stderr, "load: seen bitmap deserialize failed (ignored)\n");
    } else {
      seen.clear();   // v1 舊檔無 seen → 探索進度清空
    }
    if (!s.terrain_blob.empty()) {
      if (!terrain.deserialize(s.terrain_blob.data(), s.terrain_blob.size()))
        std::fprintf(stderr, "load: terrain state deserialize failed (ignored)\n");
    } else {
      terrain.clear();   // v1/v2 舊檔無 terrain → 互動進度清空
    }
    if (s.area < 0 || !enter_map(s.area)) {
      std::fprintf(stderr, "load: invalid/unloadable area %d\n", s.area);
      return false;
    }
    px = s.x; py = s.y; dir = s.facing;
    game_state = s.game_state;
    party = game::Party::from_raw_records(s.party_records);
    last_event_tile = -1; event_msg.clear();   // 不在讀檔當下重觸發事件
    state = S_GAME;
    std::fprintf(stderr, "load applied: area=%d (%d,%d) dir=%d party=%zu\n",
                 s.area, px, py, dir, party.size());
    return true;
  };
  // 存檔到 save_path:打包 → 寫檔。回傳是否成功(供 S 鍵提示)。
  auto do_save = [&]() {
    bool ok = game::save(capture_state(), save_path);
    std::fprintf(stderr, "save -> %s: %s\n", save_path.c_str(), ok ? "OK" : "FAIL");
    return ok;
  };
  // 從 path 讀檔並套用。回傳是否成功(供 --load / 選單 C)。
  auto do_load = [&](const std::string& path) {
    game::SaveState s;
    if (!game::load(path, s)) {
      std::fprintf(stderr, "load <- %s: FAIL (missing/bad)\n", path.c_str());
      return false;
    }
    return apply_state(s);
  };

  // ── 探索互動:門 / 密門 / 陷阱 / 地形法術(remake 設計,grounded 手冊;見 docs/gameplay/57_DOORS_TRAPS_TERRAIN.md)──
  // 確定性 RNG(供 Lockpick 檢定 / 陷阱傷害擲骰;可 seed,headless 可重現)。
  game::CombatRng terrain_rng{0x5117};
  // 隊伍最高 Lockpick 技能等級(0 = 無人會開鎖)。
  auto party_best_lockpick = [&]() -> int {
    int best = 0;
    for (std::size_t i = 0; i < party.size(); ++i) {
      if (party.at(i).status & 0x01) continue;  // 死亡隊員不算
      best = std::max(best, (int)party.at(i).skills[game::progression::kLockpick]);
    }
    return best;
  };
  // K:對面向前方格做開門 / 破密門。回傳語意(供訊息提示)。
  auto open_door_forward = [&]() -> game::DoorAction {
    if (!level) return game::DoorAction::None;
    int fx = px + dx4[dir], fy = py + dy4[dir];
    if (level->wraps()) { fx = level->wrap_x(fx); fy = level->wrap_y(fy); }
    std::uint8_t t = level->tile(fx, fy);
    using DA = game::DoorAction;
    switch (t) {
      case game::TT_DoorClosed:
        if (terrain.has(current_area, fx, fy, game::TF_DoorOpen)) return DA::AlreadyOpen;
        terrain.set(current_area, fx, fy, game::TF_DoorOpen);
        std::fprintf(stderr, "door open @(%d,%d) [sound:door_open]\n", fx, fy);
        g_sound.play(audio::SoundId::DoorOpen);   // func_5060[2] play_sound_door_open
        return DA::Opened;
      case game::TT_DoorLocked: {
        if (terrain.has(current_area, fx, fy, game::TF_DoorOpen)) return DA::AlreadyOpen;
        // Lockpick 檢定(手冊 p33:開鎖進鎖住房間)。難度暫定 10(remake 設計)。
        auto res = game::try_lockpick(
            [&]() -> const game::CharacterRecord& {
              // 取最高 Lockpick 者做檢定;隊伍空則用第 0 名(必有,K 已 gate party>0)。
              std::size_t best_i = 0; int best = -1;
              for (std::size_t i = 0; i < party.size(); ++i) {
                if (party.at(i).status & 0x01) continue;
                if ((int)party.at(i).skills[game::progression::kLockpick] > best) {
                  best = (int)party.at(i).skills[game::progression::kLockpick]; best_i = i;
                }
              }
              return party.at(best_i);
            }(),
            10, terrain_rng);
        if (party_best_lockpick() <= 0 || !res.success) {
          std::fprintf(stderr, "door locked @(%d,%d) lockpick=%d roll=%d (fail)\n",
                       fx, fy, party_best_lockpick(), res.roll);
          return DA::LockedNeedPick;
        }
        terrain.set(current_area, fx, fy, game::TF_DoorOpen);
        std::fprintf(stderr, "door unlocked @(%d,%d) lockpick=%d [sound:door_open]\n",
                     fx, fy, party_best_lockpick());
        g_sound.play(audio::SoundId::DoorOpen);   // func_5060[2] play_sound_door_open
        return DA::Unlocked;
      }
      case game::TT_SecretDoor:
        if (terrain.has(current_area, fx, fy, game::TF_SecretBroken)) return DA::AlreadyOpen;
        terrain.set(current_area, fx, fy, game::TF_SecretBroken);
        std::fprintf(stderr, "secret door smashed @(%d,%d) [sound:door_open]\n", fx, fy);
        g_sound.play(audio::SoundId::DoorOpen);   // func_5060[2] play_sound_door_open
        return DA::SecretBroken;
      case game::TT_Stone:
        // 石牆障礙:K 無法破(需 Soften Stone 法術);提示。
        if (terrain.has(current_area, fx, fy, game::TF_SecretBroken)) return DA::AlreadyOpen;
        std::fprintf(stderr, "stone wall @(%d,%d): need Soften Stone spell\n", fx, fy);
        return DA::StoneBlocked;
      default:
        std::fprintf(stderr, "open door: no door/wall ahead @(%d,%d) tile=0x%02X\n",
                     fx, fy, t);
        return DA::None;
    }
  };
  // 踩到陷阱格(0x33)結算:未解除·未觸發 → 對全隊擲傷害(remake 設計,grounded 手冊
  //   「陷阱」概念;骰式量化)。回傳是否觸發。
  auto trigger_trap_here = [&]() -> bool {
    if (!level || party.size() == 0) return false;
    // 陷阱格判定:優先真實陷阱(路徑 B,原版座標);相容保留 0x33 測試關 tile。
    if (!real_traps.is_trap(px, py) && level->tile(px, py) != game::TT_Trap) return false;
    if (terrain.has(current_area, px, py, game::TF_TrapDisarmed)) return false;
    if (terrain.has(current_area, px, py, game::TF_TrapSprung)) return false;
    terrain.set(current_area, px, py, game::TF_TrapSprung);
    // 傷害 1d8(remake 設計;手冊未給陷阱數值)。對全活著隊員施加。
    int dmg = 1 + (int)terrain_rng.below(8);
    for (std::size_t i = 0; i < party.size(); ++i) {
      auto& c = party.at(i);
      if (c.status & 0x01) continue;
      int hp = (int)c.health - dmg;
      if (hp < 0) hp = 0;
      c.health = (std::uint16_t)hp;
      c.raw[0x14] = (std::uint8_t)(hp & 0xFF);
      c.raw[0x15] = (std::uint8_t)((hp >> 8) & 0xFF);
      if (hp == 0) { c.status |= 0x01; c.raw[76] |= 0x01; }
    }
    std::fprintf(stderr, "trap sprung @(%d,%d) dmg=%d to party\n", px, py, dmg);
    return true;
  };
  // 探索施法結算:隊伍第 0 名施放 spell_id。地形法術(Soften Stone/Disarm Trap/
  //   Sense Traps)套用到前方/當前格;其餘法術探索時只扣 Power(無探索效果)。
  //   回傳訊息 i18n key(英文)。扣 Power 寫回 record [0x1C]。
  auto resolve_explore_cast = [&](std::uint8_t spell_id) -> const char* {
    if (!level || party.size() == 0) return "Nothing happens.";
    const game::SpellDef* sp = game::find_spell(spell_id);
    if (!sp) return "Nothing happens.";
    auto& c0 = party.at(0);
    if ((int)c0.power < sp->power_cost) return "Not enough power.";
    g_sound.play(audio::SoundId::Cast);   // 施法音效(remake 設計;見 sound.hpp)
    // 扣 Power(variable_power 扣最低投入;對齊 cast_spell)。
    int pw = (int)c0.power - sp->power_cost;
    c0.power = (std::uint16_t)pw;
    c0.raw[0x1C] = (std::uint8_t)(pw & 0xFF);
    c0.raw[0x1D] = (std::uint8_t)((pw >> 8) & 0xFF);
    // 地形效果:對面向前方格 + 當前格。
    int fx = px + dx4[dir], fy = py + dy4[dir];
    if (level->wraps()) { fx = level->wrap_x(fx); fy = level->wrap_y(fy); }
    std::uint8_t ft = level->tile(fx, fy), ct = level->tile(px, py);
    using TSR = game::TerrainSpellResult;
    // 真實陷阱格(路徑 B)讓 Sense/Disarm 對原版陷阱生效(非保留 0x33)。
    bool fwd_rt = real_traps.is_trap(fx, fy), cur_rt = real_traps.is_trap(px, py);
    TSR r = game::apply_terrain_spell(terrain, spell_id, current_area, fx, fy, ft,
                                      px, py, ct, fwd_rt, cur_rt);
    switch (r) {
      case TSR::TrapsSensed:   std::fprintf(stderr, "cast Sense Traps -> sensed\n");  return "You sense the traps nearby.";
      case TSR::TrapDisarmed:  std::fprintf(stderr, "cast Disarm Trap -> disarmed\n"); return "The trap is disarmed.";
      case TSR::StoneSoftened: std::fprintf(stderr, "cast Soften Stone -> softened\n"); return "The stone softens and crumbles.";
      case TSR::WallCreated:   std::fprintf(stderr, "cast Create Wall -> wall placed @(%d,%d)\n", fx, fy); return "A wall of stone rises.";
      case TSR::LightLit:      std::fprintf(stderr, "cast Mage Light -> light lit\n"); return "Light fills the area.";
      case TSR::NoEffect:
      case TSR::NotTerrain:
      default:
        std::fprintf(stderr, "cast spell 0x%02X -> no terrain effect (power -%d)\n",
                     spell_id, sp->power_cost);
        return "Nothing happens.";
    }
  };

  // ── 建角流程(手冊選單 B → 建立人物)──
  // 進建角畫面:開新一輪建角(清空已建清單,從第一名命名開始)。
  auto start_chargen = [&]() {
    cg.start();
    state = S_CREATE;
    std::fprintf(stderr, "chargen: enter S_CREATE (new game / create character)\n");
  };
  // 完成建角 → 把已建 records 組成 Party → 進遊戲(波卡城 area 1)。
  // 至少 1 名才可開始;0 名則回退預設隊伍(避免空隊)。回傳是否成功進遊戲。
  auto finish_chargen = [&]() -> bool {
    if (cg.done_records.empty()) {
      std::fprintf(stderr, "chargen: no characters created; keep default party\n");
    } else {
      party = game::Party::from_raw_records(cg.done_records);
      std::fprintf(stderr, "chargen: party assembled (%zu members):", party.size());
      for (std::size_t i = 0; i < party.size(); ++i)
        std::fprintf(stderr, " %s", party.at(i).name.c_str());
      std::fprintf(stderr, "\n");
    }
    cg.close();
    if (!enter_map(1)) { std::fprintf(stderr, "chargen: enter_map(1) failed\n"); return false; }
    state = S_GAME;
    return true;
  };

  if (map_area >= 0) {
    if (!enter_map(map_area)) return 1;
    state = S_GAME;
    // --read-para N:直接開段落 N 的捲動 overlay(headless 驗證長段落跨頁)。
    // 實際 open 延後到 SDL/TextLayer 就緒後(open_para 需要 tl.wrap);這裡只記 N。
    if (read_para >= 0) {
      event_msg = para_text(read_para);   // 預載全文(空 = 查無;含 zh-TW 回退)
      event_para_n = read_para; last_event_tile = -1;
      std::fprintf(stderr, "read-para %d: %zu bytes%s\n", read_para, event_msg.size(),
                   event_msg.empty() ? " (not found)" : "");
    }
    // --at:把玩家放到指定格;若是事件格(tile>1)立刻跑事件腳本(headless 驗證)。
    else if (at_x >= 0 && at_y >= 0 && level && level->in_bounds(at_x, at_y)) {
      px = at_x; py = at_y;
      int tv = level->tile(px, py);
      // 終戰 Namtar:area27 op_8A combat encounter 格(tile 0x18/0x19)→ 由互動移動迴圈
      //   接 combat_loop(begin_namtar);此早期 --at setup 只記座標/事件,不於此觸發戰鬥
      //   (begin_namtar lambda 在後方定義)。headless 端到端走 --fight-namtar。
      if (current_area == 27 && (tv == 0x18 || tv == 0x19)) {
        last_event_tile = -1;   // 不於此跑 run_event(避免 op_8A halt 噪音);留給移動迴圈觸發
        std::fprintf(stderr, "at (%d,%d) tile=0x%02X = area27 終戰格(移動觸發 Namtar)\n", px, py, tv);
      } else if (tv > 1) {
        event_msg = run_event((std::uint8_t)tv); last_event_tile = tv;
        // 寶箱偵測(grounded):--at 直接落在寶箱格時也標記可 K 開箱(headless 驗證;
        //   真實遊玩走移動迴圈的同名偵測,此處對齊)。
        chest_here = (event_is_chest) &&
                     !opened_chests.count((long)current_area * 1000000 + (long)px * 1000 + py);
        std::fprintf(stderr, "at (%d,%d) tile=0x%02X event=\"%s\" chest_here=%d\n",
                     px, py, tv, event_msg.c_str(), chest_here ? 1 : 0);
        sync_relocation();   // 事件可能換 area / 傳送(headless 也套用,供 --map+--at 驗證)
      }
    }
  }

  // --automap N:headless 直接進第 N 區的俯視平面地圖(`?` 鍵功能)。
  //   例外:同時帶 --char-sheet 時,角色屬性表為使用者明確意圖,優先於 automap。
  //   automap 仍需 enter_map 載入該區(供屬性表後方場景一致),但 state 留在 S_GAME
  //   讓 char_sheet 在 2761 被消費(否則 S_MAP 會吞掉 --char-sheet,稽核 #3 根因)。
  if (automap_mode) {
    if (!enter_map(automap_area)) return 1;
    if (char_sheet >= 1) {
      state = S_GAME;
      std::fprintf(stderr, "note: --char-sheet 優先於 --automap,state 留在 S_GAME 開屬性表\n");
    } else {
      state = S_MAP;
    }
  }

  // ── --selftest-save:headless round-trip 自測(不開 SDL,印 PASS/FAIL 後結束)──
  // 流程:進 area 1 → 走幾步 + 改 game_state/party → 存檔A → 讀回 → 再存檔B
  //       → 逐欄位比對(area/x/y/facing/game_state[256]/party records)且 A、B byte-for-byte 相同。
  if (selftest_save) {
    if (!enter_map(1)) { std::printf("FAIL: enter_map(1)\n"); return 1; }
    state = S_GAME;
    // 走幾步(沿可走格)+ 改朝向。
    for (int s = 0; s < 5; ++s) {
      dir = (dir + 1) % 4;
      int nx = px + dx4[dir], ny = py + dy4[dir];
      if (level && level->walkable_wrap(nx, ny)) {
        if (level->wraps()) { nx = level->wrap_x(nx); ny = level->wrap_y(ny); }
        px = nx; py = ny;
      }
    }
    // 改點 game_state(確定性樣式)。
    for (int i = 0; i < 256; ++i) game_state[i] = (std::uint8_t)((i * 7 + 3) & 0xFF);
    // 改 party 第一名角色的金幣與血量(經由 raw record;確保 raw 與欄位都覆蓋到)。
    {
      auto recs = party.raw_records();
      if (!recs.empty()) {
        recs[0][0x55] = 0x39; recs[0][0x56] = 0x05;  // gold = 0x0539
        recs[0][0x14] = 0x2A; recs[0][0x15] = 0x00;  // health = 42
        party = game::Party::from_raw_records(recs);
      }
    }
    game::SaveState before = capture_state();
    std::string p = "save/_selftest.sav";
    if (!game::save(before, p)) { std::printf("FAIL: save A\n"); return 1; }
    game::SaveState loaded;
    if (!game::load(p, loaded)) { std::printf("FAIL: load\n"); return 1; }
    // 逐欄位比對(loaded vs before)。
    auto field_ok = [&]() {
      if (loaded.area != before.area || loaded.x != before.x ||
          loaded.y != before.y || loaded.facing != before.facing) return false;
      if (loaded.game_state != before.game_state) return false;
      if (loaded.party_records.size() != before.party_records.size()) return false;
      for (std::size_t i = 0; i < loaded.party_records.size(); ++i)
        if (loaded.party_records[i] != before.party_records[i]) return false;
      return true;
    };
    bool fields = field_ok();
    // 再存一份(B),比對兩檔 byte-for-byte。
    std::string p2 = "save/_selftest_b.sav";
    if (!game::save(loaded, p2)) { std::printf("FAIL: save B\n"); return 1; }
    auto read_all = [](const std::string& fp) {
      std::vector<std::uint8_t> v; std::FILE* f = std::fopen(fp.c_str(), "rb");
      if (!f) return v; int c; while ((c = std::fgetc(f)) != EOF) v.push_back((std::uint8_t)c);
      std::fclose(f); return v;
    };
    bool bytes_eq = read_all(p) == read_all(p2);
    std::printf("fields: area=%d (%d,%d) dir=%d gs[256] party=%zu\n",
                loaded.area, loaded.x, loaded.y, loaded.facing,
                loaded.party_records.size());
    std::printf("field-by-field match: %s\n", fields ? "yes" : "NO");
    std::printf("save->load->save byte-for-byte: %s\n", bytes_eq ? "yes" : "NO");
    bool pass = fields && bytes_eq;
    std::printf("%s: save round-trip\n", pass ? "PASS" : "FAIL");
    return pass ? 0 : 1;
  }

  // --load <path>:啟動即讀檔還原(進遊戲);失敗則回退到一般選單流程。
  if (!load_path.empty()) {
    if (do_load(load_path)) { /* state = S_GAME(已於 apply_state 設) */ }
    else std::fprintf(stderr, "load: falling back to menu\n");
  }

  // --shop / --recruit / --char-sheet 單獨使用(無 --map/--load/--newgame)時:
  //   這些是 headless 驗證旗標,需 S_GAME 才會被消費(見下方 2565+/2579+)。預設隊伍
  //   已於啟動載入(load_default),但 state 仍停在 S_MENU → 旗標靜默跳過(game tester
  //   發現:單獨 --shop/--recruit/--char-sheet 落在主選單)。此處在沒有地圖/讀檔/建角把
  //   我們帶進 S_GAME 時,直接進 S_GAME(子畫面不需地圖),使這些旗標可獨立 headless 驗證。
  if (state == S_MENU && party.size() > 0 &&
      (shop_open || recruit_open || char_sheet >= 1)) {
    state = S_GAME;
    std::fprintf(stderr, "headless flag (shop/recruit/char-sheet) without map → enter S_GAME with default party\n");
  }

  // --newgame:啟動即進建角畫面(互動)。
  if (newgame && menu_mode) start_chargen();

  // --newgame-screen SPEC:停在配點畫面(PhAttr)供截圖。取 SPEC 第一員。
  if (!newgame_screen.empty()) {
    auto split = [](const std::string& s, char d) {
      std::vector<std::string> out; std::string cur;
      for (char c : s) { if (c == d) { out.push_back(cur); cur.clear(); } else cur.push_back(c); }
      out.push_back(cur); return out;
    };
    cg.start();
    std::string spec = split(newgame_screen, '/')[0];
    auto f = split(spec, ':');
    if (f.size() >= 1) cg.draft.name = f[0];
    if (f.size() >= 2) cg.draft.gender = (std::uint8_t)(std::atoi(f[1].c_str()) & 1);
    if (f.size() >= 3) {
      auto nums = split(f[2], ',');
      for (int a = 0; a < 4 && a < (int)nums.size(); ++a) {
        int target = std::atoi(nums[a].c_str()), guard = 0;
        while (cg.draft.attr[a] < target && cg.draft.inc(a) && guard++ < 100) {}
        while (cg.draft.attr[a] > target && cg.draft.dec(a) && guard++ < 100) {}
      }
    }
    cg.phase = CharGenUi::PhAttr;
    state = S_CREATE;
    std::fprintf(stderr, "newgame-screen: at PhAttr, name='%s' leftover=%d\n",
                 cg.draft.name.c_str(), cg.draft.leftover());
  }

  // --newgame-demo SPEC:headless 腳本化建角 + 驗證(確定性,不需互動)。
  //   SPEC 格式:角色以 '/' 分隔,每員 "name:gender:STR,DEX,INT,SPI"
  //     gender 0=男 1=女;四數為「目標屬性絕對值」(以 inc/dec 逼近,合法範圍內夾制)。
  //   範例:--newgame-demo "Aria:1:14,16,10,10/Borin:0:18,9,9,12"
  //   建完組成 Party + 進遊戲(area 1)→ --dump 可出隊伍面板;summary 印到 stderr。
  if (!newgame_demo.empty()) {
    cg.start();
    auto split = [](const std::string& s, char d) {
      std::vector<std::string> out; std::string cur;
      for (char c : s) { if (c == d) { out.push_back(cur); cur.clear(); } else cur.push_back(c); }
      out.push_back(cur); return out;
    };
    for (const std::string& spec : split(newgame_demo, '/')) {
      if (spec.empty()) continue;
      if ((int)cg.done_records.size() >= CharGenUi::kMaxParty) break;
      auto f = split(spec, ':');
      cg.draft = game::DraftCharacter{};
      if (f.size() >= 1) cg.draft.name = f[0];
      if (f.size() >= 2) cg.draft.gender = (std::uint8_t)(std::atoi(f[1].c_str()) & 1);
      if (f.size() >= 3) {
        auto nums = split(f[2], ',');
        for (int a = 0; a < 4 && a < (int)nums.size(); ++a) {
          int target = std::atoi(nums[a].c_str());
          // 以 inc/dec 逼近目標(守恆 + 範圍由 DraftCharacter 保證)。
          int guard = 0;
          while (cg.draft.attr[a] < target && cg.draft.inc(a) && guard++ < 100) {}
          while (cg.draft.attr[a] > target && cg.draft.dec(a) && guard++ < 100) {}
        }
      }
      if (!cg.commit_current())
        std::fprintf(stderr, "newgame-demo: invalid spec '%s' (skipped)\n", spec.c_str());
    }
    std::fprintf(stderr, "newgame-demo: created %zu character(s)\n", cg.done_records.size());
    if (finish_chargen()) {
      // 驗證 summary(stderr):逐員名/屬性/衍生/effective AV·DV(SDA = DEX/4)。
      for (std::size_t i = 0; i < party.size(); ++i) {
        const auto& c = party.at(i);
        std::fprintf(stderr,
                     "  [%zu] %-12s g=%u STR=%u DEX=%u INT=%u SPI=%u  HP=%u STUN=%u PWR=%u  "
                     "eAV=%d eDV=%d (DEX/4=%d) lvl=%u\n",
                     i, c.name.c_str(), c.gender, c.strength, c.dexterity, c.intel,
                     c.spirit, c.health, c.stun, c.power, c.effective_av(),
                     c.effective_dv(), c.dexterity / 4, c.level);
      }
    }
  }

  // ── 開機 title splash 進入點(對齊 DOS「先 dragon art 標題畫面 → 按鍵 → 主選單」)──
  //   只在「一般選單流程」(menu_mode)且 state 仍停在 S_MENU(沒有 --load/--newgame/
  //   headless 子畫面旗標把我們帶離選單)時生效。--no-splash 略過;--title 強制。
  //   headless(--press / --frames 自動化)走 --no-splash 即可直達選單,不影響既有測試。
  //   顯式 --menu(menu_tsv 指定)= 針對主選單的呼叫,直接進選單(splash 由 --title 才補)。
  if (menu_mode && state == S_MENU && !no_splash &&
      (want_title || (press == 0 && !newgame && menu_tsv.empty()))) {
    state = S_TITLE;
    // art_ok 由 load_title_art(vid 建立後)決定,此處只報進入 S_TITLE(art 狀態見 "title art loaded" log)。
    std::fprintf(stderr, "title splash: enter S_TITLE (theme=%s title_ref=%s)\n",
                 theme.name.c_str(), theme.title_ref.c_str());
  }

  // 第一人稱 viewport 資源(--fp 或選單 B 進遊戲時用):元件 bundle + 靜態框架模板。
  render::ComponentStore comps(bundle + "/components");
  // 區域攻略提示(bundle/hints.tsv:area<TAB>提示;來源《軟體世界》1991 攻略,見
  //   docs/walkthrough/38)。開區域地圖(?)時於下方顯示。缺檔則無提示(不影響執行)。
  std::map<int, std::string> area_hints;
  if (std::FILE* hf = std::fopen((bundle + "/hints.tsv").c_str(), "rb")) {
    std::string line;
    int ch;
    auto flush = [&]() {
      if (line.empty() || line[0] == '#') { line.clear(); return; }
      auto tab = line.find('\t');
      if (tab != std::string::npos) {
        int a = std::atoi(line.substr(0, tab).c_str());
        area_hints[a] = line.substr(tab + 1);
      }
      line.clear();
    };
    while ((ch = std::fgetc(hf)) != EOF) {
      if (ch == '\n') flush();
      else if (ch != '\r') line.push_back((char)ch);
    }
    flush();
    std::fclose(hf);
    std::fprintf(stderr, "hints: loaded %zu area hints\n", area_hints.size());
  }
  // 註:Amiga 原生 viewport 圖塊(themes/amiga/components,AmigaComponentStore)已抽出並按
  //   slot 對映,但重組落點仍受阻(見 draw_explore 內 Amiga FP 說明)→ Amiga 第一人稱改用
  //   DOS golden 透視 + kAmigaViewportPalette,暫不實際組裝原生圖塊(成果保留待後續逆向)。

  // area 0(Dilmun)專屬美化世界地圖 view(旋轉 90° landscape + 地形美化 + 地點標記)。
  //   與 oracle automap(下方 minimap)分工:area 0 走美化版、其餘 39 關走 oracle automap。
  render::WorldMap worldmap;
  // 俯視平面地圖(`?` 鍵 → S_MAP)。與第一人稱共用 comps;載 minimap/玩家標記模板。
  render::Minimap minimap;
  bool minimap_ok = minimap.load_templates(bundle + "/viewport/minimap.bin",
                                           bundle + "/viewport/data6820.bin");
  bool minimap_dirty = true;   // px/py 或 area 變動後需重畫;進 S_MAP 時觸發一次。
  auto minimap_seed = [&]() {
    return mm_seed == 1 ? render::Minimap::Seed::kPlayer
         : mm_seed == 2 ? render::Minimap::Seed::kNone
                        : render::Minimap::Seed::kAll;
  };
  std::vector<std::uint8_t> vpt[4];
  bool vpt_ok = false;
  if (fp_mode) {
    auto load_bin = [&](const std::string& name) -> std::vector<std::uint8_t> {
      std::string path = bundle + "/viewport/" + name + ".bin";
      std::FILE* f = std::fopen(path.c_str(), "rb");
      if (!f) return {};
      std::vector<std::uint8_t> buf; int c;
      while ((c = std::fgetc(f)) != EOF) buf.push_back((std::uint8_t)c);
      std::fclose(f);
      return buf;
    };
    vpt[0] = load_bin("vp0"); vpt[1] = load_bin("vp1");
    vpt[2] = load_bin("vp2"); vpt[3] = load_bin("vp3");
    vpt_ok = !vpt[0].empty() && !vpt[1].empty() && !vpt[2].empty() && !vpt[3].empty();
    if (!vpt_ok) std::fprintf(stderr, "fp: viewport frame templates missing (vp0..vp3)\n");
  }

  if (viewport_mode) {
    // ── 原版第一人稱 viewport 靜態框架(port 自 opendw ui_update_viewport +
    //     update_viewport)。從 bundle 載 4 象限模板 vp0..vp3,compose 進
    //     viewport_memory,再 blit 到 framebuffer (16,8),160×136 視窗。──
    auto load_vp = [&](const std::string& name) -> std::vector<std::uint8_t> {
      std::string path = bundle + "/viewport/" + name + ".bin";
      std::FILE* f = std::fopen(path.c_str(), "rb");
      if (!f) { std::fprintf(stderr, "viewport open failed: %s\n", path.c_str()); return {}; }
      std::vector<std::uint8_t> buf; int c;
      while ((c = std::fgetc(f)) != EOF) buf.push_back((std::uint8_t)c);
      std::fclose(f);
      return buf;
    };
    auto v0 = load_vp("vp0"), v1 = load_vp("vp1"), v2 = load_vp("vp2"), v3 = load_vp("vp3");
    if (v0.empty() || v1.empty() || v2.empty() || v3.empty()) return 1;
    render::ViewportDecoder vd;
    vd.reset(0);
    vd.compose_frame(v0.data(), v1.data(), v2.data(), v3.data());
    fb.clear(0);
    vd.to_framebuffer(fb);   // 預設原點 (16, 8)
    std::fprintf(stderr, "viewport frame composed (vp0..vp3, 160x136 @ 16,8)\n");
  } else if (scene_mode) {
    // ── E:全螢幕場景圖(從 bundle .pic 載解壓資料,title_adjust 去交錯)──
    std::string path = scene_name.find('/') != std::string::npos
                         ? scene_name : bundle + "/scenes/" + scene_name + ".pic";
    std::FILE* sf = std::fopen(path.c_str(), "rb");
    if (!sf) { std::fprintf(stderr, "scene open failed: %s\n", path.c_str()); return 1; }
    std::vector<std::uint8_t> data(32000);
    std::size_t n = std::fread(data.data(), 1, data.size(), sf); std::fclose(sf);
    if (n != data.size()) { std::fprintf(stderr, "scene size %zu != 32000: %s\n", n, path.c_str()); return 1; }
    render::decode_fullscreen(fb, data);
    std::fprintf(stderr, "scene %s rendered (bundle, no DATA1)\n", scene_name.c_str());
  } else if (sprite_mode) {
    // ── A:sprite 檢視 ──
    std::string path = sprite_name.find('/') != std::string::npos
                         ? sprite_name : bundle + "/sprites/" + sprite_name + ".spr";
    auto sp = render::Sprite::load(path);
    if (!sp) { std::fprintf(stderr, "sprite load failed: %s\n", path.c_str()); return 1; }
    fb.clear(0);
    sp->blit(fb, (render::kW - sp->w) / 2, (render::kH - sp->h) / 2, 6);
    // sprite 名稱標籤改走文字層(每幀 render_now → draw_static_text)。
    std::fprintf(stderr, "sprite %s %dx%d (bundle, no DATA1)\n", sprite_name.c_str(), sp->w, sp->h);
  } else if (encounter_mode) {
    // ── A':遭遇畫面 ──(像素層由 begin_encounter/begin_namtar/draw_encounter 於下方 render 迴圈處理)
    if (monsters.empty()) { std::fprintf(stderr, "monsters.bin missing\n"); return 1; }
    if (!fight_namtar && encounter_id >= (int)monsters.size()) {  // --fight-namtar 不用 monsters[] 索引
      std::fprintf(stderr, "encounter id %d >= %zu monsters\n", encounter_id, monsters.size());
      return 1;
    }
    // 實際進場由下方 begin_encounter/begin_namtar 完成(lambda 需先定義)。
  } else if (menu_mode) {
    // ── B:VM 在地化選單 → D:快捷字母選項 ──(tr 已於頂層依 locale 載入)
    res::BundleProvider bun(bundle);
    auto sec0 = bun.load(0);
    if (!sec0) { std::fprintf(stderr, "bundle section 0 load failed: %s\n", bundle.c_str()); return 1; }

    std::vector<std::string> msgs;
    { vm::VmState st; st.script = *sec0; st.pc = (std::size_t)start_pc;
      vm::Interpreter ip(st);
      ip.set_message_sink([&](std::size_t, const std::string& s) { msgs.push_back(s); });
      ip.run(); }
    std::string menu;
    for (auto& s : msgs) if (s.find("Begin a new game") != std::string::npos) { menu = s; break; }
    if (menu.empty()) for (auto& s : msgs) if (s.size() > menu.size()) menu = s;

    // 英文行取快捷字母(highlighted letter)+ i18n 在地化
    std::vector<std::string> en;
    for (auto& ln : lines_of(menu)) if (!ln.empty()) en.push_back(ln);
    std::size_t first_opt = 0;
    if (en.size() > 1) { header_en = en[0]; header = tr.tr(en[0]); first_opt = 1; }   // 第一行為提示
    for (std::size_t i = first_opt; i < en.size(); ++i) {
      char hot = 0;
      for (char ch : en[i]) if (std::isalpha((unsigned char)ch)) { hot = std::toupper((unsigned char)ch); break; }
      opts.push_back({hot, tr.tr(en[i]), en[i]});   // 存英文源供 F4 重譯
    }
    std::fprintf(stderr, "menu: header=\"%s\" options=%zu (hotkeys:", header.c_str(), opts.size());
    for (auto& o : opts) std::fprintf(stderr, " %c", o.hot ? o.hot : '?');
    std::fprintf(stderr, ")\n");

    // --press 模擬:直接觸發對應快捷字母(headless 驗證分支)
    if (press) for (std::size_t i = 0; i < opts.size(); ++i)
      if (opts[i].hot == press) {
        sel = (int)i;
        if (opts[i].hot == 'B') { start_chargen(); }  // 開始新遊戲 → 建角畫面(手冊 B)
        else { state = S_BRANCH; branch_label = opts[i].label; branch_label_en = opts[i].en; }
      }
  }

  // 文字層 CJK 字型解析(可攜性):若 --font-ttf / 預設路徑不存在(非本機 wqy 安裝路徑),
  //   依序退而搜常見 CJK 字型路徑,讓發佈包在未裝特定字型的環境也能找到中/日字型。
  //   不打包字型(授權考量),改執行期探測;DWR_FONT 環境變數可強制指定。
  {
    auto readable = [](const std::string& p) {
      if (p.empty()) return false;
      std::FILE* f = std::fopen(p.c_str(), "rb");
      if (f) { std::fclose(f); return true; }
      return false;
    };
    if (const char* env = std::getenv("DWR_FONT"); env && *env) font_ttf = env;
    if (!readable(font_ttf)) {
      static const char* kFontCandidates[] = {
        "/usr/share/fonts/truetype/wqy/wqy-zenhei.ttc",         // Debian/Ubuntu fonts-wqy-zenhei
        "/usr/share/fonts/wenquanyi/wqy-zenhei/wqy-zenhei.ttc", // Fedora/Arch
        "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc",
        "/usr/share/fonts/truetype/noto/NotoSansCJK-Regular.ttc",
        "/usr/share/fonts/noto-cjk/NotoSansCJK-Regular.ttc",
        "/usr/share/fonts/google-noto-cjk/NotoSansCJK-Regular.ttc",
        "/System/Library/Fonts/PingFang.ttc",                  // macOS
        "/System/Library/Fonts/STHeiti Light.ttc",
        "C:/Windows/Fonts/msjh.ttc",                           // Windows 微軟正黑
        "C:/Windows/Fonts/msyh.ttc",                           // Windows 微軟雅黑
      };
      for (const char* c : kFontCandidates) {
        if (readable(c)) { font_ttf = c; break; }
      }
      std::fprintf(stderr, "font-ttf: 回退搜尋 → %s%s\n", font_ttf.c_str(),
                   readable(font_ttf) ? "" : "(仍未找到;文字層將停用,可設 DWR_FONT 指定)");
    }
  }

  // ── 雙層渲染:像素層(framebuffer)由各 draw_* 建;文字層(CJK/ASCII)丟給 SdlVideo::text()。
  //    headless 條件:有 --dump 且未設 DISPLAY → 用 dummy driver 合成高解析畫面。──
  const bool headless = !dump.empty() && std::getenv("DISPLAY") == nullptr;
  render::SdlVideo vid;
  bool opened = win640
      ? vid.open_640x480("OpenDW Remake — 火龍之戰", font_ttf, headless)
      : vid.open(scale, "OpenDW Remake — 火龍之戰", font_ttf, headless);
  if (!opened) {
    std::fprintf(stderr, "SDL open failed\n"); return 1;
  }
  render::TextLayer& tl = vid.text();

  // ── 視窗標題:火龍之戰[語言][tileset](tileset = 目前圖形/sprite 集 = 主題)──────
  //   F4 切語言 / F8 切 tileset 時即時更新。畫面上不再常駐語系角標(移到標題)。
  auto tileset_label = [](const std::string& tn) -> std::string {
    if (tn == "dos") return "DOS";
    if (tn == "amiga") return "Amiga";
    if (tn == "x68000") return "X68000";
    if (tn == "vga") return "VGA256";
    return tn;
  };
  auto update_window_title = [&]() {
    vid.set_title("火龍之戰[" + lang_label + "][" + tileset_label(theme.name) + "]");
  };
  update_window_title();

  // ── 依當前主題載 title splash art + 套用該主題 palette(per-theme palette 打通點)──
  //   DOS / X68000(kDosScene):bundle/scenes/<title_ref>.pic → decode_fullscreen,套 theme.palette(DOS 盤)。
  //   Amiga(kAmigaPic):themes/<title_ref> → decode_amiga_planar,palette 讀檔頭覆蓋 theme.palette。
  //   啟動呼叫一次;F8 切主題時 reload。載失敗 → title_art_ok=false(splash 退回藍底),
  //   但 palette 仍套(theme 預設盤),確保整體色系隨主題切換。
  auto load_title_art = [&]() {
    vid.set_palette(theme.palette);   // 先套主題預設盤(art 載失敗時整畫面仍呈現該主題色系)
    title_art_ok = false;
    const bool amiga = (theme.title_source == render::TitleSource::kAmigaPic);
    std::string tpath = amiga
        ? bundle + "/themes/" + theme.title_ref                 // themes/amiga/title.pic
        : bundle + "/scenes/" + theme.title_ref + ".pic";       // scenes/29.pic
    std::FILE* tf = std::fopen(tpath.c_str(), "rb");
    if (!tf) {
      std::fprintf(stderr, "title art open failed: %s (splash uses fallback)\n", tpath.c_str());
      return;
    }
    // 讀整檔(DOS = 32000B nibble;Amiga = 35996B = 32B palette + 4-plane planar)。
    std::vector<std::uint8_t> tdata;
    std::uint8_t chunk[4096];
    std::size_t n;
    while ((n = std::fread(chunk, 1, sizeof chunk, tf)) > 0)
      tdata.insert(tdata.end(), chunk, chunk + n);
    std::fclose(tf);
    title_fb.clear(0);
    if (amiga) {
      if (tdata.size() < 32 + 32000) {
        std::fprintf(stderr, "amiga title art short (%zu < 32032): %s\n", tdata.size(), tpath.c_str());
        return;
      }
      render::decode_amiga_planar(title_fb, tdata);
      vid.set_palette(render::read_amiga_palette(tdata));  // 用檔頭 palette(權威)覆蓋預設盤
      title_art_ok = true;
    } else {
      if (tdata.size() < 32000) {
        std::fprintf(stderr, "dos title art short (%zu < 32000): %s\n", tdata.size(), tpath.c_str());
        return;
      }
      render::decode_fullscreen(title_fb, tdata);
      title_art_ok = true;
    }
    std::fprintf(stderr, "title art loaded: theme=%s src=%s path=%s ok=%d\n",
                 theme.name.c_str(), amiga ? "amiga-planar" : "dos-scene",
                 tpath.c_str(), (int)title_art_ok);
  };
  load_title_art();   // 啟動載一次(當前主題;F8 後 reload)。

  // ── 載入結局過場場景 idx 進 ending_fb(decode + 記下 palette)──
  //   來源/解碼規則同 title art:kDosScene → bundle/scenes/<ref>.pic、decode_fullscreen、
  //   套 theme.palette;kAmigaPic → bundle/themes/<ref>、decode_amiga_planar、palette 讀檔頭。
  //   載失敗 → ending_fb_ok=false(該幀退黑底,敘事字仍可讀)。回傳是否成功。
  auto load_ending_scene = [&](int idx) -> bool {
    ending_fb_ok = false;
    ending_pal = theme.palette;
    const auto& seq = render::theme_ending_scenes(theme);
    if (idx < 0 || idx >= (int)seq.size()) return false;
    const render::EndingScene& sc = seq[idx];
    const bool amiga = (sc.source == render::TitleSource::kAmigaPic);
    std::string path = amiga ? bundle + "/themes/" + sc.ref
                             : bundle + "/scenes/" + sc.ref + ".pic";
    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) {
      std::fprintf(stderr, "ending scene open failed: %s\n", path.c_str());
      return false;
    }
    std::vector<std::uint8_t> data;
    std::uint8_t chunk[4096];
    std::size_t n;
    while ((n = std::fread(chunk, 1, sizeof chunk, f)) > 0)
      data.insert(data.end(), chunk, chunk + n);
    std::fclose(f);
    ending_fb.clear(0);
    if (amiga) {
      if (data.size() < 32 + 32000) {
        std::fprintf(stderr, "ending amiga scene short (%zu): %s\n", data.size(), path.c_str());
        return false;
      }
      render::decode_amiga_planar(ending_fb, data);
      ending_pal = render::read_amiga_palette(data);   // 檔頭 palette(權威)
    } else {
      if (data.size() < 32000) {
        std::fprintf(stderr, "ending dos scene short (%zu): %s\n", data.size(), path.c_str());
        return false;
      }
      render::decode_fullscreen(ending_fb, data);
    }
    ending_fb_ok = true;
    std::fprintf(stderr, "ending scene %d loaded: %s (%s)\n",
                 idx, path.c_str(), amiga ? "amiga-planar" : "dos-scene");
    return true;
  };

  // 640×480 模式:像素層固定 ×2(scale=2),故 scale 概念對文字層 = 2;字級「解綁」
  //   為固定原生 px(CJK 24 / UI 16 / 標題 48),不隨 scale 縮放(這正是 docs/assessment/47 方案 3 要點)。
  // 一般 scale 模式:原生字級隨 scale 等比(基準 scale=3)。
  const int eff_scale = win640 ? 2 : scale;   // 文字/版面虛擬座標換算用的有效倍率
  const int PX_TITLE = win640 ? 48 : 48 * scale / 3;   // 標題「火龍之戰」
  const int PX_BODY  = win640 ? 24 : 24 * scale / 3;   // CJK 內文(選單/事件/段落)
  const int PX_UI    = win640 ? 16 : 16 * scale / 3;   // ASCII UI(關卡名/控制提示)

  // 文字層:大標題走 tr("Dragon Wars")(zh→火龍之戰、en→Dragon Wars、ja→ドラゴンウォーズ)。
  auto add_title = [&]() { tl.add(8, 6, tr.tr("Dragon Wars"), 14, PX_TITLE); };
  // 文字層:角落語系指示 + F4 提示(每幀重繪,即時反映當前語系)。
  auto add_lang_badge = [&]() {
    // 語系指示([繁中]/[EN]/[日])已移到視窗標題「火龍之戰[語言][tileset]」;畫面只留 F4 提示。
    tl.add(render::kW - 78, 13, "F4:lang", 8, PX_UI * 3 / 4);
  };

  // ── 開機 title splash(S_TITLE):dragon art 背景 + 在地化標題 +「按任意鍵」提示 ──
  //   像素層:theme.title_scene 的 dragon art(res29;含原版金色 "Dragon Wars" 立繪)。
  //   文字層:在地化標題「火龍之戰」(zh-TW/日)疊在下方 + 閃爍「按任意鍵」提示。
  //   按任意鍵 → S_MENU(對齊 DOS「art 標題畫面 → 按鍵 → 主選單」)。art 載失敗則退回
  //   藍底 + 標題字(仍可進選單)。frames 計數驅動提示閃爍(headless dump 為穩定相位)。
  int title_blink = 0;   // splash 提示閃爍相位(每幀 +1)
  int anim_tick = 0;     // 全域動畫相位(每幀 +1;戰鬥怪物 idle 呼吸 / 受擊閃白用,headless dump 相位穩定)
  auto draw_title = [&]() {
    if (title_art_ok) {
      fb = title_fb;     // 直接套用已解碼的 dragon art(像素層)
    } else {
      fb.clear(1);       // 退回深藍底
    }
    // 在地化標題:art 已含原版英文金色立繪 logo(畫面下方);在地化字(火龍之戰)放
    //   「頂端襯底條」,提示放「底部襯底條」,中段龍圖 + 原版 logo 完整不被蓋(可讀性優先)。
    const std::string zh_title = tr.tr("Dragon Wars");   // zh-TW→火龍之戰 / ja→ドラゴンウォーズ / en→Dragon Wars
    const std::string prompt = tr.tr("Press any key");
    // 只壓暗「文字後方的小框」(棋盤式 dither,置中),避免整條 band 蓋住原版底部金色 logo。
    auto dim_box = [&](int cx, int y0, int y1, int half_w) {
      for (int y = y0; y < y1 && y < render::kH; ++y)
        for (int x = cx - half_w; x < cx + half_w; ++x)
          if (x >= 0 && x < render::kW && ((x + y) & 1) == 0) fb.put(x, y, 0);
    };
    // 頂端在地化標題(zh/ja;en 主題 art 已有英文 logo):只壓暗標題字後方小框。
    int title_px = PX_BODY * 5 / 4;
    int tw = tl.measure_vwidth(zh_title, title_px);
    dim_box(render::kW / 2, 3, 28, tw / 2 + 8);
    tl.add((render::kW - tw) / 2, 5, zh_title, 14, title_px);     // 金色在地化標題(頂端置中)
    // 底部「按任意鍵」提示:常駐顯示(不硬閃,避免刺眼)。以亮/次亮兩色做極輕柔脈動,
    //   永不消失(週期約 1.5 秒;原本 18 幀全亮/全暗的硬閃會閃爍不舒服)。
    {
      int pw = tl.measure_vwidth(prompt, PX_UI);
      dim_box(render::kW / 2, 184, 198, pw / 2 + 8);
      std::uint8_t pc = ((title_blink / 45) % 2 == 0) ? 15 : 7;   // 白 ↔ 灰,輕柔交替(不消失)
      tl.add((render::kW - pw) / 2, 186, prompt, pc, PX_UI);
    }
    add_lang_badge();
  };

  // ── 探索/戰鬥畫面 UI chrome(docs/gameplay/59「該修」#1/#2/#3,往 1990 原版靠攏)───────────
  //
  // 誠實標示(grounding):原版的藍石磚邊框與金色「Dragon Wars」立繪 logo 是 DRAGON.COM
  //   內的 UI piece 資源(com 0x6AE0,opendw ui.c:798 `ui_pieces`)。本專案 bundle **未**抽出
  //   該資源(assets/bundle/viewport/README.md 標示「⏳ UI 框件另行抽取」),且執行期不依賴
  //   DRAGON.COM,故 **無法 byte-for-byte 還原石磚紋理與立繪 logo**。
  //   以下為 **近似(approximate,非 byte-for-byte)**:
  //     - 邊框:用藍色(調色盤 1/9)實線外框框住 viewport + 隊伍面板 + 底部訊息列,
  //       還原原版「整個 UI 被一圈藍框包住」的版面結構(非石磚紋理)。
  //     - logo:沿用 remake 既有標題文字 tr("Dragon Wars")(zh→火龍之戰),置於隊伍面板
  //       正上方(對齊原版 logo 位置),金色 ── 文字近似,非原版立繪。
  //   一旦日後抽出 com 0x6AE0 的 ui_pieces,可改以原始資源 blit 取代本近似層。
  //
  // viewport 幾何固定:160×136 @ (16,8)(像素層,render_sweep 154 對拍鎖定,不動)。
  constexpr int kVpX = 16, kVpY = 8, kVpW = 160, kVpH = 136;  // viewport 像素框
  constexpr int kPanelX = 0x36 * 4;                            // 216,隊伍面板狀態條 x 起點
  constexpr int kPanelX1 = 0x4E * 4;                           // 312,狀態條 x 終點
  // 在 framebuffer 畫一個矩形外框(四邊各一條;color = DOS 調色盤索引)。
  auto frame_rect = [&](int x0, int y0, int x1, int y1, std::uint8_t color) {
    for (int x = x0; x <= x1; ++x) { fb.put(x, y0, color); fb.put(x, y1, color); }
    for (int y = y0; y <= y1; ++y) { fb.put(x0, y, color); fb.put(x1, y, color); }
  };
  // 底部訊息列(原版:viewport 下方白底黑字框)。近似:藍框 + 黑底,文字走文字層。
  //   只佔 viewport 正下方寬度(對齊原版),不橫跨到隊伍面板區。
  constexpr int kMsgStripX0 = kVpX - 2;                 // 14
  constexpr int kMsgStripX1 = kVpX + kVpW + 1;          // 177(viewport 右緣外一格)
  constexpr int kMsgStripY0 = kVpY + kVpH + 6;          // 150
  constexpr int kMsgStripY1 = render::kH - 8;           // 192
  auto draw_msg_strip = [&](const std::string& line, std::uint8_t text_col) {
    for (int y = kMsgStripY0; y <= kMsgStripY1; ++y)        // 黑底(原版白底;此處與在地化深色系一致)
      for (int x = kMsgStripX0; x <= kMsgStripX1; ++x) fb.put(x, y, 0);
    frame_rect(kMsgStripX0, kMsgStripY0, kMsgStripX1, kMsgStripY1, 9);  // 亮藍外框
    if (!line.empty())
      tl.add(kMsgStripX0 + 4, kMsgStripY0 + 3, line, text_col, PX_UI);
  };
  // 探索/戰鬥共用框架。
  //   優先:原版 ui_pieces(石磚邊框 + Dragon Wars 立繪 logo + pillar),對拍 opendw
  //         draw_ui_piece/ui_draw/ui_header_draw,byte-for-byte 真值(docs/gameplay/59 收尾)。
  //   退回(ui_pieces.bin 缺失時):藍色實線外框 + 文字 logo 近似(舊行為)。
  //   兩者皆只畫 viewport(160×136 @ (16,8))外圍 + 右側面板區,不碰 viewport_memory
  //   (render_sweep 154 case 鎖定的像素),故不影響第一人稱對拍。
  auto draw_explore_chrome = [&]() {
    if (ui_pieces) {
      ui_pieces->draw_chrome(fb);  // 原版石磚框 + 立繪 logo + pillar(真值)
      return;
    }
    // 退回近似(無原版資源時)。
    frame_rect(kVpX - 2, kVpY - 2, kVpX + kVpW + 1, kVpY + kVpH + 1, 9);  // viewport 外框(亮藍)
    frame_rect(kPanelX - 4, kVpY - 2, kPanelX1 + 3, kVpY + kVpH + 1, 9);  // 隊伍面板外框(亮藍)
    tl.add(kPanelX - 2, 2, tr.tr("Dragon Wars"), 14, PX_UI);              // 金色文字 logo 近似
  };

  // ── 訊息/段落檢視器框幾何(320×200 虛擬座標)──
  // 落在畫面下半 + 左右留邊;CJK 內文 24px、行距適中。每頁行數由框內可用高度算。
  const int MB_X = 6, MB_W = render::kW - 12;        // 框左 + 寬(左右各留 6)
  const int MB_Y = 96, MB_H = render::kH - MB_Y - 4; // 框上緣 + 高(落在下半,底留 4)
  const int MB_PAD = 5;                              // 框內邊距
  const int MB_LINE_H = PX_BODY / eff_scale + 3;         // 行距(虛擬座標;CJK 字高/scale + 間距)
  const int MB_TEXT_X = MB_X + MB_PAD;
  const int MB_TEXT_W = MB_W - 2 * MB_PAD;
  const int MB_TEXT_TOP = MB_Y + MB_PAD;
  // 預留底部一行給 ▼ / 提示 → 內文可用行數。
  const int MB_LINES = (MB_H - 2 * MB_PAD - MB_LINE_H) / (MB_LINE_H > 0 ? MB_LINE_H : 1);

  // 用當前語系文字(已在地化)開啟訊息檢視器:wrap → 切頁。
  auto open_msg = [&](const std::string& z) {
    if (z.empty()) return;
    msg.max_vw = MB_TEXT_W; msg.body_px = PX_BODY;
    msg.open(tl.wrap(z, MB_TEXT_W, PX_BODY), MB_LINES);
  };

  // 隊伍是否已持有某名稱物品(掃全員背包;判重 → quest 物品不重給,亦免存讀檔重複)。
  auto party_has_item = [&](const std::string& name) -> bool {
    for (std::size_t i = 0; i < party.size(); ++i)
      for (int s = 0; s < game::CharacterRecord::kInventorySlots; ++s) {
        auto it = party.at(i).item_at(s);
        if (it.present && it.name == name) return true;
      }
    return false;
  };

  // grounded quest 物品給予:進區(或踩指定格)時,把該區攻略編目的 quest 物品給第 0 名並持久。
  //   tile<0:進區即給;tile>=0:踩該 tile 才給。已持有則跳過(判重)。回傳本次新給件數。
  auto try_grant_area = [&](int area, int cur_tile) -> int {
    int n = 0; std::vector<std::string> got;
    auto range = quest_grants.equal_range(area);
    for (auto it = range.first; it != range.second; ++it) {
      const QuestGrant& g = it->second;
      if (g.tile >= 0 && g.tile != cur_tile) continue;     // 指定格才給
      if (party_has_item(g.name)) continue;                // 已持有 → 不重給
      if (party.add_item(0, make_quest_item(g.name)) < 0) continue;  // 背包滿 → 略過
      got.push_back(tr.tr(g.name)); ++n;
      std::fprintf(stderr, "quest grant: area %d '%s' -> party[0]\n", area, g.name.c_str());
    }
    if (n > 0) {
      std::string z = party.at(0).name + tr.tr(" gets the ");
      for (std::size_t i = 0; i < got.size(); ++i) z += (i ? "、" : "") + got[i];
      open_msg(z + "!");
      g_sound.play(audio::SoundId::DoorOpen);
    }
    return n;
  };

  // ── 半透明 + 優雅覆蓋框(theme-aware;見 render/ui_theme.hpp OverlayStyle)──
  //   indexed framebuffer 無原生 alpha → 用「棋盤式 dithering」模擬半透明:
  //     dither=true 時,只在 (x+y) 偶數格填 base 底色,奇數格保留底下遊戲畫面像素 →
  //     形成「紗窗(screen-door)」半透明,viewport/地圖隱約透出;文字走 TTF 層後繪,恆銳利。
  //   邊框:外白(border)+ 內亮藍(border2)雙線框 + 四角內縮一格(柔化/圓角感)。
  //   dither=false 退回實心底(舊行為)。可讀性優先:文字底永遠夠暗(深藍/黑系 base)。
  auto draw_overlay_box = [&](int bx, int by, int bw, int bh,
                              const render::OverlayStyle& st) {
    int x1 = bx + bw - 1, y1 = by + bh - 1;
    // 1) 底:dither 半透明 或 實心。
    //    透明採「3/4 覆蓋」格網(每 2×2 區塊只留 1 格透出底下畫面),比純棋盤(1/2)更
    //    不透明 → 即使底下是亮色(cyan 天空)白字仍清楚(可讀性優先),同時保留透視感。
    //    透出格選右下角((x&1)&&(y&1)),分布均勻不結塊。
    for (int y = by; y <= y1 && y < render::kH; ++y)
      for (int x = bx; x <= x1 && x < render::kW; ++x) {
        bool show_through = st.dither && ((x & 1) && (y & 1));
        if (!show_through) fb.put(x, y, st.base);
        // show_through 格:不動 → 保留底下遊戲畫面像素(透出)。
      }
    // 2) 外框(白實線),四角內縮一格 → 柔角(避免硬直角,優雅感)。
    for (int x = bx + 1; x <= x1 - 1; ++x) { fb.put(x, by, st.border); fb.put(x, y1, st.border); }
    for (int y = by + 1; y <= y1 - 1; ++y) { fb.put(bx, y, st.border); fb.put(x1, y, st.border); }
    // 3) 內框(亮藍雙線),內縮 2 格 → 雙線優雅邊。
    int ix0 = bx + 2, iy0 = by + 2, ix1 = x1 - 2, iy1 = y1 - 2;
    if (ix1 > ix0 && iy1 > iy0) {
      for (int x = ix0; x <= ix1; ++x) { fb.put(x, iy0, st.border2); fb.put(x, iy1, st.border2); }
      for (int y = iy0; y <= iy1; ++y) { fb.put(ix0, y, st.border2); fb.put(ix1, y, st.border2); }
    }
  };

  // 文字置中(虛擬座標):回傳讓 utf8 在 [x0,x1] 置中的 x。
  auto center_x = [&](const std::string& s, int x0, int x1, int px) {
    int w = tl.measure_vwidth(s, px);
    return x0 + ((x1 - x0) - w) / 2;
  };

  // ── F1 Help 覆蓋層:半透明優雅框 + 操作鍵清單(i18n 三語)──
  //   精要整理自 docs/engine/CONTROLS.md;每幀重繪 → F4 切語系即時重排。Esc / F1 關閉。
  auto draw_help_overlay = [&]() {
    const int HX = 24, HW = render::kW - 48, HY = 18, HH = render::kH - 36;
    draw_overlay_box(HX, HY, HW, HH, theme.overlay);
    int tx = HX + 8, y = HY + 6;
    std::string title = tr.tr("Controls");
    tl.add(center_x(title, HX, HX + HW, PX_BODY), y, title, theme.overlay.accent, PX_BODY);
    y += PX_BODY / eff_scale + 6;
    // 操作鍵清單(英文鍵 + i18n 說明)。鍵碼原文不譯,說明走 tr()。
    const std::pair<const char*, const char*> rows[] = {
      {"I / J / L", "Move forward / turn left / turn right"},
      {"K", "Open / force door"},
      {"V  /  1-4", "Character sheet"},
      {"C", "Cast spell (explore)"},
      {"P  /  T", "Shop / Tavern"},
      {"?", "Overhead map"},
      {"G", "Main quest guide"},
      {"S", "Save game"},
      {"F1", "This help"},
      {"F4", "Cycle language"},
      {"F8", "Cycle UI theme"},
      {"F10  /  Esc", "Quit (autosave + confirm)"},
    };
    int line_h = PX_UI / eff_scale + 4;
    for (const auto& r : rows) {
      tl.add(tx, y, r.first, 11, PX_UI);                 // 鍵(亮藍,原文)
      tl.add(tx + 80, y, tr.tr(r.second), 15, PX_UI);    // 說明(白,i18n)
      y += line_h;
    }
    std::string foot = tr.tr("Esc: close");
    tl.add(center_x(foot, HX, HX + HW, PX_UI), HY + HH - line_h - 2, foot, 8, PX_UI);
  };

  // ── 離開確認視窗(F10 / 頂層 ESC 觸發;已自動存檔)──
  //   半透明優雅框 + 兩行訊息(已自動存檔 / 確定離開?Y/N),i18n 三語。
  auto draw_confirm_quit = [&]() {
    const int CW = 230, CH = 70;
    const int CXq = (render::kW - CW) / 2, CYq = (render::kH - CH) / 2;
    draw_overlay_box(CXq, CYq, CW, CH, theme.overlay);
    std::string l1 = tr.tr("Game autosaved.");
    std::string l2 = tr.tr("Quit the game? (Y/N)");
    int line_h = PX_UI / eff_scale + 5;
    int y = CYq + 12;
    tl.add(center_x(l1, CXq, CXq + CW, PX_UI), y, l1, 15, PX_UI); y += line_h;
    tl.add(center_x(l2, CXq, CXq + CW, PX_UI), y, l2, theme.overlay.accent, PX_UI);
  };

  // ── F8 主題切換短暫提示(toast;畫面下緣,theme_toast 幀數倒數)──
  auto draw_theme_toast = [&]() {
    char buf[128];
    // 誠實標示:partial 主題在名稱後加「(partial)」(對齊 ui_theme.hpp note;非完整移植)。
    //   VGA-256 主題在名稱後加「(256色)」標示為 256 色增強版(remake 加值)。
    if (theme.partial)
      std::snprintf(buf, sizeof buf, "%s: %s (%s)", tr.tr("Theme").c_str(),
                    theme.name.c_str(), tr.tr("partial").c_str());
    else if (theme.vga256)
      std::snprintf(buf, sizeof buf, "%s: %s (%s)", tr.tr("Theme").c_str(),
                    theme.name.c_str(), tr.tr("256 colours").c_str());
    else
      std::snprintf(buf, sizeof buf, "%s: %s", tr.tr("Theme").c_str(), theme.name.c_str());
    int tw = tl.measure_vwidth(buf, PX_UI);
    int bx = (render::kW - tw) / 2 - 6, by = render::kH - 22, bw = tw + 12, bh = 16;
    draw_overlay_box(bx, by, bw, bh, theme.overlay);
    tl.add(bx + 6, by + 3, buf, theme.overlay.accent, PX_UI);
  };

  // 訊息框底框 + 邊框(半透明 + 雙線優雅框;theme.overlay)。
  auto fill_msg_box = [&]() {
    draw_overlay_box(MB_X, MB_Y, MB_W, MB_H, theme.overlay);
  };

  // 畫訊息檢視器:底框(像素層)+ 當前頁文字行(文字層)+ ▼/提示。
  auto draw_msg_overlay = [&]() {
    fill_msg_box();
    int y = MB_TEXT_TOP;
    for (const std::string& ln : msg.page_lines()) {
      tl.add(MB_TEXT_X, y, ln, 15, PX_BODY);
      y += MB_LINE_H;
    }
    // 底部指示:多於一頁顯示 ▼ 與頁碼;最後一頁顯示「Esc/Space 繼續」。
    int iy = MB_Y + MB_H - MB_LINE_H - 1;
    if (msg.has_more()) {
      char buf[32];
      std::snprintf(buf, sizeof buf, "%s  %d/%d", "\xE2\x96\xBC", msg.page + 1, msg.page_count());
      tl.add(MB_TEXT_X, iy, buf, 14, PX_UI);          // ▼ + 頁碼(黃)
    } else {
      tl.add(MB_TEXT_X, iy, tr.tr("[ continue ]"), 11, PX_UI);  // 末頁:繼續提示
    }
  };

  // ── Read Paragraph 長段落捲動 overlay 框幾何(320×200 虛擬座標)──
  // 近全螢幕(只在四周留薄邊),標題列在上、內文捲動區在中、捲動位置提示在下。
  // 用既有雙層渲染:底框 + 邊框走像素層;標題與段落文字走 TextLayer 高解析(CJK 銳利)。
  const int PB_X = 4, PB_W = render::kW - 8;          // 框左 + 寬(左右各留 4)
  const int PB_Y = 4, PB_H = render::kH - 8;          // 框上 + 高(上下各留 4,幾乎全螢幕)
  const int PB_PAD = 5;
  const int PB_LINE_H = PX_BODY / eff_scale + 3;          // 內文行距(虛擬座標)
  const int PB_TEXT_X = PB_X + PB_PAD;
  const int PB_TEXT_W = PB_W - 2 * PB_PAD;
  const int PB_TITLE_TOP = PB_Y + PB_PAD;             // 標題列 y
  const int PB_TEXT_TOP = PB_TITLE_TOP + PB_LINE_H + 3;  // 內文起始 y(標題下)
  // 內文可視行數:扣掉標題列(+間隔)與底部一行捲動提示。
  const int PB_LINES = (PB_H - 2 * PB_PAD - 2 * PB_LINE_H - 3) / (PB_LINE_H > 0 ? PB_LINE_H : 1);

  // 把段落 N 的在地化全文 wrap 後開啟捲動 overlay(回到頂端)。
  // 段落僅 zh-TW 有資料;book->text(N) 缺則由呼叫端事先回退,這裡只負責排版。
  auto open_para = [&](int n, const std::string& full) {
    if (full.empty()) return;
    para.open(n, tl.wrap(full, PB_TEXT_W, PX_BODY), PB_LINES);
  };

  // ── 結局序列(S_ENDING)──────────────────────────────────────────────────
  // 誠實標示:**remake 組合結局,非原版單一 script**。原版「勝利後結局畫面」由戰鬥
  //   流程/DRAGON.COM 主控觸發,不在任何 level event script 中(掃全 40 關證實,docs/gameplay/55
  //   §3.3),逆不出獨立結局 script。本序列以「已 bundle 的真實素材」組合:
  //     ① area27 結局敘事(events.tsv 的真實 emit 鍵:納達現身/鐵頭巴克/決戰平原)
  //     ② 結局段落(手冊段落 131/132/135/137/138,攻略 §5.20 註明的決戰/結局段落;
  //        已 bundle 於 paragraphs/zh-TW)③ remake 勝利訊息 + 全劇終。
  //   敘事鍵與段落 = 真實素材;「組合與串接」= remake 設計(誠實標示)。
  //
  // 把結局組成單一可捲動文件(ParaViewer,para_n = -1 表示結局)。各段以空行分隔。
  auto build_ending_doc = [&]() -> std::string {
    auto L = [&](const char* en) { return tr.tr(en); };  // i18n(查無回退英文)
    std::string d;
    auto add = [&](const std::string& s) {
      if (s.empty()) return;
      if (!d.empty()) d += "\n\n";
      d += s;
    };
    // ── ① 終戰敘事(area27 真實 emit 鍵)──
    add(L("A voice rings from the shadows, \"So you are finally here... I must admit "
          "I underestimated you.\""));
    add(L("Buck Ironhead, Namtar's top general, regards you from across the room. "
          "\"You've hacked past my best men -- I'm not sure what I can do against "
          "you,\" he says \"But I'm not going down without a fight...\""));
    add(L("To the south lay the forces of Namtar!"));
    add(L("It's you against an entire army! "));
    // ── ② remake 勝利結算(自由之劍劈死 Namtar)──
    add(L("You raise the blessed Sword of Freedom and strike with all your strength!"));
    add(L("With a final, terrible cry, Namtar -- the Beast From The Pit -- crumbles "
          "before you."));
    // ── ③ 結局段落(手冊段落,已 bundle 真實素材)── 攻略 §5.20:131/132/135(納達現身)、
    //     137/138(Irkalla 與自由之劍重生 / 結局)。逐段附段落號標頭。
    auto add_para = [&](int n) {
      if (!book) return;
      if (auto t = book->text(n)) {
        char hdr[48];
        std::snprintf(hdr, sizeof hdr, "%s %d", L("Paragraph").c_str(), n);
        add(std::string(hdr) + "\n" + *t);
      }
    };
    for (int n : {131, 132, 135, 137, 138}) add_para(n);
    // ── 收尾:屍身送靈魂之泉 → 納達之坑 → 歐西納自由 → 全劇終 ──
    add(L("You bear his body to the Well of Souls, then walk slowly toward the pit "
          "that spawned him."));
    add(L("Oceana is free at last. Enjoy the sweet taste of your final victory!"));
    add(L("THE END"));
    return d;
  };
  // 結局序列「組合說明」標頭(誠實標示,放文件最前一段)。
  auto ending_disclaimer = [&]() -> std::string {
    // 中性說明:此結局為 remake 以 bundled 段落 + area27 敘事組合,非原版單一 script。
    return tr.tr("(remake-composed ending: bundled paragraphs + area27 narrative; "
                 "not a single original script)");
  };
  // 開 bundled 段落捲動文件(結局過場場景之間的長文閱讀,沿用 ParaViewer/para_n=-1)。
  auto open_ending_doc = [&]() {
    std::string doc = ending_disclaimer() + "\n\n" + build_ending_doc();
    para.open(-1, tl.wrap(doc, PB_TEXT_W, PX_BODY), PB_LINES);
    std::fprintf(stderr, "ENDING doc opened (lines=%d, page_count=%d)\n",
                 para.total_lines(), para.page_count());
  };
  // ── 主線指引(quest guide;remake 加值,按 G 開)──────────────────────────────
  //   內容 = 已逆出的勝利條件鏈(docs/gameplay/55,bytecode trace + 攻略真值);讓中後期
  //   不再「亂走撞事件」。誠實標示:這是 remake 依逆向結果整理的指引,非原版內建系統。
  //   可選性開啟(不強迫提示);純粹派可不按。
  auto build_quest_guide = [&]() -> std::string {
    std::string g;
    g += "《火龍之戰》主線指引\n";
    g += "(remake 依逆向勝利鏈整理;迷路時參考,不強迫使用)\n\n";
    g += "最終目標:擊敗納達(Namtar)—— 需「受祝福的自由之劍」+ 召喚龍后助戰。\n\n";
    g += "── 開局 ──\n";
    g += "你在波卡城(Purgatory)被裸身丟入。先在城內探索、招募隊員(T)、買裝備(P)、\n";
    g += "練等;按 ? 看平面地圖,I 前進 / J·L 轉向,K 開門破密門。\n\n";
    g += "── 1. 鑄造自由之劍 ──\n";
    g += "· 取得骷髏(Skull)。\n";
    g += "· 到矮人城堡(area 16)的鑄爐(Dwarf Forge)交骷髏鑄劍。\n";
    g += "· 經地獄之火(Inferno)+ 英雄羅拔精神 + 阿普蘇之水(Apsu Waters)淬煉重生\n";
    g += "  (對應段落書第 138 段)。\n\n";
    g += "── 2. 為劍祝福(祝福後一擊可削納達約 100 HP)──\n";
    g += "· 伊爾卡拉(Irkalla)祝福:到瑪根地底世界(area 18),解除被銀鍊綁住的伊爾卡拉\n";
    g += "  之詛咒(段落書第 137 段)。\n";
    g += "· 永恆之神(Universal God)祝福:全屬性 +3。\n\n";
    g += "── 3. 召龍 ──\n";
    g += "· 到龍谷(area 32)取得龍寶石(Dragon Gem)—— 決戰時可召喚龍后(Dragon Queen)×3 助戰。\n\n";
    g += "── 其他關鍵 ──\n";
    g += "· 沉沒之城(area 22,水下):需水中呼吸藥水;取英雄魂(角色祝福)。\n\n";
    g += "── 4. 終戰與結局 ──\n";
    g += "· 到尼塞山腹(Depths of Nisir,area 27)決戰納達。\n";
    g += "· 擊敗後,將納達屍體送到靈魂之泉(Well of Souls),再投入納達之坑(Namtar's Pit)。\n\n";
    g += "(按 ▲▼ / PgUp·PgDn 捲動;Esc 關閉)";
    return g;
  };
  auto open_quest_guide = [&]() {
    para.open(-1, tl.wrap(build_quest_guide(), PB_TEXT_W, PX_BODY), PB_LINES);
    std::fprintf(stderr, "QUEST GUIDE opened (lines=%d, pages=%d)\n",
                 para.total_lines(), para.page_count());
  };

  // 結局過場序列「最後一張」索引(The End / Amiga 單張結局):末張只疊收尾標題,無段落。
  auto ending_last_idx = [&]() -> int {
    return (int)render::theme_ending_scenes(theme).size() - 1;
  };
  // 進入結局序列:從第一張過場場景開始(phase 0)。流程:
  //   phase 0(場景過場 24→倒數第二張,各配在地化敘事)→ phase 1(bundled 段落捲動)
  //   → phase 2(末張 The End / Amiga 結局,收尾標題)。
  auto enter_ending = [&]() {
    ending_phase = 0;
    ending_idx = 0;
    load_ending_scene(ending_idx);
    para.close();                       // 過場期間段落檢視器關閉(phase 1 才開)
    state = S_ENDING;
    int total = (int)render::theme_ending_scenes(theme).size();
    std::fprintf(stderr, "ENTER ENDING (scenes=%d, theme=%s)\n", total, theme.name.c_str());
  };

  // ── 畫結局過場場景(像素層 = 已解碼場景 art;文字層 = 底部襯底條疊在地化敘事)──
  //   作法對齊 title splash:保留原版場景 art(含烤進英文),在底部壓暗襯底條疊繁中敘事,
  //   確保可讀且不破壞畫面其餘部分。末張(The End)無敘事 → 只置中疊「全劇終」收尾標題。
  auto draw_ending_scene = [&]() {
    // 像素層:套該場景的 palette(Amiga 用檔頭盤;DOS 用 theme 盤)後 blit 場景。
    vid.set_palette(ending_fb_ok ? ending_pal : theme.palette);
    if (ending_fb_ok) {
      fb = ending_fb;
    } else {
      for (int y = 0; y < render::kH; ++y)
        for (int x = 0; x < render::kW; ++x) fb.put(x, y, 0);   // 退黑底
    }
    // 棋盤式壓暗襯底條(半透明黑網點),讓底下 art 隱約透出仍保 CJK 對比。
    auto dim_band = [&](int y0, int y1) {
      for (int y = y0; y < y1 && y < render::kH; ++y)
        for (int x = 0; x < render::kW; ++x)
          if (((x + y) & 1) == 0) fb.put(x, y, 0);
    };
    const auto& seq = render::theme_ending_scenes(theme);
    const render::EndingScene* sc =
        (ending_idx >= 0 && ending_idx < (int)seq.size()) ? &seq[ending_idx] : nullptr;
    // 非英文語系才疊在地化敘事;英文語系直接呈現原版 art(英文已烤進圖,不疊任何字)。
    const bool localized = (locale_tag != "[EN]");
    std::string narr;
    if (sc && localized && !sc->narrative_en.empty())
      narr = tr.tr(sc->narrative_en);   // 在地化敘事(查無回退英文)
    const bool is_last = (ending_idx == (int)seq.size() - 1);
    if (!narr.empty() && sc->ew > 0) {
      // ── 「換字不換版」:擦掉原版英文烤字區(實心填黑)→ 在原位畫銳利在地化敘事 ──
      //   取代舊版底部字幕條:英文不再透出、中文落在原版英文的構圖位置(沿用 scene_localize)。
      int bx = sc->ex, by = sc->ey, bw = sc->ew, bh = sc->eh;
      for (int y = by; y < by + bh && y < render::kH; ++y)
        for (int x = bx; x < bx + bw && x < render::kW; ++x)
          if (x >= 0 && y >= 0) fb.put(x, y, 0);   // 實心黑(= 還原英文後方黑底)
      std::vector<std::string> lines = tl.wrap(narr, bw - 8, PX_BODY);
      int line_h = PX_BODY / eff_scale + 2;
      int total_h = (int)lines.size() * line_h;
      int y = by + (bh - total_h) / 2; if (y < by + 2) y = by + 2;   // 框內垂直置中
      for (const std::string& ln : lines) {
        int w = tl.measure_vwidth(ln, PX_BODY);
        int x = bx + (bw - w) / 2; if (x < bx + 2) x = bx + 2;       // 框內水平置中
        tl.add(x, y, ln, 15, PX_BODY);   // 白字
        y += line_h;
      }
    } else if (!narr.empty()) {
      // 無擦除框(Amiga 單張結局等):沿用底部襯底條疊字幕。
      const int band_top0 = 110;
      std::vector<std::string> lines = tl.wrap(narr, render::kW - 16, PX_BODY);
      int line_h = PX_BODY / eff_scale + 2;
      int need = (int)lines.size() * line_h + 8;
      int band_top = render::kH - need; if (band_top < band_top0) band_top = band_top0;
      dim_band(band_top, render::kH);
      int y = band_top + 4;
      for (const std::string& ln : lines) {
        int w = tl.measure_vwidth(ln, PX_BODY);
        tl.add((render::kW - w) / 2, y, ln, 15, PX_BODY);   // 白字置中
        y += line_h;
      }
    }
    if (is_last) {
      // 末張收尾標題「全劇終」+ 提示(置中,The End logo 上方留白區)。
      const std::string the_end = tr.tr("THE END");
      int tpx = PX_BODY * 5 / 4;
      int w = tl.measure_vwidth(the_end, tpx);
      dim_band(140, 174);
      tl.add((render::kW - w) / 2, 146, the_end, 14, tpx);   // 金色「全劇終」
      const std::string prompt = tr.tr("Press any key");
      int pw = tl.measure_vwidth(prompt, PX_UI);
      tl.add((render::kW - pw) / 2, 188, prompt, 8, PX_UI);
    } else {
      // 非末張:右下角推進提示(i18n)。
      const std::string prompt = tr.tr("Press any key");
      int pw = tl.measure_vwidth(prompt, PX_UI);
      tl.add(render::kW - pw - 6, render::kH - 12, prompt, 8, PX_UI * 3 / 4);
    }
  };

  // 段落 overlay 底框 + 邊框(半透明 + 雙線優雅框;theme.overlay)。
  //   段落檢視器近全螢幕、以長文閱讀為主 → dither 半透明讓底下畫面隱約透出,
  //   同時 base 深藍/網點壓暗確保長文白字對比足夠(可讀性優先)。
  auto fill_para_box = [&]() {
    draw_overlay_box(PB_X, PB_Y, PB_W, PB_H, theme.overlay);
    // 標題列下方一道分隔線(像素層;落在雙線內框內),把標題與內文分開。
    int sep_y = PB_TITLE_TOP + PB_LINE_H + 1;
    for (int x = PB_X + 3; x < PB_X + PB_W - 3; ++x) fb.put(x, sep_y, theme.overlay.border2);
  };

  // 畫段落捲動 overlay:底框 + 標題「段落 N」+ 可見行切片 + 捲動位置提示(▲/▼/頁碼)。
  auto draw_para_overlay = [&]() {
    fill_para_box();
    // 標題列:結局序列(para_n<0)顯示結局標題;否則「段落 N」(i18n「段落」+ 數字)。
    char title[64];
    if (para.para_n < 0) {
      std::snprintf(title, sizeof title, "%s", tr.tr("The Ending of Dragon Wars").c_str());
    } else {
      std::snprintf(title, sizeof title, "%s %d", tr.tr("Paragraph").c_str(), para.para_n);
    }
    tl.add(PB_TEXT_X, PB_TITLE_TOP, title, 14, PX_BODY);
    // 內文可見行(自動換行後切片,top..top+visible)。
    int y = PB_TEXT_TOP;
    for (const std::string& ln : para.visible()) {
      tl.add(PB_TEXT_X, y, ln, 15, PX_BODY);
      y += PB_LINE_H;
    }
    // 底部捲動位置提示:上方還有 → ▲;下方還有 → ▼;附「目前行範圍/總行數」(明確無歧義,
    // 證明跨頁無遺漏)。例:「▼ 行 1–14 / 21」。
    int iy = PB_Y + PB_H - PB_LINE_H - 1;
    char ind[96];
    const char* up = para.at_top() ? "  " : "\xE2\x96\xB2";    // ▲
    const char* dn = para.at_bottom() ? "  " : "\xE2\x96\xBC"; // ▼
    int first = para.total_lines() ? para.top + 1 : 0;
    int last = para.top + (int)para.visible().size();
    std::snprintf(ind, sizeof ind, "%s%s  %d-%d / %d", up, dn, first, last, para.total_lines());
    tl.add(PB_TEXT_X, iy, ind, 14, PX_UI);                     // 黃:▲▼ + 行範圍
    // 操作提示(i18n)靠右;放不下時自動被框裁切,不影響閱讀。
    tl.add(PB_TEXT_X + 60, iy, tr.tr("Up/Down scroll  Space page  Esc close"), 8, PX_UI * 3 / 4);
  };

  // ── 角色屬性表框幾何(320×200 虛擬座標)──
  // 落在畫面中央偏左(避開右側隊伍面板區),框較高以容納所有屬性列。
  const int CS_X = 8, CS_Y = 20, CS_W = 200, CS_H = render::kH - CS_Y - 8;
  const int CS_PAD = 6;
  const int CS_LINE_H = PX_BODY / eff_scale + 3;   // 屬性列行距(虛擬座標)
  const int CS_VAL_X = CS_X + CS_PAD + 70;     // 數值欄起點(標籤右側)

  // 畫角色屬性表底框(像素層)。
  auto fill_char_sheet = [&]() {
    for (int y = CS_Y; y < CS_Y + CS_H && y < render::kH; ++y)
      for (int x = CS_X; x < CS_X + CS_W && x < render::kW; ++x)
        fb.put(x, y, 1);                                 // 深藍實心底
    for (int x = CS_X; x < CS_X + CS_W; ++x) {           // 上下邊框
      fb.put(x, CS_Y, 15); fb.put(x, CS_Y + CS_H - 1, 15);
    }
    for (int y = CS_Y; y < CS_Y + CS_H; ++y) {           // 左右邊框
      fb.put(CS_X, y, 15); fb.put(CS_X + CS_W - 1, y, 15);
    }
  };

  // 畫角色屬性表:底框(像素層)+ 角色名 + 各屬性列(標籤 i18n / 數值 cur/max)(文字層)。
  // 標籤一律走 tr()(查無回退英文);數值逐項取自 CharacterRecord(沿用既有 record 解析)。
  auto draw_char_sheet = [&]() {
    if (!sheet.active || sheet.idx < 0 || sheet.idx >= (int)party.size()) return;
    const auto& c = party.at((std::size_t)sheet.idx);
    fill_char_sheet();
    int tx = CS_X + CS_PAD;
    int y = CS_Y + CS_PAD;
    // 標題列:「角色 N/總數  名字」。
    char head[64];
    std::snprintf(head, sizeof head, "%s %d/%d", tr.tr("Character").c_str(),
                  sheet.idx + 1, (int)party.size());
    tl.add(tx, y, head, 14, PX_BODY);
    tl.add(CS_VAL_X, y, c.name.empty() ? "?" : c.name, 15, PX_BODY);
    y += CS_LINE_H + 2;

    // 一列:標籤(i18n)+ cur/max 數值。
    auto row = [&](const char* label_en, int cur, int max_v) {
      tl.add(tx, y, tr.tr(label_en), 7, PX_BODY);        // 標籤(灰白)
      char buf[24];
      std::snprintf(buf, sizeof buf, "%d/%d", cur, max_v);
      tl.add(CS_VAL_X, y, buf, 15, PX_BODY);             // 數值(白)
      y += CS_LINE_H;
    };
    // 一列:標籤 + 單一數值(等級/金幣/狀態)。
    auto row1 = [&](const char* label_en, const std::string& val, std::uint8_t col = 15) {
      tl.add(tx, y, tr.tr(label_en), 7, PX_BODY);
      tl.add(CS_VAL_X, y, val, col, PX_BODY);
      y += CS_LINE_H;
    };

    // 配點模式時:四屬性已列在下方配點清單 → 上方不重複(讓出空間,清單完整顯示)。
    if (!sheet.alloc_mode) {
      row("Strength",  c.strength,  c.max_strength);
      row("Dexterity", c.dexterity, c.max_dexterity);
      row("Intel",     c.intel,     c.max_intel);
      row("Spirit",    c.spirit,    c.max_spirit);
    }
    row("Health",    c.health,    c.max_health);
    row("Stun",      c.stun,      c.max_stun);
    row("Power",     c.power,     c.max_power);
    row1("Level",    std::to_string(c.level));
    // 成長點數 AP(可花於 X 配點;>0 時亮黃提示玩家可分配)。
    int ap = game::available_ap(c);
    row1("Advancement points", std::to_string(ap), ap > 0 ? 14 : 7);
    // 配點模式時隱藏金幣/狀態列(讓出空間給配點清單;不重疊出框)。
    if (!sheet.alloc_mode) {
      row1("Gold",     std::to_string(c.gold));
      row1("Status",   tr.tr(game::Party::status_key(c.status)),
           c.status ? 12 : 11);                          // 異常亮紅,正常亮綠
      // 性別(原版 record 0x4E:0 男 / 1 女)。
      row1("Gender", tr.tr(c.gender ? "Female" : "Male"), 7);
    }

    // ── X 配點模式:列出可加項目(屬性/技能),游標 > 高亮,+ 加 1 點 ──
    if (sheet.alloc_mode) {
      y += 4;
      tl.add(tx, y, tr.tr("Spend AP (X):") , 14, PX_BODY); y += CS_LINE_H;
      for (int i = 0; i < CharSheet::kAllocCount; ++i) {
        AllocTarget at = alloc_target_at(i);
        int val = at.is_attr
            ? (at.target == 0 ? c.strength : at.target == 1 ? c.dexterity
               : at.target == 2 ? c.intel : c.spirit)
            : (int)c.skills[at.target];
        bool cur = (i == sheet.alloc_cursor);
        char line[80];
        std::snprintf(line, sizeof line, "%s%s: %d", cur ? "> " : "  ",
                      tr.tr(at.label_en).c_str(), val);
        tl.add(tx + 4, y, line, cur ? 15 : 7, PX_BODY);
        y += CS_LINE_H;
      }
    }
    // ── 刪除確認(手冊 D):Y/N 提示 ──
    if (!sheet.alloc_mode && sheet.delete_confirm) {
      y += 4;
      char q[96];
      std::snprintf(q, sizeof q, "%s (%s)", tr.tr("Delete this character?").c_str(),
                    "Y/N");
      tl.add(tx, y, q, 12, PX_BODY); y += CS_LINE_H;
    }
    // ── 改名輸入(手冊 R):顯示輸入緩衝 + 游標 ──
    if (!sheet.alloc_mode && sheet.rename_mode) {
      y += 4;
      std::string line = tr.tr("New name:") + " " + sheet.rename_buf + "_";
      tl.add(tx, y, line, 14, PX_BODY); y += CS_LINE_H;
    }
    if (!sheet.flash.empty()) {
      tl.add(tx, y, sheet.flash, 11, PX_BODY); y += CS_LINE_H;
    }

    // 底部操作提示。
    int iy = CS_Y + CS_H - CS_LINE_H - 2;
    tl.add(tx, iy, tr.tr("[ continue ]"), 8, PX_UI);
    const char* hint =
        sheet.alloc_mode      ? "Up/Dn  +:add  X:done"
      : sheet.delete_confirm  ? "Y: confirm   N: cancel"
      : sheet.rename_mode     ? "Type name  Enter:OK  Esc"
                              : "1-4 E:Items X:AP D:Del R:Name Esc";
    tl.add(CS_VAL_X, iy, hint, 8, PX_UI);
  };

  // 畫物品欄(背包):底框 + 標題 + 13 格物品列(名稱 + 類型 + AV/AC 修正 + 已裝備標記)。
  // 對齊手冊「Item」操作(檢視為主);名稱走物品本身(英文),類型/標籤走 i18n tr()。
  auto draw_inventory = [&]() {
    if (!sheet.active || sheet.idx < 0 || sheet.idx >= (int)party.size()) return;
    const auto& c = party.at((std::size_t)sheet.idx);
    fill_char_sheet();
    int tx = CS_X + CS_PAD;
    int y = CS_Y + CS_PAD;
    // 標題:「物品欄  角色名」。
    char head[80];
    std::snprintf(head, sizeof head, "%s  %s", tr.tr("Inventory").c_str(),
                  c.name.empty() ? "?" : c.name.c_str());
    tl.add(tx, y, head, 14, PX_BODY);
    y += CS_LINE_H + 2;

    auto inv = c.inventory();
    int shown = 0;                 // 已顯示(present)物品數 → 游標範圍
    for (int s = 0; s < (int)inv.size(); ++s) {
      const auto& it = inv[s];
      if (!it.present) continue;
      bool cur = (shown == sheet.inv_cursor);   // 游標所在格(以「已顯示序」計)
      ++shown;
      // 名稱(白;已裝備亮綠;游標列前加 >)。
      std::uint8_t ncol = it.equipped ? 10 : 15;
      std::string nm = (cur ? "> " : "  ") + (it.name.empty() ? tr.tr("(empty)") : tr.tr(it.name));
      tl.add(tx, y, nm, cur ? 15 : ncol, PX_BODY);
      // 類型(灰)+ AV/AC 修正 + 已裝備標記。
      char meta[96];
      char mods[48] = "";
      int p = 0;
      if (it.av_mod) p += std::snprintf(mods + p, sizeof(mods) - p, " AV%+d", it.av_mod);
      if (it.ac_mod) p += std::snprintf(mods + p, sizeof(mods) - p, " AC%+d", it.ac_mod);
      std::snprintf(meta, sizeof meta, "%s%s%s",
                    tr.tr(game::item_type_key(it.type)).c_str(), mods,
                    it.equipped ? (" [" + tr.tr("Equipped") + "]").c_str() : "");
      // meta(類型/修正/已裝備)放右半欄;名稱占左半,避免重疊。
      tl.add(CS_X + CS_W / 2 + 4, y, meta, 7, PX_UI);
      y += CS_LINE_H;
    }
    if (shown == 0)
      tl.add(tx, y, tr.tr("no items"), 8, PX_BODY);
    // 有效 AV/AC(隨裝備變)+ flash 提示。
    y += 2;
    char eff[80];
    std::snprintf(eff, sizeof eff, "AV %d  AC %d", c.effective_av(), c.effective_ac());
    tl.add(tx, y, eff, 11, PX_BODY); y += CS_LINE_H;

    // ── 物品轉移子模式:列出目標隊員(↑↓ 選、Enter 確認)──
    if (sheet.transfer_mode) {
      tl.add(tx, y, tr.tr("Transfer to:"), 14, PX_BODY); y += CS_LINE_H;
      int shown_t = 0;
      for (int t = 0; t < (int)party.size(); ++t) {
        if (t == sheet.idx) continue;
        bool cur = (shown_t == sheet.target_cursor);
        const auto& tc = party.at((std::size_t)t);
        std::string nm = (cur ? "> " : "  ") + (tc.name.empty() ? std::string("?") : tc.name);
        tl.add(tx + 4, y, nm, cur ? 15 : 7, PX_BODY);
        y += CS_LINE_H;
        ++shown_t;
      }
    }
    if (!sheet.flash.empty()) tl.add(tx, y, sheet.flash, 14, PX_BODY);

    int iy = CS_Y + CS_H - CS_LINE_H - 2;
    tl.add(tx, iy, tr.tr("[ continue ]"), 8, PX_UI);
    tl.add(CS_VAL_X, iy, sheet.transfer_mode ? "Up/Dn  Enter:to  Esc:cancel"
                                             : "U:use Ent:eq D:drop T:give E Esc", 8, PX_UI);
  };

  // 商店買賣畫面:底框 + 標題(含主角金幣)+ 庫存(買)/ 背包(賣)清單 + flash。
  //   付款/收款方 = 隊伍第 0 名(主角)。買=庫存清單;賣=主角背包 present 物品清單。
  auto draw_shop = [&]() {
    fill_char_sheet();
    int tx = CS_X + CS_PAD;
    int y = CS_Y + CS_PAD;
    int gold = party.size() > 0 ? game::get_gold(party.at(0)) : 0;
    char head[96];
    std::snprintf(head, sizeof head, "%s  -  %s %d",
                  tr.tr(shop_ui.sell_mode ? "Sell" : "Buy").c_str(),
                  tr.tr("Gold").c_str(), gold);
    tl.add(tx, y, head, 14, PX_BODY);
    y += CS_LINE_H + 2;

    if (!shop_ui.sell_mode) {
      // 買:列出庫存(名稱 i18n + 購買價;買不起者灰)。
      const auto& stock = shop_data.stock();
      for (int i = 0; i < (int)stock.size() && y < CS_Y + CS_H - 2 * CS_LINE_H; ++i) {
        const auto& e = stock[(std::size_t)i];
        bool cur = (i == shop_ui.cursor);
        int price = e.buy_price();
        std::string nm = e.name_key.empty() ? e.parsed().name : tr.tr(e.name_key);
        std::uint8_t col = cur ? 15 : (gold >= price ? 7 : 8);
        char row[96];
        std::snprintf(row, sizeof row, "%s%s", cur ? "> " : "  ", nm.c_str());
        tl.add(tx, y, row, col, PX_BODY);
        char pr[24]; std::snprintf(pr, sizeof pr, "%d", price);
        tl.add(CS_X + CS_W - 46, y, pr, cur ? 14 : col, PX_UI);
        y += CS_LINE_H;
      }
    } else {
      // 賣:列出主角背包 present 物品(名稱 + 售價=購買價÷2)。
      if (party.size() > 0) {
        const auto& c = party.at(0);
        auto inv = c.inventory();
        int shown = 0;
        for (int s = 0; s < (int)inv.size() && y < CS_Y + CS_H - 2 * CS_LINE_H; ++s) {
          if (!inv[s].present) continue;
          bool cur = (shown == shop_ui.cursor);
          ++shown;
          std::string nm = inv[s].name.empty() ? tr.tr(game::item_type_key(inv[s].type)) : tr.tr(inv[s].name);
          char row[96];
          std::snprintf(row, sizeof row, "%s%s", cur ? "> " : "  ", nm.c_str());
          tl.add(tx, y, row, cur ? 15 : (inv[s].equipped ? 10 : 7), PX_BODY);
          char pr[24]; std::snprintf(pr, sizeof pr, "%d", inv[s].sale_price);
          tl.add(CS_X + CS_W - 46, y, pr, cur ? 14 : 7, PX_UI);
          y += CS_LINE_H;
        }
        if (shown == 0) tl.add(tx, y, tr.tr("no items"), 8, PX_BODY);
      }
    }
    if (!shop_ui.flash.empty())
      tl.add(tx, CS_Y + CS_H - 2 * CS_LINE_H - 2, shop_ui.flash, 14, PX_BODY);
    int iy = CS_Y + CS_H - CS_LINE_H - 2;
    tl.add(tx, iy, tr.tr("Buy/Sell"), 8, PX_UI);
    tl.add(CS_VAL_X - 10, iy, "Tab Up/Dn Ent Esc", 8, PX_UI);
  };

  // 酒館招募畫面:底框 + 標題 + 可招募 NPC 清單(已在隊伍者標 (in party))+ flash。
  auto draw_tavern = [&]() {
    fill_char_sheet();
    int tx = CS_X + CS_PAD;
    int y = CS_Y + CS_PAD;
    char head[96];
    std::snprintf(head, sizeof head, "%s  (%zu/%d)", tr.tr("Tavern").c_str(),
                  party.size(), game::kMaxPartyMembers);
    tl.add(tx, y, head, 14, PX_BODY);
    y += CS_LINE_H + 2;
    const auto& roster = game::RecruitRoster::roster();
    for (int i = 0; i < (int)roster.size(); ++i) {
      const auto& t = roster[(std::size_t)i];
      bool cur = (i == tavern_ui.cursor);
      bool joined = game::party_has_npc(party, t.identifier);
      std::string nm = tr.tr(t.name);
      char row[96];
      std::snprintf(row, sizeof row, "%s%s", cur ? "> " : "  ", nm.c_str());
      tl.add(tx, y, row, cur ? 15 : (joined ? 8 : 7), PX_BODY);
      if (joined) tl.add(CS_X + CS_W / 2 + 4, y, tr.tr("(in party)"), 8, PX_UI);
      else {
        char st[80];
        // B5:招募屬性標籤走 i18n(recruit_str/dex/int → 力量/敏捷/智力,CONTEXT.md 屬性節)。
        std::snprintf(st, sizeof st, "%s%d %s%d %s%d",
                      tr.tr("recruit_str").c_str(), t.strength,
                      tr.tr("recruit_dex").c_str(), t.dexterity,
                      tr.tr("recruit_int").c_str(), t.intel);
        tl.add(CS_X + CS_W / 2 - 20, y, st, cur ? 14 : 7, PX_UI);
      }
      y += CS_LINE_H;
    }
    if (!tavern_ui.flash.empty())
      tl.add(tx, CS_Y + CS_H - 2 * CS_LINE_H - 2, tavern_ui.flash, 14, PX_BODY);
    int iy = CS_Y + CS_H - CS_LINE_H - 2;
    tl.add(tx, iy, tr.tr("Recruit"), 8, PX_UI);
    tl.add(CS_VAL_X, iy, "Up/Dn Ent Esc", 8, PX_UI);
  };

  // 重排隊伍畫面:底框 + 標題 + 隊員列(序號 + 名;游標 > 高亮;抓起的成員亮黃 [#])。
  auto draw_reorder = [&]() {
    fill_char_sheet();
    int tx = CS_X + CS_PAD;
    int y = CS_Y + CS_PAD;
    tl.add(tx, y, tr.tr("Reorder party"), 14, PX_BODY);
    y += CS_LINE_H + 2;
    for (int i = 0; i < (int)party.size(); ++i) {
      const auto& c = party.at((std::size_t)i);
      bool cur = (i == reorder_ui.cursor);
      bool grabbed = (i == reorder_ui.grabbed);
      char row[96];
      std::snprintf(row, sizeof row, "%s%d. %s%s", cur ? "> " : "  ", i + 1,
                    c.name.empty() ? "?" : c.name.c_str(), grabbed ? "  [*]" : "");
      tl.add(tx, y, row, grabbed ? 14 : (cur ? 15 : 7), PX_BODY);
      y += CS_LINE_H;
    }
    if (!reorder_ui.flash.empty())
      tl.add(tx, CS_Y + CS_H - 2 * CS_LINE_H - 2, reorder_ui.flash, 14, PX_BODY);
    int iy = CS_Y + CS_H - CS_LINE_H - 2;
    tl.add(tx, iy, tr.tr("Reorder party"), 8, PX_UI);
    tl.add(CS_VAL_X, iy, reorder_ui.grabbed >= 0 ? "Up/Dn:move  Ent:drop  Esc"
                                                 : "Up/Dn  Ent:grab  Esc", 8, PX_UI);
  };

  // 探索施法畫面:底框 + 標題(主角 Power)+ 可施法清單(法術名 + Power)+ flash。
  auto draw_cast = [&]() {
    fill_char_sheet();
    int tx = CS_X + CS_PAD;
    int y = CS_Y + CS_PAD;
    int pw = party.size() > 0 ? (int)party.at(0).power : 0;
    char head[96];
    std::snprintf(head, sizeof head, "%s  -  Power %d", tr.tr("Cast which spell?").c_str(), pw);
    tl.add(tx, y, head, 14, PX_BODY);
    y += CS_LINE_H + 2;
    for (int i = 0; i < (int)cast_ui.spellbook.size() && y < CS_Y + CS_H - 2 * CS_LINE_H; ++i) {
      std::uint8_t sid = cast_ui.spellbook[(std::size_t)i];
      const game::SpellDef* sp = game::find_spell(sid);
      bool cur = (i == cast_ui.cursor);
      std::string nm = sp ? tr.tr(sp->name_key) : "?";
      // 地形法術以青色標示(戰鬥外實際有效),其餘灰白。
      bool terr = game::is_terrain_spell(sid);
      char row[96];
      std::snprintf(row, sizeof row, "%s%s", cur ? "> " : "  ", nm.c_str());
      tl.add(tx, y, row, cur ? 15 : (terr ? 11 : 7), PX_BODY);
      if (sp) { char pr[16]; std::snprintf(pr, sizeof pr, "P%d", sp->power_cost);
                tl.add(CS_X + CS_W - 46, y, pr, cur ? 14 : 7, PX_UI); }
      y += CS_LINE_H;
    }
    if (!cast_ui.flash.empty())
      tl.add(tx, CS_Y + CS_H - 2 * CS_LINE_H - 2, cast_ui.flash, 14, PX_BODY);
    int iy = CS_Y + CS_H - CS_LINE_H - 2;
    tl.add(tx, iy, tr.tr("Cast"), 8, PX_UI);
    tl.add(CS_VAL_X, iy, "Up/Dn Ent Esc", 8, PX_UI);
  };

  // ── 建角畫面(S_CREATE):全螢幕,像素層底 + 文字層(TTF / i18n)。──
  // PhName:名字輸入列(游標 _)。PhAttr:四屬性配點 + 衍生值 + 剩餘點數 + 性別 + 已建隊員。
  auto draw_chargen = [&]() {
    fb.clear(1);
    // ── 邊框面板版面(往 Amiga「米色雙線框對話框」靠攏,取代純藍底裸字)──────────────
    //   左面板:建角內容(名字/屬性/衍生值);右面板:已建隊伍。實心優雅雙線框 + 柔角。
    render::OverlayStyle ps = theme.overlay; ps.dither = false;   // 實心(背後無遊戲畫面)
    draw_overlay_box(6, 22, 196, 150, ps);     // 左:建角面板
    draw_overlay_box(206, 22, 108, 150, ps);   // 右:隊伍面板
    add_title();
    add_lang_badge();
    int x = 16, y = 30;
    // 標題:「建立人物  (已建 N/4)」(面板內頂端,黃強調)。
    char head[80];
    std::snprintf(head, sizeof head, "%s  (%d/%d)", tr.tr("Create Character").c_str(),
                  (int)cg.done_records.size(), CharGenUi::kMaxParty);
    tl.add(x, y, head, ps.accent, PX_BODY); y += 18;

    if (cg.phase == CharGenUi::PhName) {
      tl.add(x, y, tr.tr("Enter name:"), 7, PX_BODY); y += 16;
      // 輸入中名字 + 閃爍游標(每幀都畫 _,簡單可見即可)。
      std::string shown = cg.draft.name + "_";
      tl.add(x + 8, y, shown, 15, PX_BODY); y += 22;
      tl.add(x, y, tr.tr("Enter: confirm  Esc: cancel"), 8, PX_UI);
    } else {  // PhAttr
      // 角色名 + 性別。
      char nm[80];
      std::snprintf(nm, sizeof nm, "%s  [%s]", cg.draft.name.c_str(),
                    tr.tr(cg.draft.gender ? "Female" : "Male").c_str());
      tl.add(x, y, nm, 15, PX_BODY); y += 16;
      // 剩餘點數。
      char pl[64];
      std::snprintf(pl, sizeof pl, "%s: %d", tr.tr("Points left").c_str(),
                    cg.draft.leftover());
      tl.add(x, y, pl, cg.draft.leftover() > 0 ? 14 : 11, PX_BODY); y += 18;
      // 四屬性列(游標 > 高亮)。
      const char* labels[4] = {"Strength", "Dexterity", "Intel", "Spirit"};
      for (int i = 0; i < 4; ++i) {
        bool cur = (i == cg.cursor);
        char row[64];
        std::snprintf(row, sizeof row, "%s%-10s %2d", cur ? "> " : "  ",
                      tr.tr(labels[i]).c_str(), (int)cg.draft.attr[i]);
        tl.add(x, y, row, cur ? 15 : 7, PX_BODY);
        y += 14;
      }
      y += 4;
      // 衍生值(HP/STUN/PWR/AV/DV)。
      char dv[96];
      std::snprintf(dv, sizeof dv, "%s %d   %s %d   %s %d",
                    tr.tr("HP").c_str(), cg.draft.derived_hp(),
                    tr.tr("Stun").c_str(), cg.draft.derived_stun(),
                    tr.tr("PWR").c_str(), cg.draft.derived_power());
      tl.add(x, y, dv, 11, PX_UI); y += 12;
      char dv2[64];
      std::snprintf(dv2, sizeof dv2, "%s %d   %s %d", tr.tr("AV").c_str(),
                    cg.draft.base_av(), tr.tr("DV").c_str(), cg.draft.base_dv());
      tl.add(x, y, dv2, 11, PX_UI);
      // 操作提示移到面板下方(全寬,不擠進面板)。
      tl.add(8, 178, tr.tr("Up/Down select  +/- adjust  G gender  Enter done"), 8, PX_UI);
      tl.add(8, 189, tr.tr("B: begin  N: add member  Esc: back"), 8, PX_UI);
    }

    // 右面板:已建隊員清單。
    int rx = 214, ry = 30;
    tl.add(rx, ry, tr.tr("Party"), ps.accent, PX_UI); ry += 16;
    if (cg.done_records.empty()) {
      tl.add(rx, ry, tr.tr("(none yet)"), 7, PX_UI);
    }
    for (std::size_t i = 0; i < cg.done_records.size(); ++i) {
      // 直接解析名字(高位元終止)供顯示。
      std::string nm;
      const auto& rec = cg.done_records[i];
      for (int k = 0; k < 12; ++k) {
        char c = (char)(rec[k] & 0x7F);
        if (c >= 0x20 && c < 0x7F) nm.push_back(c);
        if ((rec[k] & 0x80) == 0) break;
      }
      char line[48];
      std::snprintf(line, sizeof line, "%d. %s", (int)i + 1, nm.c_str());
      tl.add(rx, ry, line, 15, PX_UI); ry += 13;
    }
  };

  auto draw_menu = [&]() {
    fb.clear(1);
    add_title();
    add_lang_badge();
    int y = 40;
    // 目前隊伍清單(DOS 主選單語意:「Current party… 1)..4) + Begin the game」)。
    //   remake 啟動已載入預設 / 已建隊伍(party);在選單同屏疊出隊伍,貼近原版整合樣貌。
    //   空隊(理論上不會發生,預設四人)則略過此區,只顯示選項。
    if (party.size() > 0) {
      tl.add(16, y, tr.tr("Current party..."), 11, PX_BODY); y += 14;
      for (std::size_t i = 0; i < party.size(); ++i) {
        std::string line = std::to_string(i + 1) + ") " + party.at(i).name;
        if (party.at(i).status & 0x01) line += " (" + tr.tr("unconscious") + ")";  // 昏倒標記
        tl.add(32, y, line, 15, PX_BODY); y += 13;
      }
      y += 6;  // 隊伍清單與選項間留白
    }
    if (!header.empty()) { tl.add(16, y, header, 7, PX_BODY); y += 14; }
    for (std::size_t i = 0; i < opts.size(); ++i) {
      bool cur = (int)i == sel;
      std::uint8_t col = cur ? 14 : 15;
      std::string line;
      if (cur) line += "> ";
      if (opts[i].hot) { line += opts[i].hot; line += ") "; }
      line += opts[i].label;
      tl.add(16, y, line, col, PX_BODY);
      y += 14;
    }
  };
  auto draw_branch = [&]() {
    fb.clear(1);
    add_title();
    add_lang_badge();
    tl.add(16, 60, branch_label, 14, PX_BODY);
    tl.add(16, 110, "(game screen - to be implemented)", 7, PX_UI);
    tl.add(16, 140, "Esc: back   Q: quit", 8, PX_UI);
  };
  // ── 遭遇畫面:怪物 index → 可用 bundle sprite。──
  // 誠實揭露:res31 record byte[0x0B] 推出的 sprite 編號與實際 sprite 資源有偏差
  //   (docs/reverse-engineering/26_MONSTERS_AND_SPRITES.md 已記:需逐一視覺核對),故此處用「怪物名 → 已
  //    視覺核對過的 bundle sprite」對照表;查無則回退第一個 spider/wolf,再無則畫空框。
  //   sprite 圖渲染路徑本身(.spr indexed blit)已由 sprite_dump golden 對拍 oracle。
  auto sprite_for_monster = [&](const std::string& name,
                                int sprite_res = -1) -> std::optional<render::Sprite> {
    // theme-aware 來源:Amiga theme → themes/amiga/sprites(自帶 palette);否則 DOS bundle/sprites。
    //   檔名沿用 DOS 命名(152_guard / 196_spider …);Amiga 缺檔時回退 DOS bundle(誠實降級)。
    const std::string dir = theme.sprite_dir.empty()
                                ? (bundle + "/sprites/")
                                : (bundle + "/" + theme.sprite_dir + "/");
    auto load = [&](const char* file) -> std::optional<render::Sprite> {
      if (auto s = render::Sprite::load(dir + file + ".spr")) return s;
      return render::Sprite::load(bundle + "/sprites/" + file + ".spr");  // 回退 DOS
    };
    // 1) 權威對映:MonsterRecord::sprite_res() = (attr[0x0B]<<1)+0x8A 指向 data4 怪物 sprite
    //    資源編號(逐記錄精確;25 隻 monsters.bin 全落在已抽的偶數資源)。Amiga theme 下
    //    優先載 themes/amiga/sprites/<res>.spr(全套 50 隻 Amiga 立繪);命中即用,確保
    //    F8 切 Amiga 時各怪物呈現自己的 Amiga 美術(不再只有名稱關鍵字命中的 6 隻)。
    if (sprite_res > 0 && !theme.sprite_dir.empty()) {
      if (auto s = render::Sprite::load(dir + std::to_string(sprite_res) + ".spr")) return s;
    }
    // 2) 名稱關鍵字 → 已核對 sprite 檔(視覺核對來源:docs/reverse-engineering/26 contact sheet;DOS / Amiga 缺檔回退)。
    if (name.find("Spider") != std::string::npos) return load("196_spider");
    if (name.find("Wolf") != std::string::npos)   return load("168_wolf");
    if (name.find("Dog") != std::string::npos || name.find("hound") != std::string::npos)
      return load("168_wolf");
    if (name.find("Guard") != std::string::npos || name.find("Soldier") != std::string::npos ||
        name.find("Keeper") != std::string::npos || name.find("Pikeman") != std::string::npos ||
        name.find("Gladiator") != std::string::npos)
      return load("152_guard");
    if (name.find("Fanatic") != std::string::npos || name.find("Loon") != std::string::npos)
      return load("222_fanatic");
    if (name.find("Innocent") != std::string::npos || name.find("Accused") != std::string::npos)
      return load("200_innocent_man");
    // 其餘人類敵人(Robber/Bandit/Drunk/Cannibal/…)用 pikeman 立繪占位。
    if (auto s = load("210_pikeman")) return s;
    return load("196_spider");
  };
  // 進遭遇:設 sprite + 結算單位 + RNG(seed 來自 --combat-seed)。
  auto begin_encounter = [&](int idx) {
    if (idx < 0 || idx >= (int)monsters.size()) return;
    enc = EncounterState{};
    enc.active = true;
    enc.monster_idx = idx;
    enc.mon_name_en = monsters[idx].name;
    enc.sprite_res = (int)monsters[idx].sprite_res();
    enc.sprite = sprite_for_monster(monsters[idx].name, enc.sprite_res);
    enc.rng = game::CombatRng((std::uint16_t)combat_seed);
    if (party.size() > 0) {
      const auto& c0 = party.at(0);
      enc.hero = game::Combatant::from_player(c0);
      enc.hero_power = (int)c0.power;        // 施法者法力池(record Power[28-31])
      enc.hero_str = (int)c0.strength;       // PowerScaled / +STR 結算用
      enc.hero_int = (int)c0.intel;          // Zap 攻擊判定(docs/gameplay/58_MAGIC_REFERENCE.md 門檻含 INT)
      // 魔法技能 ranks:技能槽→法術系對映未反編出(受阻)→ 以角色等級當保守 proxy(誠實標示)。
      enc.hero_ranks = (int)c0.level;
      enc.spellbook = game::castable_spells(c0, enc.hero_power);  // 已習得且可施法
    } else { enc.hero.name = "Hero"; enc.hero.is_player = true; enc.hero.hp = enc.hero.max_hp = 20;
           enc.hero.av = 5; enc.hero.dv = 5; enc.hero.ac = 0; enc.hero.dmg_dice = 1; enc.hero.dmg_sides = 6;
           enc.hero_power = 0; enc.hero_str = 12; enc.hero_int = 12; enc.hero_ranks = 1; enc.spellbook.clear(); }
    enc.mon = game::Combatant::from_monster(monsters[idx]);
    // ── 完整戰鬥迴圈:整隊(存活成員)vs 怪群 ──
    //   怪群數量 = combat_count(預設 6;沿用怪物表領頭怪 monsters[idx])。
    //   行動順序 / 目標選擇見 combat_loop.hpp(remake 設計,SDA 定性;非原版真值)。
    enc.group = true;
    enc.mon_count = combat_count > 0 ? combat_count : 1;
    std::vector<game::Combatant> party_units;
    if (party.size() > 0) {
      for (std::size_t i = 0; i < party.size(); ++i)
        party_units.push_back(game::Combatant::from_player(party.at(i)));
    } else {
      party_units.push_back(enc.hero);  // 無隊伍 → 用 fallback hero
    }
    auto grp = game::make_monster_group(monsters[idx], enc.mon_count);
    enc.group_loop.emplace(std::move(party_units), std::move(grp),
                           game::CombatRng((std::uint16_t)combat_seed));
    enc.shown_events = 0;
    state = S_COMBAT;
    std::fprintf(stderr,
                 "begin_encounter: %d x '%s' party=%zu hero_hp=%d mon_hp=%d\n",
                 enc.mon_count, enc.mon_name_en.c_str(), party.size(), enc.hero.hp,
                 enc.mon.hp);
  };
  // ── 終戰 Namtar:隊伍(自由之劍持有者受祝福)vs Namtar Boss(combat_loop)──
  //   接 area27 tile 0x18/0x19 op_8A combat encounter(怪物 id 來源見 combat.hpp 檔頭:
  //   res3 設定流程產物 0x03,非乾淨 res31 索引 → Boss 屬性為 remake 設計,誠實標示)。
  //   隊伍第 0 名套「自由之劍受祝福」加成(namtar_blessed),其餘成員照常 from_player。
  auto begin_namtar = [&]() {
    enc = EncounterState{};
    enc.active = true;
    enc.is_namtar = true;
    enc.mon_name_en = "Namtar";
    enc.sprite_res = 218;                         // Humbaba sprite 資源(Amiga 全套含此隻)
    enc.sprite = sprite_for_monster("Humbaba", enc.sprite_res);  // 占位立繪(無 Namtar 專屬 sprite;誠實:借胡姆巴巴像)
    std::vector<game::Combatant> party_units;
    if (party.size() > 0) {
      for (std::size_t i = 0; i < party.size(); ++i) {
        // 隊伍第 0 名持自由之劍(受祝福);namtar_blessed=false 時全員照常(--no-bless)。
        if (i == 0 && namtar_blessed)
          party_units.push_back(game::make_blessed_hero(party.at(0)));
        else
          party_units.push_back(game::Combatant::from_player(party.at(i)));
      }
    } else {
      // 無隊伍 fallback(理論上有預設隊伍):一名受祝福勇者。
      game::Combatant h; h.name = "Hero"; h.is_player = true;
      h.hp = h.max_hp = 60; h.av = 12; h.dv = 10; h.ac = 0;
      h.dmg_dice = 4; h.dmg_sides = 12; h.dmg_bonus = 20;
      party_units.push_back(h);
    }
    std::vector<game::Combatant> grp;
    grp.push_back(game::make_namtar());  // 單一終戰 Boss
    enc.group = true;
    enc.mon_count = 1;
    enc.group_loop.emplace(std::move(party_units), std::move(grp),
                           game::CombatRng((std::uint16_t)combat_seed));
    if (party.size() > 0) {
      enc.hero = enc.group_loop->party().at(0);
      const auto& c0 = party.at(0);
      enc.hero_power = (int)c0.power; enc.hero_str = (int)c0.strength;
      enc.hero_int = (int)c0.intel; enc.hero_ranks = (int)c0.level;  // Zap 判定;ranks proxy=level(受阻)
      enc.spellbook = game::castable_spells(c0, enc.hero_power);
    }
    enc.shown_events = 0;
    state = S_COMBAT;
    std::fprintf(stderr,
                 "begin_namtar: party=%zu blessed=%d namtar_hp=%d seed=0x%X\n",
                 party.size(), (int)namtar_blessed,
                 enc.group_loop->monsters().at(0).hp, combat_seed);
  };
  // 把 CombatLoop 累積的新事件轉成在地化戰報行,追加進 enc.log。
  //   DOS 格式(docs/reverse-engineering/43 §11):
  //     命中:「{攻擊者} 攻擊 {目標},命中 N 點傷害[,使其暈眩][,將其擊倒]。」
  //     落空:「{攻擊者} 攻擊 {目標},落空。」
  //   以 tr() 翻可翻片段(攻擊/命中/點傷害/落空/使其暈眩/將其擊倒/怪名),組成整行。
  auto append_group_events = [&]() {
    if (!enc.group_loop) return;
    const auto& evs = enc.group_loop->events();
    char buf[256];
    for (; enc.shown_events < evs.size(); ++enc.shown_events) {
      const auto& e = evs[enc.shown_events];
      std::string atk = tr.tr(e.attacker);
      std::string tgt = tr.tr(e.target);
      std::string line;
      // 控制事件(被眩目跳過 / 控制施法套用)— i18n 專屬訊息。
      if (e.dazed_skip) {
        std::snprintf(buf, sizeof buf, tr.tr("%s is too disoriented to act!").c_str(),
                      atk.c_str());
        enc.log.emplace_back(buf);
        continue;
      }
      if (e.target_fled) {
        std::snprintf(buf, sizeof buf, tr.tr("%s flees in terror!").c_str(), tgt.c_str());
        enc.log.emplace_back(buf);
        continue;
      }
      if (e.dazed_applied) {
        std::snprintf(buf, sizeof buf, tr.tr("%s is disoriented!").c_str(), tgt.c_str());
        enc.log.emplace_back(buf);
        continue;
      }
      // 特殊攻擊事件(remake 設計 grounded 手冊)。
      if (e.special_dodge) {  // 閃避姿態:不攻擊,本回合 DV 提高
        std::snprintf(buf, sizeof buf, tr.tr("%s takes a defensive stance!").c_str(),
                      atk.c_str());
        enc.log.emplace_back(buf);
        continue;
      }
      if (e.special_disarm) {  // 卸武裝命中:打掉敵人武器(本次不造成身體傷害)
        std::snprintf(buf, sizeof buf, tr.tr("%s is disarmed!").c_str(), tgt.c_str());
        enc.log.emplace_back(buf);
        continue;
      }
      if (e.hit) {
        g_sound.play(audio::SoundId::Hit);   // 命中音效(remake 設計;見 sound.hpp)
        if (e.attacker_is_player) enc.hit_flash = 8;  // 我方命中怪物 → 怪物立繪閃白受擊(8 幀)
        // 模板鍵「combat.hit.fmt」帶 3 槽:%1$=攻擊者 %2$=目標 %3$=傷害值。
        //   zh-TW:「%s 攻擊 %s,命中 %d 點傷害」;en passthrough:「%s attacks %s for %d damage」。
        std::snprintf(buf, sizeof buf, tr.tr("%s attacks %s for %d damage").c_str(),
                      atk.c_str(), tgt.c_str(), e.damage);
        line = buf;
        if (e.stunned) line += tr.tr(", stunning him");
        if (e.target_died) line += tr.tr(", killing him");
      } else {
        std::snprintf(buf, sizeof buf, tr.tr("%s attacks %s and misses").c_str(),
                      atk.c_str(), tgt.c_str());
        line = buf;
      }
      enc.log.emplace_back(line);
    }
    // 畫面只放最後 4 行(對齊版面;結束時讓勝負/XP 末行可見,不被擠出 200px 視窗)。
    while (enc.log.size() > 4) enc.log.erase(enc.log.begin());
  };
  // 戰後成長:對全隊跑升級檢查(XP 達門檻 → level+1、+2 AP、HP/STUN/STR 提升)。
  //   每名升級者推一行戰報(i18n「{名} 升到 {等級} 級!」)。grounded:+2AP=SDA、
  //   STR/HP 隨等級=手冊;XP 曲線=remake(見 progression.hpp)。award_xp 後呼叫。
  auto level_up_party = [&]() {
    for (std::size_t i = 0; i < party.size(); ++i) {
      game::LevelUpResult lr = game::check_level_up(party.at(i));
      if (lr.leveled()) {
        char buf[160];
        std::snprintf(buf, sizeof buf, tr.tr("%s reaches level %d!").c_str(),
                      party.at(i).name.c_str(), lr.new_level);
        enc.log.emplace_back(buf);
      }
    }
    while (enc.log.size() > 4) enc.log.erase(enc.log.begin());
  };
  // 推進一個完整群戰回合 + 轉戰報 + 結算勝負/XP。
  auto group_round = [&]() {
    if (!enc.group_loop || enc.over) return;
    enc.group_loop->advance_round();
    append_group_events();
    using O = game::CombatOutcome;
    O o = enc.group_loop->outcome();
    if (o == O::Victory) {
      enc.victory = true; enc.over = true;
      enc.log.emplace_back(tr.tr("Each member gets 80 experience points for combat."));
      if (!enc.xp_awarded) { party.award_xp(game::kXpPerVictory); enc.xp_awarded = true; } level_up_party();
    } else if (o == O::Defeat) {
      enc.defeat = true; enc.over = true;
      enc.log.emplace_back(tr.tr("The party has fallen."));
    }
    while (enc.log.size() > 4) enc.log.erase(enc.log.begin());
  };
  // 特殊攻擊一回合(remake 設計 grounded 手冊;見 combat.hpp SpecialAttack)。
  //   隊伍第 0 名對首個參戰怪用 type 特殊攻擊 → 轉戰報 → 怪反擊一回合(同 cast 模式)。
  //   Dodge 不攻擊也不挑目標:只提高自身 DV,接著怪反擊(被命中率因 DV 加成下降)。
  auto special_attack_round = [&](game::SpecialAttack type) {
    if (!enc.group_loop || enc.over) return;
    game::AttackResult ar =
        enc.group_loop->special_attack(type, /*actor_is_player=*/true, 0, -1);
    (void)ar;
    append_group_events();
    using O = game::CombatOutcome;
    O o = enc.group_loop->outcome();
    if (o == O::Victory) {
      enc.victory = true; enc.over = true;
      enc.log.emplace_back(tr.tr("Each member gets 80 experience points for combat."));
      if (!enc.xp_awarded) { party.award_xp(game::kXpPerVictory); enc.xp_awarded = true; }
      level_up_party();
    } else {
      group_round();  // 怪反擊一回合(Dodge 時享 DV 加成)
    }
    while (enc.log.size() > 4) enc.log.erase(enc.log.begin());
  };
  // 把 --combat-special <type> 字串解析為 SpecialAttack(無效回 Normal)。
  auto parse_special = [](const std::string& s) -> game::SpecialAttack {
    using SA = game::SpecialAttack;
    if (s == "mighty")  return SA::MightyBlow;
    if (s == "disarm")  return SA::Disarm;
    if (s == "advance") return SA::Advance;
    if (s == "quick")   return SA::QuickFight;
    if (s == "dodge")   return SA::Dodge;
    return SA::Normal;
  };
  // 群戰施法回合:隊伍第 0 名施放 spell_id(走 CombatLoop::cast,依 SpellTarget 自動鋪對象)。
  //   傷害/治療/buff/控制全部結算(grounded 手冊;控制持續/逃離為 remake 設計)。
  //   施法後若戰鬥未結束 → 推進一個怪群回合(怪反擊)。Power 照扣。
  auto group_cast_round = [&](std::uint8_t spell_id) {
    if (!enc.group_loop || enc.over) return;
    const game::SpellDef* sp = game::find_spell(spell_id);
    if (!sp) return;
    // 召喚守則(任務要求):戰鬥陣營已達 7 人(kMaxPartyMembers,含既有召喚物)→ 不可再召喚。
    if (game::summon_kind_of(spell_id) != game::SummonKind::None &&
        (int)enc.group_loop->party().size() >= game::kMaxPartyMembers) {
      enc.log.emplace_back(tr.tr("The party is full."));
      return;
    }
    game::CastResult cr = enc.group_loop->cast(spell_id, enc.hero_power, enc.hero_str,
                                               /*caster_is_player=*/true,
                                               enc.hero_int, enc.hero_ranks, /*power_points=*/1);
    if (!cr.ok) { enc.log.emplace_back(tr.tr("Not enough power")); return; }
    g_sound.play(audio::SoundId::Cast);   // 施法音效(remake 設計;見 sound.hpp)
    enc.hero_power -= cr.power_spent;  // 扣法力(寫回 encounter 狀態)
    // 施法戰報(英文鍵化;tr 在地化)。
    char buf[192];
    std::string caster = party.size() > 0 ? tr.tr(party.at(0).name) : tr.tr("Hero");
    if (sp->effect == game::SpellEffect::Control) {
      // 控制類 i18n:Daze→「{怪} 迷失了」;Flee→「{怪} 嚇得逃跑」;其餘(Disarm/Dispel)→泛用。
      const char* key = "%s casts %s";  // 預設
      if (cr.control == game::ControlKind::Daze) key = "%s is disoriented!";
      else if (cr.control == game::ControlKind::Flee) key = "%s flees in terror!";
      else if (cr.control == game::ControlKind::Disarm) key = "%s is disarmed!";
      if (cr.control == game::ControlKind::Daze || cr.control == game::ControlKind::Flee ||
          cr.control == game::ControlKind::Disarm) {
        std::snprintf(buf, sizeof buf, tr.tr(key).c_str(), tr.tr(enc.mon_name_en).c_str());
      } else {  // Dispel(幻影現形)等:泛用「{施法者} 施放 {法術}」。
        std::snprintf(buf, sizeof buf, tr.tr("%s casts %s").c_str(),
                      caster.c_str(), tr.tr(sp->name_key).c_str());
      }
    } else if (cr.summon != game::SummonKind::None) {  // 召喚:臨時友方加入(cast 已加事件)
      std::snprintf(buf, sizeof buf, tr.tr("%s casts %s").c_str(), caster.c_str(),
                    tr.tr(sp->name_key).c_str());
    } else if (sp->effect == game::SpellEffect::Heal) {
      std::snprintf(buf, sizeof buf, "%s casts %s heals %d", caster.c_str(),
                    sp->name_key, cr.amount);
    } else if (cr.handled && cr.is_zap && !cr.zap_hit) {  // Zap miss:仍吃半傷(docs/gameplay/58_MAGIC_REFERENCE.md)
      std::snprintf(buf, sizeof buf, "%s casts %s on %s grazes %d damage", caster.c_str(),
                    sp->name_key, tr.tr(enc.mon_name_en).c_str(), cr.amount);
    } else if (cr.handled && cr.amount > 0) {  // 傷害類(命中)
      std::snprintf(buf, sizeof buf, "%s casts %s on %s %d damage", caster.c_str(),
                    sp->name_key, tr.tr(enc.mon_name_en).c_str(), cr.amount);
    } else if (cr.handled) {  // buff/debuff
      std::snprintf(buf, sizeof buf, tr.tr("%s casts %s").c_str(), caster.c_str(),
                    tr.tr(sp->name_key).c_str());
    } else {  // 工具類(光源/補給/指引等探索態,戰鬥內無數值效果)
      std::snprintf(buf, sizeof buf, tr.tr("%s casts %s").c_str(), caster.c_str(),
                    tr.tr(sp->name_key).c_str());
    }
    enc.log.emplace_back(buf);
    enc.shown_events = enc.group_loop->events().size();  // cast 已自行追加事件,跳過免重複翻譯
    // 控制清場 / 致死可能提早結束。
    using O = game::CombatOutcome;
    O o = enc.group_loop->outcome();
    if (o == O::Victory) {
      enc.victory = true; enc.over = true;
      enc.log.emplace_back(tr.tr("Each member gets 80 experience points for combat."));
      if (!enc.xp_awarded) { party.award_xp(game::kXpPerVictory); enc.xp_awarded = true; } level_up_party();
    } else {
      group_round();  // 戰鬥續行 → 怪反擊一回合(append_group_events 接續)
    }
    // 施法後刷新可施法清單(Power 改變)。
    if (party.size() > 0)
      enc.spellbook = game::castable_spells(party.at(0), enc.hero_power);
    while (enc.log.size() > 4) enc.log.erase(enc.log.begin());
  };
  // 一個攻擊回合(隊伍先攻 → 怪反擊);把訊息(英文鍵)推進 enc.log。
  auto combat_round = [&]() {
    if (enc.over) return;
    char buf[96];
    auto ph = game::resolve_attack(enc.hero, enc.mon, enc.rng);
    std::snprintf(buf, sizeof buf, "%s -> %s : %s%s", enc.hero.name.c_str(),
                  enc.mon_name_en.c_str(), ph.hit ? "hit" : "miss",
                  ph.hit ? (std::string(" ") + std::to_string(ph.damage)).c_str() : "");
    enc.log.emplace_back(buf);
    if (!enc.mon.alive()) {
      std::snprintf(buf, sizeof buf, "%s slain", enc.mon_name_en.c_str());
      enc.log.emplace_back(buf); enc.over = true; return;
    }
    auto pm = game::resolve_attack(enc.mon, enc.hero, enc.rng);
    std::snprintf(buf, sizeof buf, "%s -> %s : %s%s", enc.mon_name_en.c_str(),
                  enc.hero.name.c_str(), pm.hit ? "hit" : "miss",
                  pm.hit ? (std::string(" ") + std::to_string(pm.damage)).c_str() : "");
    enc.log.emplace_back(buf);
    if (!enc.hero.alive()) enc.over = true;
    // log 只保留最後 4 行(畫面空間)。
    while (enc.log.size() > 4) enc.log.erase(enc.log.begin());
  };
  // 施法回合:hero 施放 spell_id(對 mon)→ 扣 Power → 怪反擊(同 combat_round 後半)。
  // 傷害/治療作用於 STUN;buff 作用於 hero(單體目標 = hero 自己,group 簡化為 hero);
  // 控制/工具類只扣 Power(handled=false,誠實標 TODO)。grounded in 手冊(spells.hpp)。
  auto cast_round = [&](std::uint8_t spell_id) {
    if (enc.over) return;
    const game::SpellDef* sp = game::find_spell(spell_id);
    if (!sp) return;
    char buf[160];
    // 治療/buff 類目標為我方(hero);其餘(傷害/控制)目標為怪物。
    bool ally_target = (sp->effect == game::SpellEffect::Heal ||
                        sp->effect == game::SpellEffect::BuffAv ||
                        sp->effect == game::SpellEffect::BuffDv ||
                        sp->effect == game::SpellEffect::BuffAc ||
                        sp->effect == game::SpellEffect::BuffStr ||
                        sp->effect == game::SpellEffect::BuffDex);
    game::Combatant& tgt = ally_target ? enc.hero : enc.mon;
    game::CastResult cr =
        game::cast_spell(spell_id, enc.hero_power, enc.hero_str, tgt, enc.rng,
                         enc.hero_int, enc.hero_ranks, /*power_points=*/1);
    if (!cr.ok) {  // Power 不足(理論上選單已過濾)→ 提示,不消回合
      enc.log.emplace_back("Not enough power");
      return;
    }
    enc.hero_power -= cr.power_spent;  // 扣法力(寫回 encounter 狀態)
    // 戰報(英文鍵化;tr 在地化)。
    if (sp->effect == game::SpellEffect::Heal) {
      std::snprintf(buf, sizeof buf, "%s casts %s heals %d", enc.hero.name.c_str(),
                    sp->name_key, cr.amount);
    } else if (cr.handled && cr.amount > 0) {  // 傷害類
      std::snprintf(buf, sizeof buf, "%s casts %s on %s %d damage",
                    enc.hero.name.c_str(), sp->name_key, enc.mon_name_en.c_str(),
                    cr.amount);
    } else if (cr.handled) {  // buff/debuff(amount 為加值)
      std::snprintf(buf, sizeof buf, "%s casts %s", enc.hero.name.c_str(),
                    sp->name_key);
    } else {  // 控制/工具(TODO)
      std::snprintf(buf, sizeof buf, "%s casts %s TODO", enc.hero.name.c_str(),
                    sp->name_key);
    }
    enc.log.emplace_back(buf);
    if (!enc.mon.alive()) {
      std::snprintf(buf, sizeof buf, "%s slain", enc.mon_name_en.c_str());
      enc.log.emplace_back(buf);
      enc.over = true;
      while (enc.log.size() > 4) enc.log.erase(enc.log.begin());
      return;
    }
    // 怪反擊(同 combat_round 後半:怪物對 hero 一次物理攻擊)。
    auto pm = game::resolve_attack(enc.mon, enc.hero, enc.rng);
    std::snprintf(buf, sizeof buf, "%s -> %s : %s%s", enc.mon_name_en.c_str(),
                  enc.hero.name.c_str(), pm.hit ? "hit" : "miss",
                  pm.hit ? (std::string(" ") + std::to_string(pm.damage)).c_str() : "");
    enc.log.emplace_back(buf);
    if (!enc.hero.alive()) enc.over = true;
    // 施法後刷新可施法清單(Power 改變 → 某些法術可能不再可施)。
    if (party.size() > 0)
      enc.spellbook = game::castable_spells(party.at(0), enc.hero_power);
    while (enc.log.size() > 4) enc.log.erase(enc.log.begin());
  };
  // 畫遭遇畫面:怪物 sprite(viewport 區 @16,8)+ 怪名 + 隊伍面板 + 戰/逃選單 + log。
  auto draw_encounter = [&]() {
    fb.clear(0);
    // ── Amiga theme:套當前怪物 sprite 自帶 16 色盤(各怪物色系不同)──
    //   Amiga 怪物 sprite 每隻自帶 palette(spider 綠金 / wolf 棕 / fanatic 藍紅),
    //   故 combat 整畫面以此 sprite 盤呈現(index 0=黑 1=白 8=紅 各盤一致,UI/backdrop
    //   仍可讀)。無 Amiga sprite 時維持 theme.palette(F8 切回時亦由下方 set_palette 還原)。
    // 像素層(UI chrome / 面板 / 邊框)palette:Amiga 用 viewport 盤(與探索畫面一致的藍框/青柱),
    //   DOS 用主題盤(golden byte-for-byte 不動)。Amiga viewport 內部另由 region RGB 覆寫(見下)。
    const bool amiga_combat = theme.sprite_own_palette;  // Amiga = true
    vid.set_palette(amiga_combat ? render::kAmigaViewportPalette : theme.palette);
    int ph = anim_tick % 48;
    int bob = (ph < 24) ? (ph / 12) : ((47 - ph) / 12);   // idle 呼吸 0,0..1,1..0 三角波(±1px)
    bool flash = (enc.hit_flash > 0) && ((enc.hit_flash / 2) % 2 == 0);  // 受擊閃白相位
    if (amiga_combat) {
      // ── remake 加值:viewport 在 RGB 層合成「土黃地牢牆 + 鮮豔怪物」,突破 16 色單盤隔閡 ──
      //   牆面:render_first_person → 暫存 fb,以 kAmigaViewportPalette(校準自真機)轉 RGB。
      //   怪物:sprite 自帶盤逐像素轉 RGB,置中疊在牆 RGB 上(bob + 受擊閃白)。透明色跳過 →
      //   露出牆。最後 set_region_rgb 覆寫 viewport 矩形(SdlVideo compose 套用)。
      std::vector<render::Rgb> vrgb((std::size_t)kVpW * kVpH, render::Rgb{0, 0, 0});
      if (level) {
        render::Framebuffer wfb; wfb.clear(0);
        render::ViewportDecoder bdec;
        render::render_first_person(*level, px, py, dir, bdec, comps);
        if (vpt_ok)
          bdec.compose_frame(vpt[0].data(), vpt[1].data(), vpt[2].data(), vpt[3].data());
        bdec.to_framebuffer(wfb);
        for (int yy = 0; yy < kVpH; ++yy)
          for (int xx = 0; xx < kVpW; ++xx)
            vrgb[(std::size_t)yy * kVpW + xx] =
                render::kAmigaViewportPalette[wfb.idx[(std::size_t)(kVpY + yy) * render::kW + (kVpX + xx)] & 0xF];
      }
      if (enc.sprite) {
        const auto& s = *enc.sprite;
        int bx = (kVpW - (int)s.w) / 2; if (bx < 0) bx = 0;
        int by = (kVpH - (int)s.h) / 2 - bob; if (by < 0) by = 0;
        for (int yy = 0; yy < (int)s.h; ++yy) {
          int vy = by + yy; if (vy < 0 || vy >= kVpH) continue;
          for (int xx = 0; xx < (int)s.w; ++xx) {
            int vx = bx + xx; if (vx < 0 || vx >= kVpW) continue;
            std::uint8_t i = s.idx[(std::size_t)yy * s.w + xx];
            if (theme.sprite_transparent >= 0 && i == (std::uint8_t)theme.sprite_transparent) continue;
            vrgb[(std::size_t)vy * kVpW + vx] =
                flash ? render::Rgb{0xFF, 0xFF, 0xFF}
                      : (i < (int)s.palette.size() ? s.palette[(std::size_t)i] : render::Rgb{0, 0, 0});
          }
        }
      }
      vid.set_region_rgb(std::move(vrgb), kVpX, kVpY, kVpW, kVpH);
    } else {
      // ── DOS:viewport 上半天空 + 下半地面(石礫網點)backdrop,怪物畫進 fb(golden 不動)──
      const auto& bd = theme.combat;
      int hy = kVpY + bd.horizon;   // 天空/地面分界(絕對 y)
      for (int y = kVpY; y < kVpY + kVpH; ++y) {
        bool ground = (y >= hy);
        for (int x = kVpX; x < kVpX + kVpW; ++x) {
          std::uint8_t c = ground ? bd.ground : bd.sky;
          if (ground && (((x + y) & 1) == 0)) c = bd.ground_dot;
          fb.put(x, y, c);
        }
      }
      if (enc.sprite) {
        int sy = 8 - bob;   // DOS 立繪落點固定 (16,8)(golden);上浮 = y 減
        if (flash) {
          const auto& s = *enc.sprite;
          for (int y = 0; y < (int)s.h; ++y) {
            int fy = sy + y;
            if (fy < kVpY || fy >= kVpY + kVpH) continue;
            for (int x = 0; x < (int)s.w; ++x) {
              int fx = 16 + x;
              if (fx < kVpX || fx >= kVpX + kVpW) continue;
              std::uint8_t i = s.idx[(std::size_t)y * s.w + x];
              if (theme.sprite_transparent >= 0 && i == (std::uint8_t)theme.sprite_transparent) continue;
              fb.put(fx, fy, 15);                               // index 15 = 白(DOS 盤;index 1 是藍,勿用)
            }
          }
        } else {
          enc.sprite->blit_clipped(fb, 16, sy, theme.sprite_transparent,
                                   kVpX, kVpY, kVpX + kVpW, kVpY + kVpH);
        }
      } else { // 無 sprite → 畫空框(像素層),維持版面對齊。
        for (int x = 16; x < 16 + 160; ++x) { fb.put(x, 8, 8); fb.put(x, 8 + 135, 8); }
        for (int y = 8; y < 8 + 136; ++y) { fb.put(16, y, 8); fb.put(16 + 159, y, 8); }
      }
    }
    // UI chrome(藍外框 + logo;docs/gameplay/59 #1/#2)。畫在 viewport / 面板框外,不蓋立繪。
    draw_explore_chrome();
    // 怪群描述(i18n;viewport 上方):「N 隻 {怪名}」(存活/總數)。單怪時只顯示怪名。
    //   白字對齊原版場景/怪名色(docs/gameplay/59 #6)。
    if (enc.group && enc.group_loop && enc.mon_count > 1) {
      char gbuf[128];
      // zh-TW:「6 隻 禁衛軍(存活 4)」;en passthrough:「6 King's Guard (4 left)」。
      std::snprintf(gbuf, sizeof gbuf, tr.tr("%d %s (%d left)").c_str(),
                    enc.mon_count, tr.tr(enc.mon_name_en).c_str(),
                    enc.group_loop->monsters_alive());
      tl.add(16, 2, gbuf, 15, PX_UI);
    } else {
      tl.add(16, 2, tr.tr(enc.mon_name_en), 15, PX_UI);
    }
    // 右側隊伍狀態面板(沿用 party_panel)。
    party.draw_status_panel(fb, tl, PX_UI);
    add_lang_badge();
    // ── 底部寬訊息框(全寬;對齊原版「戰鬥旁白 + 行動選單在大框」)──────────────
    //   原版把怪群描述 + 行動選單 + 戰報放在右側大框;remake 右側改成隊伍 HP 面板,故把
    //   行動選單 + 戰報旁白放底部全寬框(viewport 下方),避免擠在右側窄欄被 viewport
    //   chrome(綠柱)蓋住、看不清。
    const int cbX0 = kMsgStripX0, cbX1 = kPanelX1 + 3;           // 14 .. 315(全寬)
    const int cbY0 = kMsgStripY0, cbY1 = kMsgStripY1;            // 150 .. 192
    for (int y = cbY0; y <= cbY1; ++y) for (int x = cbX0; x <= cbX1 && x < render::kW; ++x) fb.put(x, y, 0);
    frame_rect(cbX0, cbY0, cbX1, cbY1, 9);                       // 亮藍外框
    const int LPX = PX_UI * 3 / 4;                               // 框內字級(較小,容更多字)
    // 施法選單(C 開啟):全寬框內橫向排列可施法術。
    if (enc.casting) {
      char hbuf[160];
      std::snprintf(hbuf, sizeof hbuf, "%s (PW %d)", tr.tr("Choose spell:").c_str(), enc.hero_power);
      tl.add(cbX0 + 4, cbY0 + 3, hbuf, 14, LPX);
      if (enc.spellbook.empty()) tl.add(cbX0 + 10, cbY0 + 16, tr.tr("No spells"), 7, LPX);
      else {
        int sx = cbX0 + 6, sy = cbY0 + 16;
        for (int i = 0; i < (int)enc.spellbook.size() && i < 9; ++i) {
          const game::SpellDef* s = game::find_spell(enc.spellbook[i]);
          if (!s) continue;
          std::snprintf(hbuf, sizeof hbuf, "%c%d %s(%d)", i == enc.cast_sel ? '>' : ' ', i + 1,
                        tr.tr(s->name_key).c_str(), s->power_cost);
          tl.add(sx, sy, hbuf, i == enc.cast_sel ? 15 : 7, LPX);
          sx += 96; if (sx > cbX1 - 80) { sx = cbX0 + 6; sy += 12; }   // 橫向排,滿一列換行
        }
      }
      add_lang_badge();
      return;
    }
    // 行動選單(1 行;全寬):基本(青 11)+ 特殊招式(黃 14)。戰鬥結束改顯示「繼續」提示。
    int mLineY = cbY0 + 3;
    if (!enc.over) {
      tl.add(cbX0 + 4, mLineY,
             tr.tr("F:Fight  R:Run") + "  " + tr.tr("C:Cast"), 11, LPX);
      if (enc.group)
        tl.add(cbX0 + 92, mLineY, tr.tr("M:Mighty D:Disarm A:Advance Q:Quick E:Dodge"), 14, LPX);
    }
    // 戰報旁白(最近 2 行;全寬、白字、可讀)── 移出右側窄欄,放這寬框。
    int logY = cbY0 + 16;
    int nlog = (int)enc.log.size();
    for (int i = (nlog > 2 ? nlog - 2 : 0); i < nlog; ++i) {
      tl.add(cbX0 + 4, logY, enc.log[(std::size_t)i], 15, LPX);
      logY += 12;
    }
    // 結果橫幅(over):色彩編碼結果 + 繼續提示(放選單列位置)。
    if (enc.over) {
      std::string tail;
      int tail_col = 12;
      if (enc.fled) tail = tr.tr("The party flees!");
      else if (enc.victory) { tail = tr.tr("Victory!"); tail_col = 10; }   // 亮綠
      else if (enc.defeat) { tail = tr.tr("The party has fallen."); tail_col = 4; }  // 暗紅
      else tail = enc.group ? tr.tr("Victory!")
                            : (!enc.mon.alive() ? (tr.tr(enc.mon_name_en) + " " + tr.tr("slain"))
                                                : (enc.hero.name + " down"));
      tl.add(cbX0 + 4, mLineY, tail, tail_col, LPX);
      std::string cont = tr.tr("[ continue ]");
      tl.add(cbX1 - tl.measure_vwidth(cont, LPX) - 6, mLineY, cont, 8, LPX);
    }
  };
  // area id / 關卡英文名 → 在地化關卡名(B2 在地化漏網修復)。
  //   優先用 WorldMap::place_name_zh(area)(CONTEXT.md 譯名;area 0 與 worldmap 共用同一表),
  //   其次走 i18n tr()(menu/events 等表的鍵),最後回退英文名(CONTEXT.md flagged 暫保留原文者)。
  auto area_name_tr = [&](int area, const std::string& en) -> std::string {
    std::string zh = render::WorldMap::place_name_zh(area);
    if (!zh.empty()) return zh;
    if (!en.empty()) { std::string t = tr.tr(en); if (!t.empty()) return t; }
    return en;
  };
  // area 0(Dilmun)美化世界圖共用繪製(B1):`?`/automap 與俯視探索(--map 0)同走此路徑,
  //   不再出現舊「單一水平帶」的原始 tile 格。其餘 39 關仍走 Minimap(oracle automap)。
  //   像素層由 WorldMap::render 畫(地形 + 圖示),繁中地點名 + 標題由文字層繪製(銳利)。
  auto draw_worldmap_view = [&]() {
    auto labels = worldmap.render(fb, *level, px, py);
    if (!para.active) {
      tl.add(8, 2, "Dilmun  迪瑪", 14, PX_UI);             // 標題:行星名(原文 + 繁中)
      for (auto& lb : labels) {
        int tw = lb.right_align ? tl.measure_vwidth(lb.name, PX_UI * 3 / 4) : 0;
        tl.add(lb.x - tw, lb.y, lb.name, 15, PX_UI * 3 / 4);  // 地點名(較小字,白)
      }
    }
    // 世界圖模式:語系指示已移到視窗標題;底部留提示列。
    tl.add(8, 190, tr.tr("Map  -  Esc: back  -  F4: lang"), 7, PX_UI);
  };
  auto draw_game = [&]() {
    // F:真實關卡俯視圖(從 .lvl 解出的 tile 格,像素層)+ 玩家朝向;文字走文字層。
    fb.clear(0);
    if (!level) return;
    // area 0(Dilmun overworld):俯視探索一律走美化世界圖(B1),非原始 tile 一條帶。
    if (current_area == 0) { draw_worldmap_view(); return; }
    int W = level->w, H = level->h;
    int cs = std::min(11, std::min(300 / (W ? W : 1), 150 / (H ? H : 1)));
    if (cs < 2) cs = 2;
    int ox = (render::kW - W * cs) / 2, oy = 14;
    for (int y = 0; y < H; ++y)
      for (int x = 0; x < W; ++x) {
        std::uint8_t c = tile_color(level->tile(x, y));
        for (int j = 0; j < cs - 1; ++j) for (int i = 0; i < cs - 1; ++i)
          fb.put(ox + x * cs + i, oy + y * cs + j, c);
      }
    font->draw_char(fb, ox + px * cs, oy + py * cs, (std::uint8_t)dirch[dir], 15, 0);  // 玩家(像素層)
    // 右側隊伍狀態面板(同 fp 模式;像素層狀態條 + 文字層角色名)。
    // 段落 overlay 近全螢幕 → 隱藏面板/關卡名,避免文字層名字穿透蓋在段落上。
    if (!para.active) {
      draw_explore_chrome();                              // UI chrome(藍外框 + logo;docs/gameplay/59 #1/#2)
      party.draw_status_panel(fb, tl, PX_UI);
      tl.add(8, 2, area_name_tr(current_area, level->name), 15, PX_UI);              // 文字層:關卡名(白字,對齊原版 docs/gameplay/59 #6)
    }
    add_lang_badge();
    // 控制提示移到底部訊息列(原版:viewport 下方白框)。訊息列獨立,不擠進原本的提示位置。
    // (docs/gameplay/59 #3:訊息框獨立,控制提示移到不擋訊息處。)
    if (!para.active && !msg.active && !sheet.active)    // 子畫面期間隱藏(避免穿透框)
      draw_msg_strip("I:fwd J/L:turn V:stats P:shop T:tavern S:save Esc", 7);
    // 事件/段落文字改走訊息檢視器(draw_msg_overlay,疊在最上層;見 render_now)。
  };
  // F+:第一人稱 viewport(透視牆面,像素層)。port 自 opendw refresh_viewport →
  //   update_viewport(靜態框架)→ ui_update_viewport。對拍 verify_fp 4/4(像素層不變)。
  auto draw_game_fp = [&]() {
    fb.clear(0);
    if (!level) return;
    // ── theme=Amiga:第一人稱地牢 = DOS golden 精確透視 + Amiga 風格青藍石牆配色 ──
    //   原生 Amiga viewport 圖塊(data3)已抽出並按 slot 對映,但「重組落點」需逆出 Amiga
    //   引擎 blit 錨點演算法(圖塊尺寸 ≠ DOS sprite → 直接套 DOS xpos/ypos 會破碎),屬無界
    //   RE,暫擱置(成果保留於 themes/amiga/components,見 viewport_amiga.hpp 註)。改採有界
    //   乾淨方案:沿用 byte-for-byte 對拍的 DOS 透視幾何,只把調色盤換成 kAmigaViewportPalette
    //   (石牆青藍 / 地板天花棕)→ 透視 100% 收斂、呈現 Amiga 地城氛圍。
    bool amiga_fp = !theme.component_dir.empty();
    if (amiga_fp) vid.set_palette(render::kAmigaViewportPalette);
    render::ViewportDecoder dec;
    // 牆面/地面/天空 sprite blit 進 viewport_memory(已對拍 golden 10880B)。
    render::render_first_person(*level, px, py, dir, dec, comps);
    if (vpt_ok)
      dec.compose_frame(vpt[0].data(), vpt[1].data(), vpt[2].data(), vpt[3].data());
    dec.to_framebuffer(fb);   // 160×136 @ (16,8)(像素層)
    // 右側隊伍狀態面板(port 自 opendw draw_player_status_panel):
    //   像素層 = 狀態條(HP/暈眩/法力);文字層 = 角色名(PX_UI 字級)。
    // 段落 overlay 近全螢幕 → 隱藏面板/關卡名,避免文字層名字穿透蓋在段落上。
    if (!para.active) {
      draw_explore_chrome();                              // UI chrome(藍外框 + logo;docs/gameplay/59 #1/#2)
      party.draw_status_panel(fb, tl, PX_UI);
      tl.add(8, 2, area_name_tr(current_area, level->name), 15, PX_UI);              // 文字層:關卡名(白字,對齊原版 docs/gameplay/59 #6)
    }
    add_lang_badge();
    // 控制提示移到底部訊息列(原版:viewport 下方白框);訊息列獨立。(docs/gameplay/59 #3)
    if (!para.active && !msg.active && !sheet.active)    // 子畫面期間隱藏(避免穿透框)
      draw_msg_strip("I:fwd J/L:turn V:stats P:shop T:tavern S:save Esc", 7);
    // 事件/段落文字改走訊息檢視器(draw_msg_overlay,疊在最上層;見 render_now)。
  };
  // 俯視平面地圖(`?` 鍵)。port 自 opendw process_minimap_commands:
  //   Minimap::render 組 minimap viewport_memory(已對拍 golden 36864B),
  //   再 blit 到 framebuffer 左上(對齊原版 draw_rectangle(1,0,39,192) 清空區)。
  auto draw_automap = [&]() {
    fb.clear(0);
    if (!level) return;
    // area 0(Dilmun)→ 美化世界地圖(旋轉 90° landscape + 地形美化 + 繁中地點標記)。
    //   像素層由 WorldMap::render 畫(地形 + 圖示),繁中地點名由文字層 tl 繪製(銳利)。
    //   其餘 39 關落到下方 oracle automap(Minimap),保真資產不動。
    if (current_area == 0) { draw_worldmap_view(); return; }  // B1:`?`/automap area 0 → 美化世界圖(共用)
    if (minimap_ok && minimap_dirty) {
      // --mm-seed 顯式給值(測試/展示)→ 用 Seed 模式;否則用遊戲內真實 fog of war。
      //   用 render_full(8 趟疊圖)鋪滿整個視窗(修稽核 #1:單趟 render 只畫最頂一帶)。
      if (mm_seed_set) {
        minimap.render_full(*level, px, py, comps, minimap_seed());
      } else {
        const std::vector<std::uint8_t>* bm = seen.bitmap(current_area);
        minimap.render_full_with_seen(*level, px, py, comps,
                                      bm ? bm->data() : nullptr, level->w, level->h);
      }
      minimap_dirty = false;
    }
    if (minimap_ok) minimap.to_framebuffer(fb, /*ox=*/1, /*oy=*/8, /*rows=*/0xC0);
    if (!para.active) tl.add(8, 2, area_name_tr(current_area, level->name), 14, PX_UI);   // 文字層:關卡名
    add_lang_badge();
    // ── 區域攻略提示(《軟體世界》1991):有提示則於下方壓暗面板自動顯示 ──
    auto hint_it = area_hints.find(current_area);
    if (!para.active && hint_it != area_hints.end()) {
      int hint_px = PX_BODY * 3 / 4;                 // ~18px CJK,塞更多字
      std::vector<std::string> lines = tl.wrap(hint_it->second, render::kW - 12, hint_px);
      int line_h = hint_px / eff_scale + 2;
      int hdr_h = PX_UI / eff_scale + 3;
      int box_top = render::kH - ((int)lines.size() * line_h + hdr_h + 8);
      if (box_top < 92) box_top = 92;                // 不超過上半(地圖上半仍可見)
      for (int y = box_top; y < render::kH; ++y)     // 棋盤壓暗底
        for (int x = 0; x < render::kW; ++x)
          if (((x + y) & 1) == 0) fb.put(x, y, 0);
      tl.add(6, box_top + 3, tr.tr("Guide hint (Softworld):"), 14, PX_UI * 3 / 4);
      int y = box_top + 3 + hdr_h;
      for (const std::string& ln : lines) {
        if (y + line_h > render::kH - 2) break;      // 超出畫面停(極長提示)
        tl.add(6, y, ln, 15, hint_px); y += line_h;
      }
    } else {
      tl.add(8, 188, tr.tr("Map  -  Esc: back"), 7, PX_UI);   // 無提示:原圖例
    }
  };
  // sprite/scene/viewport 靜態檢視:像素層已於前面建好;文字層每幀補上標籤。
  auto draw_static_text = [&]() {
    if (sprite_mode) tl.add(8, 4, sprite_name, 15, PX_UI);
  };
  // 各狀態的基礎畫面(不含頂層 Help / 離開確認 / theme toast)。
  auto draw_base = [&]() {
    if (state == S_TITLE) { draw_title(); return; }       // 開機 title splash(dragon art)
    if (state == S_COMBAT) { draw_encounter(); return; }  // 遭遇 / 戰鬥畫面(內部自套 sprite/theme 盤)
    // 探索 / 地圖 / 選單 / 建角:一律套當前 theme 預設盤。
    //   (避免上一幀 combat 套了 Amiga sprite 自帶盤後殘留到探索畫面;art 狀態各自於下方套盤。)
    vid.set_palette(theme.palette);
    if (state == S_ENDING) {                              // 結局序列
      if (ending_phase == 1 && para.active) {
        // phase 1:bundled 段落捲動(黑底 + 捲動 overlay,沿用既有 ParaViewer 呈現)。
        for (int y = 0; y < render::kH; ++y)
          for (int x = 0; x < render::kW; ++x) fb.put(x, y, 0);
        draw_para_overlay();
      } else {
        // phase 0 / 2:全螢幕過場場景 art + 在地化敘事 / The End 收尾。
        draw_ending_scene();
      }
      return;
    }
    if (state == S_MAP) { draw_automap(); return; }       // 俯視平面地圖(`?`)
    if (state == S_CREATE) { draw_chargen(); return; }    // 建角畫面(新遊戲 / 建立人物)
    if (state == S_GAME) {
      if (fp_mode) draw_game_fp(); else draw_game();
      if (msg.active) draw_msg_overlay();            // 一般事件訊息框疊在地圖/viewport 上層
      if (para.active) draw_para_overlay();          // Read Paragraph 長段落捲動 overlay(近全螢幕)
      if (sheet.active) {                            // 角色屬性表 / 物品欄疊在最上層
        if (sheet.show_inventory) draw_inventory();
        else draw_char_sheet();
      }
      if (shop_ui.active) draw_shop();               // 商店買賣疊在最上層
      if (tavern_ui.active) draw_tavern();           // 酒館招募疊在最上層
      if (cast_ui.active) draw_cast();               // 探索施法選單疊在最上層
      if (reorder_ui.active) draw_reorder();         // 重排隊伍疊在最上層
      return;
    }
    if (!menu_mode) { draw_static_text(); return; }  // sprite/scene/viewport:像素層靜態,只補文字
    if (state == S_MENU) {
      // 主選單按 1-4 開該角色屬性表;開啟時只畫底 + 角色表(不畫選單字,避免文字穿透)。
      if (sheet.active) {
        fb.clear(1);
        if (sheet.show_inventory) draw_inventory(); else draw_char_sheet();
      } else {
        draw_menu();
      }
    }
    else draw_branch();
  };
  auto render_now = [&]() {
    // 背景音樂:依當前 state 切曲(idempotent;缺檔 / headless 無裝置時 no-op)。
    //   標題/選單/建角 → Title;探索/地圖 → Game;戰鬥 → Combat;結局 → End。
    audio::MusicId mus = audio::MusicId::Game;
    switch (state) {
      case S_TITLE: case S_MENU: case S_BRANCH: case S_CREATE: mus = audio::MusicId::Title; break;
      case S_COMBAT: mus = audio::MusicId::Combat; break;
      case S_ENDING: mus = audio::MusicId::End; break;
      default: mus = audio::MusicId::Game; break;   // S_GAME / S_MAP
    }
    g_sound.play_music(mus);
    tl.clear();                                      // 每幀重建文字層
    draw_base();
    // 頂層全域覆蓋層(任何狀態之上):theme toast → Help → 離開確認(確認最上)。
    if (theme_toast > 0) draw_theme_toast();
    if (help_active) draw_help_overlay();
    if (confirm_quit_active) draw_confirm_quit();
    // ── VGA-256 主題:統一在 present/dump 前打開 256 色增強路徑 ──
    //   draw_base 內各狀態會呼叫 set_palette(會 reset vga256_),故在此最後一步依當前
    //   theme.vga256 重新設定;DOS/Amiga/X68000 → false(16 色路徑不變,golden 不破)。
    vid.set_vga256(theme.vga256);
  };
  // headless / 啟動即有訊息(--at 事件格 或 --read-para):進對應檢視器。
  //   段落事件(event_para_n>=0)→ 長段落捲動 overlay(--para-scroll N 先下捲 N 頁)。
  //   一般事件文字 → 下半部分頁訊息框(--msg-page N 先翻到第 N 頁)。
  if (state == S_GAME && event_para_n >= 0 && !event_msg.empty()) {
    open_para(event_para_n, event_msg);
    for (int p = 0; p < para_scroll && !para.at_bottom(); ++p) para.scroll_page(1);
    std::fprintf(stderr, "para viewer: N=%d, %d lines, %d/page → %d pages; top=%d page %d/%d\n",
                 para.para_n, para.total_lines(), para.visible_lines, para.page_count(),
                 para.top, para.cur_page(), para.page_count());
  } else if (state == S_GAME && !event_msg.empty()) {
    open_msg(event_msg);
    for (int p = 0; p < msg_page && msg.active; ++p) msg.advance();
    std::fprintf(stderr, "msg viewer: %d lines, %d/page → %d pages; showing page %d\n",
                 (int)msg.lines.size(), msg.lines_per_page, msg.page_count(), msg.page + 1);
  }
  // --char-sheet N:headless 直接開第 N 名(1-based)角色屬性表(驗證屬性值 / 在地化 / 版面)。
  if (state == S_GAME && char_sheet >= 1 && party.size() > 0) {
    sheet.open((int)party.size(), char_sheet - 1);
    sheet.show_inventory = show_inventory;   // --inventory:直接開物品欄
    if (alloc_open && !show_inventory) sheet.alloc_mode = true;  // --alloc:直接進 X 配點模式
    std::fprintf(stderr, "char sheet: showing character %d/%zu (\"%s\")%s%s\n",
                 sheet.idx + 1, party.size(), party.at((std::size_t)sheet.idx).name.c_str(),
                 show_inventory ? " [inventory]" : "", sheet.alloc_mode ? " [alloc]" : "");
  }
  // --gold N:設隊伍第 0 名 gold[81](商店 demo / 截圖 / headless 驗證)。
  if (state == S_GAME && grant_gold >= 0 && party.size() > 0) {
    game::set_gold(party.at(0), grant_gold);
    std::fprintf(stderr, "gold: set party[0] gold[81]=%d\n", game::get_gold(party.at(0)));
  }
  // --shop / --recruit:headless 直接開商店買賣 / 酒館招募子畫面(驗證版面 / 在地化)。
  if (state == S_GAME && shop_open && party.size() > 0) {
    shop_ui.open();
    std::fprintf(stderr, "shop: %zu stock items; party[0]='%s' gold=%d\n",
                 shop_data.size(), party.at(0).name.c_str(), game::get_gold(party.at(0)));
  }
  if (state == S_GAME && recruit_open && party.size() > 0) {
    tavern_ui.open();
    std::fprintf(stderr, "tavern: %zu recruitable NPCs; party=%zu/%d\n",
                 game::RecruitRoster::roster().size(), party.size(), game::kMaxPartyMembers);
  }
  // --encounter N:進遭遇畫面(headless 可 --dump 驗證圖層;互動下 F 戰鬥 / R 逃跑)。
  if (encounter_mode) {
    if (fight_namtar) begin_namtar(); else begin_encounter(encounter_id);
    // --cast <id>:headless 在遭遇中施放一次該法術(驗證效果 + 扣 Power + 戰報)。
    //   未習得時:--cast-force 可強制(供驗證效果套用);否則僅在 spellbook 內可施。
    if (cast_spell_id >= 0) {
      std::uint8_t sid = (std::uint8_t)cast_spell_id;
      const game::SpellDef* sp = game::find_spell(sid);
      bool known = !enc.spellbook.empty() &&
                   std::find(enc.spellbook.begin(), enc.spellbook.end(), sid) !=
                       enc.spellbook.end();
      if (sp && enc.hero_power < sp->power_cost) {   // 確保 Power 足夠(驗證用,給滿)
        enc.hero_power = sp->power_cost > 0 ? sp->power_cost + 10 : 20;
      }
      if (sp && (known || cast_force)) {
        int pw_before = enc.hero_power;
        if (enc.group) group_cast_round(sid); else cast_round(sid);
        std::fprintf(stderr,
                     "cast: id=0x%02X '%s' pw %d->%d mon_hp=%d hero_hp=%d over=%d "
                     "malive=%d mfled=%d\n",
                     sid, sp->name_key, pw_before, enc.hero_power, enc.mon.hp,
                     enc.hero.hp, enc.over,
                     enc.group_loop ? enc.group_loop->monsters_in_combat() : -1,
                     enc.group_loop ? enc.group_loop->monsters_fled() : -1);
      } else {
        std::fprintf(stderr, "cast: id=0x%02X not castable (known=%d sp=%p)\n",
                     sid, (int)known, (const void*)sp);
      }
    }
    // --combat-special <type>:headless 隊伍第 0 名用特殊攻擊一回合(驗證命中/傷害修正、
    //   卸武裝、DV 提升)。前後印怪/英雄狀態 + 旗標到 stderr 供斷言。
    if (!combat_special.empty() && enc.group && enc.group_loop) {
      game::SpecialAttack sa = parse_special(combat_special);
      const auto& mons0_before = enc.group_loop->monsters().at(0);
      int mon_hp_before = mons0_before.hp;
      bool mon_disarmed_before = mons0_before.disarmed;
      int hero_dodge_before = enc.group_loop->party().at(0).dodge_dv;
      game::AttackResult ar =
          enc.group_loop->special_attack(sa, /*actor_is_player=*/true, 0, -1);
      append_group_events();
      const auto& mons0 = enc.group_loop->monsters().at(0);
      const auto& hero0 = enc.group_loop->party().at(0);
      std::fprintf(stderr,
                   "combat-special: type=%s hit=%d dmg=%d "
                   "mon_hp %d->%d mon_disarmed %d->%d hero_dodge_dv %d->%d\n",
                   combat_special.c_str(), (int)ar.hit, ar.damage, mon_hp_before,
                   mons0.hp, (int)mon_disarmed_before, (int)mons0.disarmed,
                   hero_dodge_before, hero0.dodge_dv);
      for (const auto& line : enc.log) std::fprintf(stderr, "  %s\n", line.c_str());
    }
    // --combat-rounds N:自動打 N 回合(headless 驗證戰報 / 確定性)。群戰走 group_round。
    for (int r = 0; r < combat_rounds && !enc.over; ++r) {
      if (enc.group && cast_spell_id < 0) group_round(); else combat_round();
    }
    // 跑完回合清受擊閃白(hit_flash 是互動時一閃即過的回饋;headless 靜態 dump 要顯示正常
    //   立繪 + 戰報,不要凍在閃白幀,否則怪物看似消失)。
    if (combat_rounds > 0) enc.hit_flash = 0;
    if (combat_rounds > 0) {
      if (enc.group && enc.group_loop) {
        std::fprintf(stderr,
                     "combat(group): %d rounds, party_alive=%d mon_alive=%d "
                     "outcome=%d xp=%d over=%d\n",
                     combat_rounds, enc.group_loop->party_alive(),
                     enc.group_loop->monsters_alive(),
                     (int)enc.group_loop->outcome(), enc.group_loop->xp_award(),
                     enc.over);
        // 逐事件戰報印到 stderr(確定性驗證 / 截圖佐證)。
        for (const auto& line : enc.log) std::fprintf(stderr, "  %s\n", line.c_str());
      } else {
        std::fprintf(stderr, "combat: %d rounds, hero_hp=%d mon_hp=%d over=%d\n",
                     combat_rounds, enc.hero.hp, enc.mon.hp, enc.over);
      }
    }
    // 終戰 Namtar 打贏(headless,--combat-rounds 跑出 Victory)→ 進結局序列。
    if (enc.is_namtar && enc.victory) {
      enc.active = false;
      enter_ending();
    }
  }
  // --ending:不打 Namtar,直接進結局序列(demo / 截圖)。
  if (ending_mode) {
    enter_ending();
    // --ending-idx N:截圖驗證用,直接跳到第 N 張過場場景(0-based;超界夾到末張)。
    if (ending_at > 0) {
      int last = (int)render::theme_ending_scenes(theme).size() - 1;
      ending_idx = ending_at > last ? last : ending_at;
      ending_phase = (ending_idx == last) ? 2 : 0;
      load_ending_scene(ending_idx);
      std::fprintf(stderr, "ENDING jump → idx=%d phase=%d\n", ending_idx, ending_phase);
    }
  }
  // 結局序列 headless:--para-scroll N 先下捲 N 頁(截圖後段內容用)。
  if (state == S_ENDING && para.active)
    for (int p = 0; p < para_scroll && !para.at_bottom(); ++p) para.scroll_page(1);
  render_now();

  if (!dump.empty()) {
    if (vid.dump_ppm(fb, dump))
      std::fprintf(stderr, "dumped composed frame (%dx%d) to %s\n", vid.out_w(), vid.out_h(), dump.c_str());
    else
      std::fprintf(stderr, "dump failed: %s\n", dump.c_str());
  }

  int frames = 0;
  // ── --keys 合成輸入序列(headless 逐幀注入)──
  //   解析成 token 清單;每幀消耗一個 token 轉成 render::Input,覆蓋 poll() 結果。
  std::vector<std::string> key_tokens;
  if (!keys_seq.empty()) {
    std::string cur;
    for (char c : keys_seq) {
      if (c == ',') { if (!cur.empty()) key_tokens.push_back(cur); cur.clear(); }
      else cur.push_back(c);
    }
    if (!cur.empty()) key_tokens.push_back(cur);
    std::fprintf(stderr, "keys: %zu synthetic token(s) queued\n", key_tokens.size());
  }
  std::size_t key_idx = 0;
  // token → Input(headless 注入)。回傳 true 表示有注入(本幀用合成輸入)。
  auto synth_input = [&](render::Input& in) -> bool {
    if (key_idx >= key_tokens.size()) return false;
    std::string t = key_tokens[key_idx++];
    for (auto& ch : t) ch = (char)std::toupper((unsigned char)ch);
    in = render::Input{};   // 清空,只帶本 token
    if (t == "F1") in.help = true;
    else if (t == "F4") in.cycle_lang = true;
    else if (t == "F8") in.cycle_theme = true;
    else if (t == "F10") in.request_quit = true;
    else if (t == "ESC") in.back = true;
    else if (t == "ENTER" || t == "RETURN") in.select = true;
    else if (t == "SPACE") in.select = true;
    else if (t == "UP") in.up = true;
    else if (t == "DOWN") in.down = true;
    else if (t == "LEFT") in.left = true;
    else if (t == "RIGHT") in.right = true;
    else if (t == "PGUP") in.pgup = true;
    else if (t == "PGDN") in.pgdown = true;
    else if (t.size() == 1 && std::isalpha((unsigned char)t[0])) in.key = t[0];
    else if (t.size() == 1 && std::isdigit((unsigned char)t[0])) in.key = t[0];   // 數字鍵(選單選角色 / 角色表切換)
    else std::fprintf(stderr, "keys: unknown token '%s' (skipped)\n", t.c_str());
    std::fprintf(stderr, "keys: inject [%zu/%zu] %s\n", key_idx, key_tokens.size(), t.c_str());
    return true;
  };
  // --terrain-cast:headless 一次性套用(在 S_GAME、首幀對前方/當前格施地形法術後印結果)。
  bool terrain_cast_done = false;
  for (;;) {
    if (terrain_cast_id >= 0 && !terrain_cast_done && state == S_GAME) {
      terrain_cast_done = true;
      const char* m = resolve_explore_cast((std::uint8_t)terrain_cast_id);
      std::fprintf(stderr, "terrain-cast 0x%02X @(%d,%d) dir=%d -> %s\n",
                   terrain_cast_id, px, py, dir, m);
    }
    // --trap-probe:把隊伍移到本關第一個真實陷阱格,觸發 trigger_trap_here(整合驗證:
    //   真陷阱座標 → 踩格 → 扣血)。印 PASS/FAIL 後退出。位置=原版真值;傷害=remake 設計。
    if (trap_probe && state == S_GAME) {
      trap_probe = false;
      if (real_traps.count() == 0) { std::fprintf(stderr, "trap-probe: FAIL no real traps in area %d\n", current_area); break; }
      auto cell = *real_traps.cells().begin();
      px = cell.first; py = cell.second;
      std::uint16_t hp0 = party.size() ? party.at(0).health : 0;
      bool sprung = trigger_trap_here();
      std::uint16_t hp1 = party.size() ? party.at(0).health : 0;
      std::fprintf(stderr, "trap-probe area %d @(%d,%d): sprung=%d hp %u->%u (%s)\n",
                   current_area, px, py, sprung ? 1 : 0, hp0, hp1,
                   (sprung && hp1 <= hp0) ? "PASS" : "FAIL");
      break;
    }
    render_now();
    vid.present(fb);
    // 互動模式 frame cap ~30fps(老遊戲節奏;軟體 renderer 無 vsync,無此 delay 會 busy-loop
    //   吃滿 CPU → 卡頓 / 輸入延遲)。33ms ≈ 30fps,符合 1990 老遊戲手感且省 CPU。headless 不延遲。
    if (!headless) vid.delay(33);
    // 動畫相位推進(在 present 後、各輸入分支 continue 前 → 戰鬥怪物每幀皆呼吸/閃白倒數)。
    //   與 frames 同步遞增 → headless --dump-frame N 的動畫相位確定可重現。
    ++anim_tick;
    if (enc.hit_flash > 0) --enc.hit_flash;
    // --dump-frame N:迴圈第 N 幀(此時已套用前面幀的輸入,如 F1/F10 覆蓋層)再 dump 一次。
    if (dump_frame >= 0 && frames == dump_frame && !dump.empty()) {
      if (vid.dump_ppm(fb, dump))
        std::fprintf(stderr, "dumped loop frame %d to %s\n", frames, dump.c_str());
    }
    render::Input in = vid.poll();
    synth_input(in);   // headless --keys:有排隊 token 則覆蓋本幀輸入(否則保留 poll 結果)
    // grounded quest 物品給予:換區後延一輪、待事件框/子畫面關閉時發放(避免覆蓋進區事件文字)。
    if (state == S_GAME && quest_grant_pending >= 0 && !msg.active && !para.active && !sheet.active) {
      try_grant_area(quest_grant_pending, -1);
      quest_grant_pending = -1;
    }
    // 建角命名階段:'q' 是合法名字字元(如 "Quinn"),不應觸發離開確認。
    //   poll 把 Q 同時設 request_quit 與 text_char='q'/'Q' → 命名時改當文字輸入,吃掉 request_quit。
    if (in.request_quit && state == S_CREATE && cg.active && cg.phase == CharGenUi::PhName &&
        in.text_char) {
      in.request_quit = false;
    }
    // in.quit 現在只剩「關窗(SDL_QUIT)」會設(Q 改走 request_quit 確認流程,見下方)。
    if (in.quit) {
      // 關窗:遊戲中且有隊伍 → 先自動存檔再退(進度不失)。
      if (state == S_GAME && party.size() > 0) {
        bool saved = do_save();
        std::fprintf(stderr, "quit(window): autosave=%d\n", (int)saved);
      }
      break;
    }
    if (theme_toast > 0) --theme_toast;   // F8 主題提示倒數(每幀)

    // ── 離開確認視窗(最高優先;開啟時接管全部輸入)──
    //   已於開啟時自動存檔。Y / Enter → 離開遊戲;N / Esc → 取消回遊戲。
    if (confirm_quit_active) {
      if (in.key == 'Y' || in.select) {
        std::fprintf(stderr, "confirm-quit: Y → quit\n");
        break;
      }
      if (in.key == 'N' || in.back) {
        confirm_quit_active = false;
        std::fprintf(stderr, "confirm-quit: N → cancel (back to game)\n");
      }
      if (max_frames >= 0 && ++frames >= max_frames) break;
      continue;                                          // 確認期間不處理其他輸入
    }

    // ── F1 Help 覆蓋層(開啟時接管輸入;Esc / F1 關閉)──
    if (help_active) {
      if (in.back || in.help) { help_active = false; std::fprintf(stderr, "help: close\n"); }
      if (max_frames >= 0 && ++frames >= max_frames) break;
      continue;                                          // Help 期間不處理其他輸入
    }
    if (in.help) {                                       // F1:開 Help
      help_active = true;
      std::fprintf(stderr, "help: open (F1)\n");
      if (max_frames >= 0 && ++frames >= max_frames) break;
      continue;
    }

    // ── F8:循環切換 UI 主題(state 記住索引;畫面下幀重繪即時套用;短暫 toast 提示)──
    if (in.cycle_theme) {
      theme_idx = (theme_idx + 1) % render::theme_count();
      theme = render::theme_by_index(theme_idx);
      load_title_art();   // reload 對應主題的 title art + 套該主題 palette(per-theme palette 切換)
      // 戰鬥中切 theme:重載當前怪物 sprite(改用新 theme 的 sprite 來源 + 自帶盤)。
      //   F8 → Amiga 時戰鬥怪物圖即時換成 Amiga 美術(否則沿用進場時載的 DOS sprite)。
      if (state == S_COMBAT && enc.active)
        enc.sprite = sprite_for_monster(enc.mon_name_en, enc.sprite_res);
      theme_toast = 90;                                  // 顯示約 90 幀(toast)
      update_window_title();                              // tileset 變 → 更新視窗標題
      std::fprintf(stderr, "theme: cycle → [%d] %s (count=%d, partial=%d)\n",
                   theme_idx, theme.name.c_str(), render::theme_count(), (int)theme.partial);
      if (max_frames >= 0 && ++frames >= max_frames) break;
      continue;
    }

    // ── 離開請求:F10(任何狀態) 或 頂層 ESC(主選單 / S_GAME 探索無子畫面)──
    //   觸發:自動存檔(有隊伍時)→ 開離開確認視窗(Y/N)。避免不小心按 ESC 直接掉出遊戲。
    {
      // S_GAME 是否有子畫面正在用 ESC(訊息/段落/角色表/商店/酒館/施法/重排)→ 該 ESC 歸子畫面。
      bool game_sub_overlay = state == S_GAME &&
          (msg.active || para.active || sheet.active || shop_ui.active ||
           tavern_ui.active || cast_ui.active || reorder_ui.active);
      //   S_TITLE 的 ESC 維持「進主選單」(由下方 S_TITLE handler 處理),不觸發離開確認。
      //   !menu_mode(sprite/scene/viewport/--map 直開 等 headless viewer)維持 ESC 直接離開。
      bool top_level_esc = menu_mode && in.back && !game_sub_overlay &&
          (state == S_MENU || (state == S_GAME && !game_sub_overlay));
      if (in.request_quit || top_level_esc) {
        bool saved = false;
        if (party.size() > 0) saved = do_save();          // 自動存檔(既有 do_save;無隊伍則略過)
        confirm_quit_active = true;
        std::fprintf(stderr, "request-quit (%s): autosaved=%d → confirm window\n",
                     in.request_quit ? "F10" : "top-ESC", (int)saved);
        if (max_frames >= 0 && ++frames >= max_frames) break;
        continue;
      }
    }

    // F4:即時循環切換語系 → 重載字串/段落書 → 重譯所有 widget。
    // 因每幀重繪(render_now),畫面立即變為新語言;事件文字重跑該關腳本重譯。
    if (in.cycle_lang && !locales.empty()) {
      locale_idx = (locale_idx + 1) % (int)locales.size();
      load_locale(locales[locale_idx]);
      relocalize();                                      // 選單/branch 重譯
      update_window_title();                             // 語言變 → 更新視窗標題
      if (para.active) {
        // 段落 overlay:用新語系全文(含 zh-TW 回退)就地重排,維持捲動位置;標題即時換 i18n。
        std::string full = para_text(para.para_n);
        if (!full.empty()) para.reflow(tl.wrap(full, PB_TEXT_W, PX_BODY));
      } else if (state == S_GAME && level && last_event_tile > 1) {
        event_msg = run_event((std::uint8_t)last_event_tile);  // 事件文字換語言
        if (msg.active) msg.reflow(tl.wrap(event_msg, MB_TEXT_W, PX_BODY));  // 當前頁就地重排
      }
      continue;                                          // 本幀不再處理其他輸入
    }
    // ── 建角畫面(S_CREATE):接管全部輸入。F4(語系)已於上方處理。──
    //   PhName:文字輸入名字;Enter→PhAttr;Esc→取消回選單。
    //   PhAttr:↑↓ 選屬性;+/− 或 ←→ 調整;G 切性別;Enter 完成本員;
    //           N 新增下一名;B 開始遊戲(至少 1 名);Esc 回命名。
    if (state == S_CREATE && cg.active) {
      if (cg.phase == CharGenUi::PhName) {
        if (in.back) {                                   // Esc:取消建角 → 回選單
          cg.close();
          if (menu_mode) state = S_MENU; else break;
        } else if (in.backspace) {
          if (!cg.draft.name.empty()) cg.draft.name.pop_back();
        } else if (in.text_char && (int)cg.draft.name.size() < game::chargen::kNameMaxLen) {
          cg.draft.name.push_back((char)in.text_char);
        } else if (in.select) {                          // Enter:名字確認 → 配點
          if (cg.draft.name_valid()) cg.phase = CharGenUi::PhAttr;
        }
      } else {  // PhAttr
        if (in.back) { cg.phase = CharGenUi::PhName; }    // Esc:回命名
        else if (in.up) cg.cursor = (cg.cursor + 3) % 4;
        else if (in.down) cg.cursor = (cg.cursor + 1) % 4;
        else if (in.right || in.key == '=')              // → 或 +/= 增點
          cg.draft.inc(cg.cursor);
        else if (in.left || in.key == '-')               // ← 或 -/_ 退點
          cg.draft.dec(cg.cursor);
        else if (in.key == 'G') cg.draft.gender ^= 1;    // 切性別(男/女)
        else if (in.key == 'N' || in.select) {           // 完成本員 → 新增下一名(或滿員提示)
          if (cg.commit_current()) {
            std::fprintf(stderr, "chargen: committed '%s' (party now %zu)\n",
                         cg.draft.name.c_str(), cg.done_records.size());
            if (!cg.begin_next())                        // 滿 4 名 → 直接開始遊戲
              finish_chargen();
          }
        } else if (in.key == 'B') {                      // B:用目前已建隊員開始遊戲
          if (!cg.done_records.empty() || cg.draft.name_valid()) {
            if (cg.draft.name_valid()) cg.commit_current();  // 把當前未提交的也納入
            finish_chargen();
          }
        }
      }
      if (max_frames >= 0 && ++frames >= max_frames) break;
      continue;                                          // 建角期間不處理其他輸入
    }
    // 商店買賣啟用時:接管輸入(Tab 切買/賣、↑↓ 選、Enter 買/賣、Esc 離開)。暫停移動。
    if (shop_ui.active) {
      game::CharacterRecord& buyer = party.at(0);  // 付款/收款方 = 主角(隊伍第 0 名)
      // 當前頁的項目數。
      int n = 0;
      if (!shop_ui.sell_mode) n = (int)shop_data.size();
      else for (int s = 0; s < game::CharacterRecord::kInventorySlots; ++s)
             if (buyer.item_at(s).present) ++n;
      if (in.back) { shop_ui.close(); }
      else if (in.key == '\t' || in.key == 'B' || in.key == 'A') shop_ui.toggle_mode();  // Tab/B/A 切買賣
      else if (in.up)   { if (n > 0) shop_ui.cursor = (shop_ui.cursor - 1 + n) % n; shop_ui.flash.clear(); }
      else if (in.down) { if (n > 0) shop_ui.cursor = (shop_ui.cursor + 1) % n; shop_ui.flash.clear(); }
      else if (in.select && n > 0 && shop_ui.cursor < n) {
        if (!shop_ui.sell_mode) {                          // 買
          game::ShopResult r = shop_data.buy(buyer, (std::size_t)shop_ui.cursor);
          shop_ui.flash = tr.tr(r.ok ? "Bought." : r.reason);
        } else {                                           // 賣:游標序 → 實際 slot
          int target = -1, shown = 0;
          for (int s = 0; s < game::CharacterRecord::kInventorySlots; ++s) {
            if (!buyer.item_at(s).present) continue;
            if (shown == shop_ui.cursor) { target = s; break; }
            ++shown;
          }
          if (target >= 0) {
            game::ShopResult r = game::Shop::sell(buyer, target);
            shop_ui.flash = tr.tr(r.ok ? "Sold." : r.reason);
            int nn = n - (r.ok ? 1 : 0);
            if (nn > 0) shop_ui.cursor %= nn; else shop_ui.cursor = 0;
          } else shop_ui.flash = tr.tr("Nothing to sell.");
        }
      }
      if (max_frames >= 0 && ++frames >= max_frames) break;
      continue;
    }
    // 探索施法啟用時:接管輸入(↑↓ 選法術、Enter 施放、Esc 離開)。暫停移動。
    if (cast_ui.active) {
      int n = (int)cast_ui.spellbook.size();
      if (in.back) { cast_ui.close(); }
      else if (in.up)   { if (n > 0) cast_ui.cursor = (cast_ui.cursor - 1 + n) % n; cast_ui.flash.clear(); }
      else if (in.down) { if (n > 0) cast_ui.cursor = (cast_ui.cursor + 1) % n; cast_ui.flash.clear(); }
      else if (in.select && n > 0 && cast_ui.cursor < n) {
        std::uint8_t sid = cast_ui.spellbook[(std::size_t)cast_ui.cursor];
        const char* msg = resolve_explore_cast(sid);
        cast_ui.flash = tr.tr(msg);
        // 施放後可施法清單可能因 Power 改變,重算(游標夾回界內)。
        cast_ui.spellbook = game::castable_spells(party.at(0), (int)party.at(0).power);
        int nn = (int)cast_ui.spellbook.size();
        if (nn > 0) cast_ui.cursor %= nn; else cast_ui.cursor = 0;
      }
      if (max_frames >= 0 && ++frames >= max_frames) break;
      continue;
    }
    // 重排隊伍啟用時:接管輸入。暫停移動。
    //   未抓起:↑↓ 移游標,Enter/Space 抓起當前成員。
    //   已抓起:↑↓ 把抓起成員與相鄰成員對調(= Party::move),Enter/Space 放下,Esc 取消放下。
    if (reorder_ui.active) {
      int n = (int)party.size();
      if (in.back) { reorder_ui.close(); }
      else if (reorder_ui.grabbed < 0) {
        if (in.up)   { if (n > 0) reorder_ui.cursor = (reorder_ui.cursor - 1 + n) % n; reorder_ui.flash.clear(); }
        else if (in.down) { if (n > 0) reorder_ui.cursor = (reorder_ui.cursor + 1) % n; reorder_ui.flash.clear(); }
        else if (in.select && n > 1) { reorder_ui.grabbed = reorder_ui.cursor; reorder_ui.flash.clear(); }
      } else {
        if (in.up && reorder_ui.cursor > 0) {
          party.move((std::size_t)reorder_ui.grabbed, (std::size_t)(reorder_ui.cursor - 1));
          reorder_ui.cursor--; reorder_ui.grabbed = reorder_ui.cursor;
        } else if (in.down && reorder_ui.cursor < n - 1) {
          party.move((std::size_t)reorder_ui.grabbed, (std::size_t)(reorder_ui.cursor + 1));
          reorder_ui.cursor++; reorder_ui.grabbed = reorder_ui.cursor;
        } else if (in.select) {                 // 放下
          reorder_ui.grabbed = -1;
          reorder_ui.flash = tr.tr("Party reordered.");
          std::fprintf(stderr, "reorder: party order updated\n");
        }
      }
      if (max_frames >= 0 && ++frames >= max_frames) break;
      continue;
    }
    // 酒館招募啟用時:接管輸入(↑↓ 選、Enter 招募、Esc 離開)。暫停移動。
    if (tavern_ui.active) {
      int n = (int)game::RecruitRoster::roster().size();
      if (in.back) { tavern_ui.close(); }
      else if (in.up)   { if (n > 0) tavern_ui.cursor = (tavern_ui.cursor - 1 + n) % n; tavern_ui.flash.clear(); }
      else if (in.down) { if (n > 0) tavern_ui.cursor = (tavern_ui.cursor + 1) % n; tavern_ui.flash.clear(); }
      else if (in.select && n > 0 && tavern_ui.cursor < n) {
        std::uint8_t id = game::RecruitRoster::roster()[(std::size_t)tavern_ui.cursor].identifier;
        game::RecruitResult r = game::recruit_npc(party, id);
        tavern_ui.flash = tr.tr(r.ok ? "Recruited!" : r.reason);
        if (r.ok) std::fprintf(stderr, "recruit: id=0x%02X joined; party now %d\n", id, r.party_size_after);
      }
      if (max_frames >= 0 && ++frames >= max_frames) break;
      continue;
    }
    // 角色屬性表啟用時:接管輸入(切角色/關閉),暫停移動。
    //   ↑↓ 或數字 1-4 切角色;Esc 關閉。F4(語系)已於上方處理。
    if (sheet.active) {
      game::CharacterRecord& cs = party.at((std::size_t)sheet.idx);
      // ── 屬性表 X 配點模式:↑↓ 選項目、+ 加 1 點、X/Esc 離開 ──
      if (!sheet.show_inventory && sheet.alloc_mode) {
        if (in.back || in.key == 'X') { sheet.alloc_mode = false; sheet.flash.clear(); }
        else if (in.up)   sheet.alloc_cursor = (sheet.alloc_cursor - 1 + CharSheet::kAllocCount) % CharSheet::kAllocCount;
        else if (in.down) sheet.alloc_cursor = (sheet.alloc_cursor + 1) % CharSheet::kAllocCount;
        else if (in.right || in.key == '=' || in.select) {   // 加 1 點(右/+/Enter)
          AllocTarget at = alloc_target_at(sheet.alloc_cursor);
          bool ok = at.is_attr ? game::spend_ap_on_attr(cs, at.target)
                               : game::spend_ap_on_skill(cs, at.target);
          sheet.flash = tr.tr(ok ? "Point spent." : "No AP / at cap.");
        }
        if (max_frames >= 0 && ++frames >= max_frames) break;
        continue;
      }
      // ── 物品轉移子模式:選目標隊員(↑↓ 選、Enter 確認、Esc/T 取消)──
      //   目標清單 = 隊伍除自己外的所有成員(0-based,跳過 sheet.idx)。
      if (sheet.show_inventory && sheet.transfer_mode) {
        std::vector<int> targets;
        for (int t = 0; t < (int)party.size(); ++t) if (t != sheet.idx) targets.push_back(t);
        int tn = (int)targets.size();
        if (in.back || in.key == 'T') { sheet.transfer_mode = false; sheet.transfer_slot = -1; sheet.flash.clear(); }
        else if (in.up)   { if (tn > 0) sheet.target_cursor = (sheet.target_cursor - 1 + tn) % tn; }
        else if (in.down) { if (tn > 0) sheet.target_cursor = (sheet.target_cursor + 1) % tn; }
        else if (in.select && tn > 0 && sheet.target_cursor < tn) {
          int to = targets[sheet.target_cursor];
          bool ok = party.transfer_item((std::size_t)sheet.idx, sheet.transfer_slot, (std::size_t)to);
          sheet.flash = tr.tr(ok ? "Item transferred." : "Target pack is full.");
          sheet.transfer_mode = false; sheet.transfer_slot = -1;
          // 來源物品數可能變動 → 夾住游標。
          int left = 0;
          for (int s = 0; s < game::CharacterRecord::kInventorySlots; ++s)
            if (cs.item_at(s).present) ++left;
          if (left > 0) sheet.inv_cursor %= left; else sheet.inv_cursor = 0;
        }
        if (max_frames >= 0 && ++frames >= max_frames) break;
        continue;
      }
      // ── 物品欄:↑↓ 移游標、U 使用、Enter 裝備穿脫、D 丟棄、T 轉移、E 回屬性表、Esc 關閉 ──
      if (sheet.show_inventory) {
        // 統計 present 物品的真實 slot,供游標 → slot 映射。
        std::vector<int> pslots;
        for (int s = 0; s < game::CharacterRecord::kInventorySlots; ++s)
          if (cs.item_at(s).present) pslots.push_back(s);
        int n = (int)pslots.size();
        if (in.back) { sheet.close(); }
        else if (in.key == 'E') sheet.toggle_view();
        else if (in.key == 'V') sheet.close();
        else if (in.up)   { if (n > 0) sheet.inv_cursor = (sheet.inv_cursor - 1 + n) % n; sheet.flash.clear(); }
        else if (in.down) { if (n > 0) sheet.inv_cursor = (sheet.inv_cursor + 1) % n; sheet.flash.clear(); }
        else if (in.key >= '1' && in.key <= '9') sheet.select(in.key - '0');
        else if (in.key == 'U' && n > 0 && sheet.inv_cursor < n) {   // 使用物品
          int slot = pslots[sheet.inv_cursor];
          game::UseItemResult u = game::use_item(cs, slot);
          if (!u.ok) sheet.flash = tr.tr("Nothing happens.");
          else if (u.restored_power) sheet.flash = tr.tr("Power restored.");
          else if (u.taught_spell)   sheet.flash = tr.tr("Spell learned.");
          else if (u.casts_spell)    sheet.flash = tr.tr("Item cast a spell.");
          if (n > 0) { int nn = u.consumed ? n - 1 : n; if (nn > 0) sheet.inv_cursor %= nn; else sheet.inv_cursor = 0; }
        }
        else if (in.key == 'D' && n > 0 && sheet.inv_cursor < n) {    // D:丟棄(從背包移除)
          int slot = pslots[sheet.inv_cursor];
          bool ok = party.discard_item((std::size_t)sheet.idx, slot);
          sheet.flash = tr.tr(ok ? "Item discarded." : "Nothing happens.");
          int left = n - (ok ? 1 : 0);
          if (left > 0) sheet.inv_cursor %= left; else sheet.inv_cursor = 0;
        }
        else if (in.key == 'T' && n > 0 && sheet.inv_cursor < n) {    // T:轉移給其他隊員
          if (party.size() < 2) { sheet.flash = tr.tr("No one to transfer to."); }
          else {
            sheet.transfer_mode = true;
            sheet.transfer_slot = pslots[sheet.inv_cursor];
            sheet.target_cursor = 0;
            sheet.flash.clear();
          }
        }
        else if (in.select && n > 0 && sheet.inv_cursor < n) {       // Enter:裝備穿脫
          int slot = pslots[sheet.inv_cursor];
          game::EquipResult e = game::toggle_equip(cs, slot);
          if (e.ok) sheet.flash = tr.tr(e.now_equipped ? "Equipped." : "Unequipped.");
        }
        if (max_frames >= 0 && ++frames >= max_frames) break;
        continue;
      }
      // ── 改名輸入子模式(手冊 R):TTF 文字輸入;Enter 確認 / Esc 取消 ──
      if (!sheet.show_inventory && !sheet.alloc_mode && sheet.rename_mode) {
        if (in.back) { sheet.rename_mode = false; sheet.rename_buf.clear(); sheet.flash.clear(); }
        else if (in.backspace) { if (!sheet.rename_buf.empty()) sheet.rename_buf.pop_back(); }
        else if (in.text_char && (int)sheet.rename_buf.size() < 12)
          sheet.rename_buf.push_back((char)in.text_char);
        else if (in.select) {                            // Enter:確認改名
          bool ok = party.rename((std::size_t)sheet.idx, sheet.rename_buf);
          sheet.flash = tr.tr(ok ? "Name changed." : "Name unchanged.");
          sheet.rename_mode = false; sheet.rename_buf.clear();
        }
        if (max_frames >= 0 && ++frames >= max_frames) break;
        continue;
      }
      // ── 刪除人物確認子模式(手冊 D):Y 確認 / N/Esc 取消 ──
      if (!sheet.show_inventory && !sheet.alloc_mode && sheet.delete_confirm) {
        if (in.key == 'Y') {                             // 確認刪除
          int removed = sheet.idx;
          party.remove((std::size_t)removed);
          sheet.delete_confirm = false;
          if (party.size() == 0) { sheet.close(); }      // 隊伍空 → 關閉
          else { sheet.count = (int)party.size();
                 if (sheet.idx >= sheet.count) sheet.idx = sheet.count - 1;
                 sheet.flash = tr.tr("Character deleted."); }
        } else if (in.key == 'N' || in.back) {           // 取消
          sheet.delete_confirm = false; sheet.flash.clear();
        }
        if (max_frames >= 0 && ++frames >= max_frames) break;
        continue;
      }
      // ── 屬性表(預設):切角色 / E 物品欄 / X 進配點 / D 刪除 / R 改名 / Esc 關閉 ──
      if (in.back) { sheet.close(); }                    // Esc:關閉回遊戲
      else if (in.up) sheet.prev();
      else if (in.down) sheet.next();
      else if (in.key >= '1' && in.key <= '9') sheet.select(in.key - '0');
      else if (in.key == 'E') sheet.toggle_view();       // E:屬性表 ⇄ 物品欄
      else if (in.key == 'X') { sheet.alloc_mode = true; sheet.alloc_cursor = 0; sheet.flash.clear(); }  // X:進配點
      else if (in.key == 'D') { sheet.delete_confirm = true; sheet.flash.clear(); }  // D:刪除人物(進確認)
      else if (in.key == 'R') { sheet.rename_mode = true; sheet.rename_buf.clear(); sheet.flash.clear(); }  // R:改名(進輸入)
      else if (in.key == 'V') sheet.close();             // V 再按一次 → 關閉
      if (max_frames >= 0 && ++frames >= max_frames) break;
      continue;                                          // 屬性表期間不處理移動
    }
    // Read Paragraph 長段落捲動 overlay 啟用時:接管輸入,暫停移動。
    //   ↑↓:逐行捲動;PgUp/PgDn / Space / Enter / I / K:逐頁;Esc:關閉回遊戲。
    //   (放在 msg 之前;兩者互斥,paragraph 觸發時 msg 不會 active。)
    // 結局過場場景(phase 0 / 2):非段落捲動狀態。按鍵推進下一張;末張收尾。
    if (state == S_ENDING && !para.active) {
      bool advance = in.select || in.pgdown || in.key == 'I' || in.key == 'K' ||
                     in.down || in.back;
      if (advance) {
        const int last = (int)render::theme_ending_scenes(theme).size() - 1;
        if (ending_phase == 2 || ending_idx >= last) {
          // 末張 The End:結束結局序列。
          std::fprintf(stderr, "ENDING done (phase=%d, idx=%d)\n", ending_phase, ending_idx);
          if (menu_mode) { state = S_MENU; }   // 互動:回標題選單(可再開新局)
          else break;                          // headless --ending / --fight-namtar:結束
        } else if (ending_idx == last - 1) {
          // 倒數第二張看完 → phase 1:開 bundled 段落捲動文件。
          ending_phase = 1;
          open_ending_doc();
          std::fprintf(stderr, "ENDING → phase 1 (paragraph scroll)\n");
        } else {
          // phase 0:推進下一張過場場景。
          ++ending_idx;
          load_ending_scene(ending_idx);
        }
      }
      if (max_frames >= 0 && ++frames >= max_frames) break;
      continue;
    }
    if (para.active) {
      // 結局序列段落捲動(phase 1):捲到底再按 Enter/Space → 進 phase 2(末張 The End)。
      //   非到底時 Enter 仍逐頁下捲,Esc 提前跳到末張。
      if (state == S_ENDING) {
        bool to_last = in.back ||
                       ((in.select || in.pgdown || in.key == 'I' || in.key == 'K') &&
                        para.at_bottom());
        if (to_last) {
          para.close();
          ending_phase = 2;
          ending_idx = (int)render::theme_ending_scenes(theme).size() - 1;
          load_ending_scene(ending_idx);       // 末張 The End / Amiga 結局
          std::fprintf(stderr, "ENDING → phase 2 (final scene idx=%d)\n", ending_idx);
        } else if (in.up) para.scroll_line(-1);
        else if (in.down) para.scroll_line(1);
        else if (in.pgup) para.scroll_page(-1);
        else if (in.select || in.pgdown || in.key == 'I' || in.key == 'K')
          para.scroll_page(1);                 // 逐頁下捲(未到底)
        if (max_frames >= 0 && ++frames >= max_frames) break;
        continue;
      }
      if (in.back) { para.close(); }                     // Esc:關閉回遊戲
      else if (in.up) para.scroll_line(-1);              // ↑:上捲一行
      else if (in.down) para.scroll_line(1);             // ↓:下捲一行
      else if (in.pgup) para.scroll_page(-1);            // PgUp:上翻一頁
      else if (in.pgdown || in.select || in.key == 'I' || in.key == 'K')
        para.scroll_page(1);                             // PgDn/Space/Enter/I/K:下翻一頁
      if (max_frames >= 0 && ++frames >= max_frames) break;
      continue;                                          // 段落檢視期間不處理移動
    }
    // 訊息檢視器啟用時:接管輸入(翻頁/關閉),暫停移動,翻頁鍵不誤觸移動。
    if (msg.active) {
      if (in.back) { msg.close(); }                      // Esc:直接關閉回遊戲
      else if (in.select || in.down || in.up || in.key == 'I')
        msg.advance();                                   // Space/Enter/↓/I:下一頁;末頁→關閉
      if (max_frames >= 0 && ++frames >= max_frames) break;
      continue;                                          // 訊息檢視期間不處理移動
    }
    // 遭遇 / 戰鬥畫面:F=戰鬥(推進一回合)、R=逃跑、Esc/Space=結束後離開。
    if (state == S_COMBAT) {
      if (enc.over) {
        if (in.back || in.select) {                      // 戰鬥已結束 → 離開遭遇
          enc.active = false;
          // 終戰 Namtar 勝利 → 進結局序列(收官);否則回地圖 / 離開。
          if (enc.is_namtar && enc.victory) { enter_ending(); }
          else if (level) state = S_GAME;
          else break;          // 有地圖回遊戲,否則(--encounter)離開
        }
      } else if (enc.casting) {                           // 施法選單開啟:接管輸入
        if (in.back) { enc.casting = false; }             // Esc:取消施法
        else if (in.up) { if (enc.cast_sel > 0) enc.cast_sel--; }
        else if (in.down) {
          if (enc.cast_sel + 1 < (int)enc.spellbook.size()) enc.cast_sel++;
        } else if (in.key >= '1' && in.key <= '9') {      // 數字熱鍵直接選並施放
          int n = in.key - '1';
          if (n < (int)enc.spellbook.size()) {
            std::uint8_t sid = enc.spellbook[n];
            enc.casting = false;
            if (enc.group) group_cast_round(sid); else cast_round(sid);
          }
        } else if (in.select) {                           // Enter:施放游標所選
          if (enc.cast_sel < (int)enc.spellbook.size()) {
            std::uint8_t sid = enc.spellbook[enc.cast_sel];
            enc.casting = false;
            if (enc.group) group_cast_round(sid); else cast_round(sid);
          }
        }
      } else if (in.key == 'C') {                          // C:開施法選單(群戰 + 單怪 demo)
        if (party.size() > 0)
          enc.spellbook = game::castable_spells(party.at(0), enc.hero_power);
        enc.cast_sel = 0; enc.casting = true;
      } else if (in.key == 'R' || in.back) {              // R/Esc:逃跑
        enc.fled = true; enc.over = true;
        if (enc.group && enc.group_loop) enc.group_loop->flee();
        enc.log.emplace_back(tr.tr("The party flees!"));
      } else if (enc.group && in.key == 'M') {             // M:強力一擊(Mighty Blow)
        special_attack_round(game::SpecialAttack::MightyBlow);
      } else if (enc.group && in.key == 'D') {             // D:卸武裝(Disarm)
        special_attack_round(game::SpecialAttack::Disarm);
      } else if (enc.group && in.key == 'A') {             // A:前進(Advance)
        special_attack_round(game::SpecialAttack::Advance);
      } else if (enc.group && in.key == 'Q') {             // Q:快速戰鬥(Quickly fight)
        special_attack_round(game::SpecialAttack::QuickFight);
      } else if (enc.group && in.key == 'E') {             // E:閃避敵人(Dodge enemies)
        special_attack_round(game::SpecialAttack::Dodge);
      } else if (in.key == 'F' || in.select) {            // F/Enter:打一回合(全體自動攻擊)
        if (enc.group) group_round(); else combat_round();
      }
      if (max_frames >= 0 && ++frames >= max_frames) break;
      continue;                                          // 戰鬥期間不處理移動
    }
    // 俯視平面地圖(S_MAP):Esc / `?` 關閉回遊戲;--automap headless 直接 dump 後退出。
    if (state == S_MAP) {
      if (in.back || in.key == '?') {
        if (automap_mode) break;        // --automap headless:dump 完即離開
        state = S_GAME;
      }
      if (max_frames >= 0 && ++frames >= max_frames) break;
      continue;
    }
    if (state == S_GAME) {                               // F:真實地圖移動(對齊說明書)
      if (in.back) { if (menu_mode) state = S_MENU; else break; }   // Esc:選單進入→返回;--map→離開
      // `?`:顯示俯視平面地圖(手冊)。進 S_MAP;觸發重畫。
      else if (in.key == '?') { minimap_dirty = true; state = S_MAP; }
      // S=儲存遊戲(手冊):寫檔 + 訊息提示(i18n「已儲存」/「存檔失敗」)。
      else if (in.key == 'S') {
        bool ok = do_save();
        open_msg(tr.tr(ok ? "Game saved." : "Save failed."));
        last_event_tile = -1;                            // 提示非事件格,離格不重觸發
      }
      // V=查看角色屬性表(手冊);數字 1-4 直接選該角色開表(暫停移動)。
      else if (in.key == 'V') { sheet.open((int)party.size(), 0); }
      // P=進商店買賣、T=進酒館招募(remake 設計入口鍵;原版以踩商店/酒館格觸發,
      //   remake 地圖事件格資料尚未對映商店/酒館類型,故先以快捷鍵 + headless --shop/--recruit 提供)。
      else if (in.key == 'P' && party.size() > 0) { shop_ui.open(); }
      else if (in.key == 'T' && party.size() > 0) { tavern_ui.open(); }
      else if (in.key == 'O' && party.size() > 0) { reorder_ui.open(); }  // O:重排隊伍順序(手冊)
      else if (in.key == 'G') { open_quest_guide(); }   // G:主線指引(remake 加值;迷路時看下一步)
      else if (in.key >= '1' && in.key <= '9' && party.size() > 0)
        sheet.open((int)party.size(), in.key - '1');
      else {
        if (in.left  || in.key == 'J') dir = (dir + 3) % 4;   // 左轉
        if (in.right || in.key == 'L') dir = (dir + 1) % 4;   // 右轉
        if (in.up    || in.key == 'I') {                      // 前進
          int nx = px + dx4[dir], ny = py + dy4[dir];
          // wrap 關卡(flag&2):走出邊緣 → modular 環繞到對邊(opendw exit(1) 未實作,
          // 以標準環繞慣例補上)。非 wrap 關卡 walkable_wrap 退回一般 walkable。
          bool moved = false;
          if (level && level->walkable_wrap(nx, ny)) {
            if (level->wraps()) { nx = level->wrap_x(nx); ny = level->wrap_y(ny); }
            // 探索互動門/密門/石牆閘(remake 設計;見 docs/gameplay/57_DOORS_TRAPS_TERRAIN.md):關門/鎖門未開、
            //   密門未破、石牆未軟化 → 仍擋路(像牆)。陷阱可走(踩格才結算)。
            std::uint8_t nt = level->tile(nx, ny);
            if (game::terrain_walkable(terrain, current_area, nx, ny, nt)) {
              px = nx; py = ny;
              moved = true;
              mark_seen_here();   // 對齊 refresh_viewport:踏上新格即標記 seen
              trigger_trap_here();  // 踩到陷阱格 → 結算傷害(未解除/未觸發時)
            }
          }
          // 撞牆:前進被擋(牆/門/石牆閘)→ 撞牆音效(func_5060[3] play_sound_wall_bump)。
          if (!moved) g_sound.play(audio::SoundId::WallBump);
        }
        // K:寶箱開箱(grounded)優先 —— 站在未開寶箱格按 K → 開鎖檢定 → 成功給真實物品 + 持久。
        if (in.key == 'K' && party.size() > 0 && chest_here && !chest_pool.empty()) {
          // 最高 Lockpick 者做檢定(同門邏輯;難度 10 remake 設計)。
          std::size_t bi = 0; int best = -1;
          for (std::size_t i = 0; i < party.size(); ++i) {
            if (party.at(i).status & 0x01) continue;
            int lp = (int)party.at(i).skills[game::progression::kLockpick];
            if (lp > best) { best = lp; bi = i; }
          }
          auto res = game::try_lockpick(party.at(bi), 10, terrain_rng);
          if (party_best_lockpick() <= 0 || !res.success) {
            open_msg(tr.tr("The chest remains locked."));
            std::fprintf(stderr, "chest locked @(%d,%d) lockpick=%d roll=%d\n",
                         px, py, party_best_lockpick(), res.roll);
          } else {
            // 決定性選一件真實物品(依位置;同箱恆給同物)。grounded:items.bin 為真實 DW 物品,
            //   非原版該箱 byte-exact 內容(深層 RE);add_item → raw_records → 存檔持久。
            std::size_t idx = (std::size_t)((current_area + px * 7 + py * 13) % (int)chest_pool.size());
            int slot = party.add_item(0, chest_pool[idx]);
            opened_chests.insert((long)current_area * 1000000 + (long)px * 1000 + py);
            chest_here = false;
            g_sound.play(audio::SoundId::DoorOpen);
            std::string itname = game::parse_item(chest_pool[idx].data(), 23).name;   // 物品名(7-bit 解碼)
            itname = itname.empty() ? tr.tr("item") : tr.tr(itname);                   // 專有名在地化(items.tsv;無譯回退英文)
            if (slot >= 0)
              open_msg(party.at(0).name + tr.tr(" gets the ") + itname + "!");
            else
              open_msg(party.at(0).name + tr.tr(" can't carry any more."));
            std::fprintf(stderr, "chest opened @(%d,%d) → item idx=%zu slot=%d '%s'\n",
                         px, py, idx, slot, itname.c_str());
          }
          last_event_tile = -1;
        } else
        // K:打開關閉的門 / 粉碎牆中密門(手冊 p176/184;遊戲層動作,opendw 未反編 → remake 設計)。
        if (in.key == 'K' && party.size() > 0) {
          using DA = game::DoorAction;
          DA act = open_door_forward();
          const char* key = nullptr;
          switch (act) {
            case DA::Opened:        key = "The door opens."; break;
            case DA::Unlocked:      key = "You pick the lock."; break;
            case DA::LockedNeedPick:key = "The door is locked."; break;
            case DA::SecretBroken:  key = "You smash a secret door!"; break;
            case DA::StoneBlocked:  key = "A solid stone wall blocks the way."; break;
            case DA::AlreadyOpen:   key = "The door is already open."; break;
            case DA::None:          key = "There is nothing to open ahead."; break;
          }
          if (key) { open_msg(tr.tr(key)); last_event_tile = -1; }
        }
        // C:施法(手冊;S_GAME 探索施法)。開法術選單(隊伍第 0 名可施法清單)。
        //   opendw 探索施法 op 未反編 → 結算為 remake 設計(見 docs/gameplay/57_DOORS_TRAPS_TERRAIN.md)。
        if (in.key == 'C' && party.size() > 0) {
          cast_ui.spellbook = game::castable_spells(party.at(0), (int)party.at(0).power);
          if (cast_ui.spellbook.empty()) {
            open_msg(tr.tr("No spells available.")); last_event_tile = -1;
          } else {
            cast_ui.open();
            std::fprintf(stderr, "explore cast: open (castable=%zu)\n",
                         cast_ui.spellbook.size());
          }
        }
        // 事件格(對拍 op_71:tile 值變了才觸發);事件文字 → 開訊息檢視器(分頁捲動)
        if (level) {
          int tv = level->tile(px, py);
          // ── 終戰 Namtar:area27(尼塞山腹)的 op_8A combat encounter 格(tile 0x18/0x19)──
          //   原版這兩格是 op_8A 遭遇(probe_encounter_id 實測;docs/gameplay/55 §3.2)。res3 全戰鬥閉環
          //   卡遊戲層 context(docs/reverse-engineering/42),故 remake 改接自有的 combat_loop:踩格 → begin_namtar
          //   (隊伍 vs Namtar Boss)。誠實標示:Boss 屬性/祝福 = remake 設計(combat.hpp)。
          if (current_area == 27 && (tv == 0x18 || tv == 0x19) && tv != last_event_tile) {
            last_event_tile = tv;
            std::fprintf(stderr, "area27 tile 0x%02X → 終戰 Namtar(combat_loop)\n", tv);
            begin_namtar();
          } else
          if (tv > 1 && tv != last_event_tile) {
            event_msg = run_event((std::uint8_t)tv); last_event_tile = tv;
            // 寶箱偵測(grounded):事件 emit「locked chest」且此格未開過 → 標記可 K 開箱。
            chest_here = (event_is_chest) &&
                         !opened_chests.count((long)current_area * 1000000 + (long)px * 1000 + py);
            // 對拍 load_level_resources:事件可能寫 gs[2]/gs[0..1]/gs[3] → 換 area 或傳送。
            int reloc = sync_relocation();   // 2=換 area(已重載) 1=同區傳送 -1=wrap 跳過
            if (reloc == 2) {
              // 換 area:事件文字仍顯示(若有),但事件格判定改用新區的格子。
              if (level) { int ntv = level->tile(px, py); last_event_tile = (ntv > 1) ? ntv : -1; }
            }
            if (reloc == 1 || reloc == 2) mark_seen_here();   // 傳送/換區後新格也標記 seen
            // Read Paragraph 事件 → 長段落捲動 overlay;一般事件 → 下半部分頁訊息框。
            if (event_para_n >= 0) {
              std::string full = para_text(event_para_n);   // 含 zh-TW 回退(en/ja 缺段落時)
              if (full.empty()) full = event_msg;           // 連 zh-TW 都查無 → 用 run_event 回退字樣
              open_para(event_para_n, full);                // 暫停移動
            } else if (!event_msg.empty()) open_msg(event_msg);  // 進訊息檢視(暫停移動)
          } else if (tv <= 1) { last_event_tile = -1; event_msg.clear(); }
        }
      }
    } else if (!menu_mode) { if (in.back) break; }       // sprite/scene 檢視:Esc/Q 離開
    else if (state == S_TITLE) {                          // 開機 splash:按任意鍵 → 主選單
      ++title_blink;                                       // 提示閃爍相位推進
      // 任意鍵(方向/Enter/Space/Esc/字母)→ 進主選單;F4 已於上方處理(切語系不離開 splash)。
      if (in.up || in.down || in.left || in.right || in.select || in.back ||
          in.key || in.pgup || in.pgdown)
        state = S_MENU;
      // in.quit(Q / 關窗)由迴圈尾端 in.quit 統一處理(離開遊戲)。
    }
    else if (state == S_MENU) {
      if (in.back) break;                                // 選單按 Esc = 離開
      // 1-4(或至隊伍人數):開該角色屬性表檢視「目前隊伍」(sheet.active 後由上方 sheet handler 接管)。
      if (in.key >= '1' && in.key <= '9' && party.size() > 0) {
        int idx = in.key - '1';
        if (idx < (int)party.size()) {
          sheet.open((int)party.size(), idx);
          std::fprintf(stderr, "menu: open party sheet [%d] %s\n", idx + 1, party.at((std::size_t)idx).name.c_str());
          if (max_frames >= 0 && ++frames >= max_frames) break;
          continue;
        }
      }
      int n = (int)opts.size();
      if (n) {
        if (in.up) sel = (sel - 1 + n) % n;
        if (in.down) sel = (sel + 1) % n;
        int trig = in.select ? sel : -1;
        if (in.key) for (int i = 0; i < n; ++i) if (opts[i].hot == in.key) trig = i;  // 快捷字母
        if (trig >= 0) {
          sel = trig;
          std::fprintf(stderr, "selected [%c] %s\n", opts[trig].hot, opts[trig].label.c_str());
          if (opts[trig].hot == 'B') { start_chargen(); }  // 開始新遊戲 → 建角畫面(手冊 B)
          else if (opts[trig].hot == 'C') {           // 繼續舊遊戲(手冊 C):有存檔→讀檔進遊戲;無→提示
            if (do_load(save_path)) { /* state=S_GAME(apply_state 已設) */ }
            else {
              state = S_BRANCH;
              branch_label_en = "No saved game.";
              branch_label = tr.tr(branch_label_en);
            }
          }
          else { state = S_BRANCH; branch_label = opts[trig].label; branch_label_en = opts[trig].en; }
        }
      }
    } else {                                             // S_BRANCH
      if (in.back || in.select) state = S_MENU;
    }
    if (max_frames >= 0 && ++frames >= max_frames) break;
  }
  vid.close();
  std::fprintf(stderr, "ok (frames=%d, state=%d, sel=%d)\n", frames, (int)state, sel);
  return 0;
}
