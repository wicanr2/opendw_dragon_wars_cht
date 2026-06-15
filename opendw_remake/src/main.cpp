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
//   D  快捷字母選單 + 狀態分支,操作與說明書一致(見 docs/CONTROLS.md):
//      B=開始新遊戲、C=繼續舊遊戲;↑↓/Enter 為輔助;Esc 返回 / Q 離開。
//
// 像素資產來自 bundle(自包含:dw8x8.bin + sprites/scenes/maps);文字字型用 host TTF。
//
// 用法:opendw_remake [--bundle DIR] [--font RAW] [--menu TSV] [--scale N]
//                     [--font-ttf PATH] [--pc N] [--sprite NAME] [--frames N]
//                     [--dump PPM] [--press CH] [--map N] [--fp] [--at X Y]
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>
#include <optional>
#include <string>
#include <vector>
#include "resource/provider.hpp"
#include "vm/interpreter.hpp"
#include "render/font.hpp"
#include "render/framebuffer.hpp"
#include "render/sprite.hpp"
#include "render/picture.hpp"
#include "render/viewport.hpp"
#include "render/viewport_compose.hpp"
#include "render/minimap.hpp"
#include "render/sdl_video.hpp"
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

  void open(int n, int start = 0) {
    count = n < 1 ? 0 : n;
    idx = start;
    if (count > 0) { if (idx < 0) idx = 0; if (idx >= count) idx = count - 1; }
    active = count > 0;
    show_inventory = false;
  }
  void toggle_view() { show_inventory = !show_inventory; }  // 屬性表 ⇄ 物品欄
  void close() { active = false; show_inventory = false; }
  void prev() { if (count > 0) idx = (idx - 1 + count) % count; }
  void next() { if (count > 0) idx = (idx + 1) % count; }
  // 數字鍵 1-count 直選;越界忽略。回傳是否命中。
  bool select(int n) {
    if (n >= 1 && n <= count) { idx = n - 1; return true; }
    return false;
  }
};

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
  std::string bundle = "assets/bundle";
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
  int at_x = -1, at_y = -1;       // --at x y:把玩家放到指定格(headless 驗證事件文字)
  int msg_page = 0;               // --msg-page N:訊息檢視器先翻到第 N 頁再 dump(headless 驗證分頁)
  int read_para = -1;             // --read-para N:直接開段落 N 進捲動 overlay(headless 驗證長段落)
  int para_scroll = 0;            // --para-scroll N:dump 前先「逐頁」下捲 N 次(headless 驗證跨頁無遺漏)
  int char_sheet = -1;            // --char-sheet N:直接開第 N 名(1-based)角色屬性表(headless 驗證)
  bool show_inventory = false;    // --inventory:配合 --char-sheet 直接開物品欄(背包)子畫面
  int automap_area = -1;          // --automap N:headless 直接開第 N 區俯視平面地圖(`?` 鍵功能)
  int mm_seed = 0;                // --mm-seed:0=全圖探索 1=只玩家格 2=不 seed(測試/展示)
  bool mm_seed_set = false;       // 是否顯式給 --mm-seed;否則遊戲內用真實 fog of war
  std::string dump, sprite_name, scene_name;
  std::string save_path = "save/slot0.sav";  // 存/讀檔預設路徑(cwd 可寫處;見 .gitignore)
  std::string load_path;        // --load <path>:啟動即讀檔還原(進遊戲)
  bool selftest_save = false;   // --selftest-save:headless round-trip 自測(印 PASS/FAIL)
  bool viewport_mode = false;   // --viewport:顯示原版第一人稱 viewport 靜態框架
  bool fp_mode = false;         // --fp:S_GAME 用第一人稱 viewport(取代俯視彩格)
  int encounter_id = -1;        // --encounter N:直接進遭遇畫面(怪物表 index N)
  unsigned combat_seed = 0x1234;// --combat-seed N:結算 RNG 種子(確定性)
  int combat_rounds = 0;        // --combat-rounds N:dump 前自動打 N 回合(headless 驗證戰報)
  int combat_count = 6;         // --combat-count N:怪群數量(預設 6;沿用怪物表領頭怪)
  int cast_spell_id = -1;       // --cast <spellId>:headless 在遭遇中施放該法術一次(驗證)
  bool cast_force = false;      // --cast-force:即使該角色未習得也施放(僅供驗證效果套用)
  // ── 建角流程(新遊戲 / 建立人物;手冊選單 B)──
  bool newgame = false;         // --newgame:啟動直接進建角畫面(S_CREATE)
  std::string newgame_demo;     // --newgame-demo SPEC:headless 腳本化建角 + 出圖/驗證(見下方解析)
  std::string newgame_screen;   // --newgame-screen SPEC:停在配點畫面供截圖(同 SPEC 格式,只取第一員)
  for (int i = 1; i < argc; ++i) {
    auto eq = [&](const char* f) { return !std::strcmp(argv[i], f); };
    if (eq("--bundle") && i + 1 < argc) bundle = argv[++i];
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
    else if (eq("--dump") && i + 1 < argc) dump = argv[++i];
    else if (eq("--sprite") && i + 1 < argc) sprite_name = argv[++i];
    else if (eq("--scene") && i + 1 < argc) scene_name = argv[++i];
    else if (eq("--map") && i + 1 < argc) map_area = std::atoi(argv[++i]);   // 直接進某區地圖
    else if (eq("--automap") && i + 1 < argc) automap_area = std::atoi(argv[++i]);  // headless 開俯視地圖
    else if (eq("--mm-seed") && i + 1 < argc) { mm_seed = std::atoi(argv[++i]); mm_seed_set = true; } // 探索旗標 seeding
    else if (eq("--at") && i + 2 < argc) { at_x = std::atoi(argv[++i]); at_y = std::atoi(argv[++i]); }  // 玩家落點(測試)
    else if (eq("--press") && i + 1 < argc) press = std::toupper((unsigned char)argv[++i][0]);  // 模擬按鍵(測試)
    else if (eq("--msg-page") && i + 1 < argc) msg_page = std::atoi(argv[++i]);   // 訊息檢視先翻到第 N 頁再 dump
    else if (eq("--read-para") && i + 1 < argc) read_para = std::atoi(argv[++i]); // 直接開段落 N 進捲動 overlay
    else if (eq("--para-scroll") && i + 1 < argc) para_scroll = std::atoi(argv[++i]); // dump 前逐頁下捲 N 次
    else if (eq("--char-sheet") && i + 1 < argc) char_sheet = std::atoi(argv[++i]); // 直接開第 N 名角色屬性表
    else if (eq("--inventory")) show_inventory = true;                               // 配合 --char-sheet 開物品欄
    else if (eq("--load") && i + 1 < argc) load_path = argv[++i];        // 啟動讀檔還原
    else if (eq("--save-path") && i + 1 < argc) save_path = argv[++i];   // 覆寫存/讀檔路徑
    else if (eq("--selftest-save")) selftest_save = true;               // round-trip 自測
    else if (eq("--viewport")) viewport_mode = true;   // 顯示原版 viewport 靜態框架
    else if (eq("--fp")) fp_mode = true;               // 第一人稱 viewport(透視牆面)
    else if (eq("--encounter") && i + 1 < argc) encounter_id = std::atoi(argv[++i]);  // 進遭遇畫面(怪物 index)
    else if (eq("--combat-seed") && i + 1 < argc) combat_seed = (unsigned)std::strtoul(argv[++i], nullptr, 0);
    else if (eq("--combat-rounds") && i + 1 < argc) combat_rounds = std::atoi(argv[++i]);  // dump 前自動打 N 回合
    else if (eq("--combat-count") && i + 1 < argc) combat_count = std::atoi(argv[++i]);    // 怪群數量
    else if (eq("--cast") && i + 1 < argc) cast_spell_id = (int)std::strtoul(argv[++i], nullptr, 0);  // headless 施放法術
    else if (eq("--cast-force")) cast_force = true;     // 即使未習得也施放(驗證效果)
    else if (eq("--newgame")) newgame = true;           // 啟動即進建角畫面
    else if (eq("--newgame-demo") && i + 1 < argc) newgame_demo = argv[++i];  // 腳本化建角(headless)
    else if (eq("--newgame-screen") && i + 1 < argc) newgame_screen = argv[++i];  // 停在配點畫面截圖
  }
  if (scale < 1) scale = 1;

  auto font = render::Font8x8::load_table(font_raw);
  if (!font) { std::fprintf(stderr, "font load failed: %s\n", font_raw.c_str()); return 1; }
  const bool scene_mode = !scene_name.empty();
  const bool sprite_mode = !sprite_name.empty();
  const bool encounter_mode = encounter_id >= 0;
  const bool automap_mode = automap_area >= 0;
  const bool menu_mode = !scene_mode && !sprite_mode && !viewport_mode &&
                         !encounter_mode && map_area < 0 && !automap_mode;
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
    // Read paragraph 段落書(隨 locale);缺檔則回退「Read paragraph N」。
    book = res::ParagraphBook::load(bundle + "/paragraphs", loc);
    if (book) std::fprintf(stderr, "paragraphs: loaded %zu (locale=%s)\n", book->size(), loc.c_str());
    else std::fprintf(stderr, "paragraphs: none for locale=%s (fallback to 'Read paragraph N')\n", loc.c_str());
    // 角落語系指示(可讀短標)。
    if (loc == "zh-TW") locale_tag = "[繁中]";
    else if (loc == "en") locale_tag = "[EN]";
    else if (loc == "ja") locale_tag = "[日]";
    else locale_tag = "[" + loc + "]";
    std::fprintf(stderr, "locale = %s %s\n", loc.c_str(), locale_tag.c_str());
  };
  load_locale(locales.empty() ? locale : locales[locale_idx]);

  std::string header, header_en;   // header_en = 提示英文源(F4 重譯)
  std::vector<Opt> opts;
  int sel = 0;
  enum { S_MENU, S_BRANCH, S_GAME, S_COMBAT, S_MAP, S_CREATE } state = S_MENU;
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
  // 怪物表(res31 萃取,oracle 對拍 25 筆);遭遇畫面用。
  std::vector<game::MonsterRecord> monsters = game::MonsterTable::load(bundle);
  int level_res = -1;             // 當前關卡資源 index(= area + 0x46;= word_3AE8)
  int current_area = -1;          // 當前所在區域(存檔用;= level_res - 0x46)
  // 俯視地圖 fog of war:per-area「已看過」格;玩家每步標記當前格(對齊
  // opendw refresh_viewport,engine.c:5688)。存檔保存;換 area 各關獨立。
  game::SeenMap seen;
  // 持久 VM 遊戲狀態(對拍 opendw game_state.unknown[256]):跨事件保留,存檔/讀檔的核心欄位。
  // run_event 跑事件腳本時以此為初值並回寫,使旗標(門/開關/劇情)能持久累積。
  std::array<std::uint8_t, 256> game_state{};
  std::string event_msg;          // 踩到事件格時跑 script emit 的文字(原文,F4 重排用)
  int last_event_tile = -1;       // 對拍 op_71:tile 值變了才觸發
  MsgViewer msg;                  // 一般事件訊息檢視器(下半部分頁;active 時暫停移動)
  ParaViewer para;                // Read Paragraph 長段落捲動檢視器(全螢幕 overlay;active 時暫停移動)
  CharSheet sheet;                // 角色屬性表檢視子狀態(V / 數字 1-4 進;active 時暫停移動)
  CharGenUi cg;                   // 新遊戲建角流程(選單 B → S_CREATE;見 CharGenUi)
  // run_event 攔到「Read paragraph N」時,把 N 寫進此處(>=0 表示本次事件是段落觸發);
  // main 偵測後改開 ParaViewer(長段落捲動)而非一般訊息框。-1 = 非段落事件。
  int event_para_n = -1;

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
    std::vector<std::uint8_t> spellbook;   // 已習得且當前可施法的法術 id(castable_spells)
    bool casting = false;                  // 施法選單開啟中(C 進;上下選;Enter 施放;Esc 取消)
    int cast_sel = 0;                      // 施法選單游標
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
    if (!level) return "";
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
      if (!out.empty()) out += ' ';
      out += t;
    });
    ip.run();
    game_state = st.game_state;   // 回寫:事件對遊戲狀態的修改持久保留
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
      std::fprintf(stderr, "area switch %d->%d SKIPPED: target uses wrap boundary (flag&2), "
                   "opendw leaves this unimplemented\n", current_area, new_area);
      game_state[2] = (std::uint8_t)current_area;
      return -1;
    }
    // 乾淨重載(等價 load_level_resources 的 resource_load(area+0x46) + read_level_metadata)。
    if (!enter_map(new_area)) {
      game_state[2] = (std::uint8_t)current_area;
      return -1;
    }
    // 套用事件指定的入口座標/朝向(enter_map 預設落在第一可走格,這裡覆寫成腳本值)。
    px = gx; py = gy; dir = gf;
    game_state[0] = (std::uint8_t)px; game_state[1] = (std::uint8_t)py; game_state[3] = (std::uint8_t)dir;
    last_event_tile = -1; event_msg.clear();         // 新區不立即重觸發進入格事件
    std::fprintf(stderr, "AREA SWITCH %d->%d entry=(%d,%d) dir=%d%s\n",
                 old_area, new_area, px, py, dir,
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
      if (tv > 1) {
        event_msg = run_event((std::uint8_t)tv); last_event_tile = tv;
        std::fprintf(stderr, "at (%d,%d) tile=0x%02X event=\"%s\"\n", px, py, tv, event_msg.c_str());
        sync_relocation();   // 事件可能換 area / 傳送(headless 也套用,供 --map+--at 驗證)
      }
    }
  }

  // --automap N:headless 直接進第 N 區的俯視平面地圖(`?` 鍵功能)。
  if (automap_mode) {
    if (!enter_map(automap_area)) return 1;
    state = S_MAP;
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
      if (level && level->walkable(nx, ny)) { px = nx; py = ny; }
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

  // 第一人稱 viewport 資源(--fp 或選單 B 進遊戲時用):元件 bundle + 靜態框架模板。
  render::ComponentStore comps(bundle + "/components");

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
    // ── A':遭遇畫面 ──(像素層由 begin_encounter/draw_encounter 於下方 render 迴圈處理)
    if (monsters.empty()) { std::fprintf(stderr, "monsters.bin missing\n"); return 1; }
    if (encounter_id >= (int)monsters.size()) {
      std::fprintf(stderr, "encounter id %d >= %zu monsters\n", encounter_id, monsters.size());
      return 1;
    }
    // 實際進場由下方 begin_encounter(encounter_id) 完成(lambda 需先定義)。
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
  // 640×480 模式:像素層固定 ×2(scale=2),故 scale 概念對文字層 = 2;字級「解綁」
  //   為固定原生 px(CJK 24 / UI 16 / 標題 48),不隨 scale 縮放(這正是 docs/47 方案 3 要點)。
  // 一般 scale 模式:原生字級隨 scale 等比(基準 scale=3)。
  const int eff_scale = win640 ? 2 : scale;   // 文字/版面虛擬座標換算用的有效倍率
  const int PX_TITLE = win640 ? 48 : 48 * scale / 3;   // 標題「火龍之戰」
  const int PX_BODY  = win640 ? 24 : 24 * scale / 3;   // CJK 內文(選單/事件/段落)
  const int PX_UI    = win640 ? 16 : 16 * scale / 3;   // ASCII UI(關卡名/控制提示)

  // 文字層:大標題走 tr("Dragon Wars")(zh→火龍之戰、en→Dragon Wars、ja→ドラゴンウォーズ)。
  auto add_title = [&]() { tl.add(8, 6, tr.tr("Dragon Wars"), 14, PX_TITLE); };
  // 文字層:角落語系指示 + F4 提示(每幀重繪,即時反映當前語系)。
  auto add_lang_badge = [&]() {
    tl.add(render::kW - 56, 2, locale_tag, 11, PX_UI);   // 右上角:[繁中]/[EN]/[日]
    tl.add(render::kW - 78, 13, "F4:lang", 8, PX_UI * 3 / 4);
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

  // 框底實心 + 邊框(像素層);疊在 viewport/地圖之上。half=半透明感(暗藍底)。
  auto fill_msg_box = [&]() {
    for (int y = MB_Y; y < MB_Y + MB_H && y < render::kH; ++y)
      for (int x = MB_X; x < MB_X + MB_W && x < render::kW; ++x)
        fb.put(x, y, 1);                              // 深藍實心底
    for (int x = MB_X; x < MB_X + MB_W; ++x) {        // 上下邊框
      fb.put(x, MB_Y, 15); fb.put(x, MB_Y + MB_H - 1, 15);
    }
    for (int y = MB_Y; y < MB_Y + MB_H; ++y) {        // 左右邊框
      fb.put(MB_X, y, 15); fb.put(MB_X + MB_W - 1, y, 15);
    }
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

  // 段落 overlay 底框 + 邊框(像素層;深藍底 + 亮框,對齊既有 UI 風格)。
  auto fill_para_box = [&]() {
    for (int y = PB_Y; y < PB_Y + PB_H && y < render::kH; ++y)
      for (int x = PB_X; x < PB_X + PB_W && x < render::kW; ++x)
        fb.put(x, y, 1);                               // 深藍實心底
    for (int x = PB_X; x < PB_X + PB_W; ++x) {          // 上下邊框
      fb.put(x, PB_Y, 15); fb.put(x, PB_Y + PB_H - 1, 15);
    }
    for (int y = PB_Y; y < PB_Y + PB_H; ++y) {          // 左右邊框
      fb.put(PB_X, y, 15); fb.put(PB_X + PB_W - 1, y, 15);
    }
    // 標題列下方一道分隔線(像素層),把標題與內文分開。
    int sep_y = PB_TITLE_TOP + PB_LINE_H + 1;
    for (int x = PB_X + 1; x < PB_X + PB_W - 1; ++x) fb.put(x, sep_y, 8);
  };

  // 畫段落捲動 overlay:底框 + 標題「段落 N」+ 可見行切片 + 捲動位置提示(▲/▼/頁碼)。
  auto draw_para_overlay = [&]() {
    fill_para_box();
    // 標題列:「段落 N」(i18n「段落」+ 數字)。
    char title[48];
    std::snprintf(title, sizeof title, "%s %d", tr.tr("Paragraph").c_str(), para.para_n);
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

    row("Strength",  c.strength,  c.max_strength);
    row("Dexterity", c.dexterity, c.max_dexterity);
    row("Intel",     c.intel,     c.max_intel);
    row("Spirit",    c.spirit,    c.max_spirit);
    row("Health",    c.health,    c.max_health);
    row("Stun",      c.stun,      c.max_stun);
    row("Power",     c.power,     c.max_power);
    row1("Level",    std::to_string(c.level));
    row1("Gold",     std::to_string(c.gold));
    row1("Status",   tr.tr(game::Party::status_key(c.status)),
         c.status ? 12 : 11);                            // 異常亮紅,正常亮綠
    // 性別(原版 record 0x4E:0 男 / 1 女)。
    row1("Gender", tr.tr(c.gender ? "Female" : "Male"), 7);

    // 底部操作提示。
    int iy = CS_Y + CS_H - CS_LINE_H - 2;
    tl.add(tx, iy, tr.tr("[ continue ]"), 8, PX_UI);
    tl.add(CS_VAL_X, iy, "1-4  E:Items  Esc", 8, PX_UI);
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
    int shown = 0;
    for (int s = 0; s < (int)inv.size(); ++s) {
      const auto& it = inv[s];
      if (!it.present) continue;
      ++shown;
      // 名稱(白;已裝備亮綠)。
      std::uint8_t ncol = it.equipped ? 10 : 15;
      std::string nm = it.name.empty() ? tr.tr("(empty)") : it.name;
      tl.add(tx, y, nm, ncol, PX_BODY);
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

    int iy = CS_Y + CS_H - CS_LINE_H - 2;
    tl.add(tx, iy, tr.tr("[ continue ]"), 8, PX_UI);
    tl.add(CS_VAL_X, iy, "1-4  E:Stats  Esc", 8, PX_UI);
  };

  // ── 建角畫面(S_CREATE):全螢幕,像素層底 + 文字層(TTF / i18n)。──
  // PhName:名字輸入列(游標 _)。PhAttr:四屬性配點 + 衍生值 + 剩餘點數 + 性別 + 已建隊員。
  auto draw_chargen = [&]() {
    fb.clear(1);
    add_title();
    add_lang_badge();
    int x = 16, y = 36;
    // 標題:「建立人物  (已建 N/4)」。
    char head[80];
    std::snprintf(head, sizeof head, "%s  (%d/%d)", tr.tr("Create Character").c_str(),
                  (int)cg.done_records.size(), CharGenUi::kMaxParty);
    tl.add(x, y, head, 14, PX_BODY); y += 18;

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
      tl.add(x, y, dv2, 11, PX_UI); y += 18;
      tl.add(x, y, tr.tr("Up/Down select  +/- adjust  G gender  Enter done"), 8, PX_UI);
      y += 12;
      tl.add(x, y, tr.tr("B: begin  N: add member  Esc: back"), 8, PX_UI);
    }

    // 右側:已建隊員清單(名 + STR/DEX/INT/SPI)。
    int rx = render::kW - 130, ry = 36;
    tl.add(rx, ry, tr.tr("Party"), 14, PX_UI); ry += 14;
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
      tl.add(rx, ry, line, 15, PX_UI); ry += 12;
    }
  };

  auto draw_menu = [&]() {
    fb.clear(1);
    add_title();
    add_lang_badge();
    int y = 40;
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
  //   (docs/26_MONSTERS_AND_SPRITES.md 已記:需逐一視覺核對),故此處用「怪物名 → 已
  //    視覺核對過的 bundle sprite」對照表;查無則回退第一個 spider/wolf,再無則畫空框。
  //   sprite 圖渲染路徑本身(.spr indexed blit)已由 sprite_dump golden 對拍 oracle。
  auto sprite_for_monster = [&](const std::string& name) -> std::optional<render::Sprite> {
    auto load = [&](const char* file) {
      return render::Sprite::load(bundle + "/sprites/" + file + ".spr");
    };
    // 名稱關鍵字 → 已核對 sprite 檔(視覺核對來源:docs/26 contact sheet)。
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
    enc.sprite = sprite_for_monster(monsters[idx].name);
    enc.rng = game::CombatRng((std::uint16_t)combat_seed);
    if (party.size() > 0) {
      const auto& c0 = party.at(0);
      enc.hero = game::Combatant::from_player(c0);
      enc.hero_power = (int)c0.power;        // 施法者法力池(record Power[28-31])
      enc.hero_str = (int)c0.strength;       // PowerScaled / +STR 結算用
      enc.spellbook = game::castable_spells(c0, enc.hero_power);  // 已習得且可施法
    } else { enc.hero.name = "Hero"; enc.hero.is_player = true; enc.hero.hp = enc.hero.max_hp = 20;
           enc.hero.av = 5; enc.hero.dv = 5; enc.hero.ac = 0; enc.hero.dmg_dice = 1; enc.hero.dmg_sides = 6;
           enc.hero_power = 0; enc.hero_str = 12; enc.spellbook.clear(); }
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
  // 把 CombatLoop 累積的新事件轉成在地化戰報行,追加進 enc.log。
  //   DOS 格式(docs/43 §11):
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
      if (e.hit) {
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
      if (!enc.xp_awarded) { party.award_xp(game::kXpPerVictory); enc.xp_awarded = true; }
    } else if (o == O::Defeat) {
      enc.defeat = true; enc.over = true;
      enc.log.emplace_back(tr.tr("The party has fallen."));
    }
    while (enc.log.size() > 4) enc.log.erase(enc.log.begin());
  };
  // 群戰施法回合:隊伍第 0 名施放 spell_id(走 CombatLoop::cast,依 SpellTarget 自動鋪對象)。
  //   傷害/治療/buff/控制全部結算(grounded 手冊;控制持續/逃離為 remake 設計)。
  //   施法後若戰鬥未結束 → 推進一個怪群回合(怪反擊)。Power 照扣。
  auto group_cast_round = [&](std::uint8_t spell_id) {
    if (!enc.group_loop || enc.over) return;
    const game::SpellDef* sp = game::find_spell(spell_id);
    if (!sp) return;
    game::CastResult cr = enc.group_loop->cast(spell_id, enc.hero_power, enc.hero_str,
                                               /*caster_is_player=*/true);
    if (!cr.ok) { enc.log.emplace_back(tr.tr("Not enough power")); return; }
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
    } else if (sp->effect == game::SpellEffect::Heal) {
      std::snprintf(buf, sizeof buf, "%s casts %s heals %d", caster.c_str(),
                    sp->name_key, cr.amount);
    } else if (cr.handled && cr.amount > 0) {  // 傷害類
      std::snprintf(buf, sizeof buf, "%s casts %s on %s %d damage", caster.c_str(),
                    sp->name_key, tr.tr(enc.mon_name_en).c_str(), cr.amount);
    } else if (cr.handled) {  // buff/debuff
      std::snprintf(buf, sizeof buf, tr.tr("%s casts %s").c_str(), caster.c_str(),
                    tr.tr(sp->name_key).c_str());
    } else {  // 工具/召喚(仍 TODO)
      std::snprintf(buf, sizeof buf, "%s casts %s TODO", caster.c_str(), sp->name_key);
    }
    enc.log.emplace_back(buf);
    enc.shown_events = enc.group_loop->events().size();  // cast 已自行追加事件,跳過免重複翻譯
    // 控制清場 / 致死可能提早結束。
    using O = game::CombatOutcome;
    O o = enc.group_loop->outcome();
    if (o == O::Victory) {
      enc.victory = true; enc.over = true;
      enc.log.emplace_back(tr.tr("Each member gets 80 experience points for combat."));
      if (!enc.xp_awarded) { party.award_xp(game::kXpPerVictory); enc.xp_awarded = true; }
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
        game::cast_spell(spell_id, enc.hero_power, enc.hero_str, tgt, enc.rng);
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
    // 怪物圖:畫進 framebuffer (16,8),160×136(對齊 oracle viewport 區)。
    if (enc.sprite) enc.sprite->blit(fb, 16, 8, 6);  // 6 = encounter 棕色背景透明
    else { // 無 sprite → 畫空框(像素層),維持版面對齊。
      for (int x = 16; x < 16 + 160; ++x) { fb.put(x, 8, 8); fb.put(x, 8 + 135, 8); }
      for (int y = 8; y < 8 + 136; ++y) { fb.put(16, y, 8); fb.put(16 + 159, y, 8); }
    }
    // 怪群描述(i18n;viewport 上方):「N 隻 {怪名}」(存活/總數)。單怪時只顯示怪名。
    if (enc.group && enc.group_loop && enc.mon_count > 1) {
      char gbuf[128];
      // zh-TW:「6 隻 禁衛軍(存活 4)」;en passthrough:「6 King's Guard (4 left)」。
      std::snprintf(gbuf, sizeof gbuf, tr.tr("%d %s (%d left)").c_str(),
                    enc.mon_count, tr.tr(enc.mon_name_en).c_str(),
                    enc.group_loop->monsters_alive());
      tl.add(16, 2, gbuf, 14, PX_UI);
    } else {
      tl.add(16, 2, tr.tr(enc.mon_name_en), 14, PX_UI);
    }
    // 右側隊伍狀態面板(沿用 party_panel)。
    party.draw_status_panel(fb, tl, PX_UI);
    add_lang_badge();
    // 選單列(熱鍵對齊原版戰鬥選單資源 Section 0x12:Fight / Run + 施咒 C)。
    int menu_y = 8 + 136 + 4;
    tl.add(16, menu_y, tr.tr("F:Fight  R:Run"), 11, PX_UI);
    tl.add(16 + 110, menu_y, tr.tr("C:Cast"), 11, PX_UI);
    // 施法選單(C 開啟):列出可施法術(已習得 + Power 足夠),熱鍵 1-9 + 上下游標。
    if (enc.casting) {
      int cy = menu_y + 14;
      char hbuf[160];
      std::snprintf(hbuf, sizeof hbuf, "%s (PW %d)", tr.tr("Choose spell:").c_str(),
                    enc.hero_power);
      tl.add(16, cy, hbuf, 14, PX_UI); cy += 12;
      if (enc.spellbook.empty()) {
        tl.add(24, cy, tr.tr("No spells"), 7, PX_UI);
      } else {
        for (int i = 0; i < (int)enc.spellbook.size() && i < 9; ++i) {
          const game::SpellDef* s = game::find_spell(enc.spellbook[i]);
          if (!s) continue;
          std::snprintf(hbuf, sizeof hbuf, "%c%d %s (%d)",
                        i == enc.cast_sel ? '>' : ' ', i + 1,
                        tr.tr(s->name_key).c_str(), s->power_cost);
          tl.add(24, cy, hbuf, i == enc.cast_sel ? 15 : 7, PX_UI);
          cy += 12;
        }
      }
      add_lang_badge();
      return;  // 施法選單期間不疊戰報(避免版面擁擠)
    }
    // 戰鬥 log(在地化:hit/miss/slain 走 tr;含數字部分原樣)。
    int ly = menu_y + 14;
    for (const auto& line : enc.log) {
      // 把鍵化片段(hit/miss/slain/->)逐字保留;只翻可翻片段太細,這裡整行顯示英文鍵
      // + 末行若 over 顯示在地化結果。簡化:整行直接顯示(英文戰報)。
      tl.add(16, ly, line, 7, PX_UI); ly += 12;
    }
    if (enc.over) {
      std::string tail;
      int tail_col = 12;
      if (enc.fled) tail = tr.tr("The party flees!");
      else if (enc.victory) { tail = tr.tr("Victory!"); tail_col = 10; }   // 亮綠
      else if (enc.defeat) { tail = tr.tr("The party has fallen."); tail_col = 4; }  // 暗紅
      else tail = enc.group ? tr.tr("Victory!")
                            : (!enc.mon.alive() ? (tr.tr(enc.mon_name_en) + " " + tr.tr("slain"))
                                                : (enc.hero.name + " down"));
      // 結果橫幅放右側面板下方空白區(y≈110;不與下方戰報 log 爭垂直空間)。
      int rx = 16 + 160 + 8;     // 右側欄起點(對齊隊伍面板 x)
      int ry = 110;
      tl.add(rx, ry, tail, tail_col, PX_UI);
      if (enc.victory)           // 勝利:右欄顯示簡短 XP 橫幅(全文在下方戰報 log)
        tl.add(rx, ry + 14, tr.tr("Each member +80 XP"), 14, PX_UI);
      tl.add(rx, ry + 28, tr.tr("[ continue ]"), 8, PX_UI);
    }
  };
  auto draw_game = [&]() {
    // F:真實關卡俯視圖(從 .lvl 解出的 tile 格,像素層)+ 玩家朝向;文字走文字層。
    fb.clear(0);
    if (!level) return;
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
      party.draw_status_panel(fb, tl, PX_UI);
      tl.add(8, 2, level->name, 14, PX_UI);              // 文字層:關卡名
    }
    add_lang_badge();
    int hint_y = oy + H * cs + 6;
    if (!msg.active && !para.active && !sheet.active)    // 子畫面期間隱藏控制提示(避免穿透框)
      tl.add(8, hint_y, "I:fwd  J/L:turn  K:door  V:stats  S:save  Esc:back", 7, PX_UI);
    // 事件/段落文字改走訊息檢視器(draw_msg_overlay,疊在最上層;見 render_now)。
  };
  // F+:第一人稱 viewport(透視牆面,像素層)。port 自 opendw refresh_viewport →
  //   update_viewport(靜態框架)→ ui_update_viewport。對拍 verify_fp 4/4(像素層不變)。
  auto draw_game_fp = [&]() {
    fb.clear(0);
    if (!level) return;
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
      party.draw_status_panel(fb, tl, PX_UI);
      tl.add(8, 2, level->name, 14, PX_UI);              // 文字層:關卡名
    }
    add_lang_badge();
    if (!msg.active && !para.active && !sheet.active)    // 子畫面期間隱藏控制提示(避免穿透框)
      tl.add(8, 150, "I:fwd  J/L:turn  K:door  V:stats  S:save  Esc:back", 7, PX_UI);
    // 事件/段落文字改走訊息檢視器(draw_msg_overlay,疊在最上層;見 render_now)。
  };
  // 俯視平面地圖(`?` 鍵)。port 自 opendw process_minimap_commands:
  //   Minimap::render 組 minimap viewport_memory(已對拍 golden 36864B),
  //   再 blit 到 framebuffer 左上(對齊原版 draw_rectangle(1,0,39,192) 清空區)。
  auto draw_automap = [&]() {
    fb.clear(0);
    if (!level) return;
    if (minimap_ok && minimap_dirty) {
      // --mm-seed 顯式給值(測試/展示)→ 用 Seed 模式;否則用遊戲內真實 fog of war。
      if (mm_seed_set) {
        minimap.render(*level, px, py, comps, minimap_seed());
      } else {
        const std::vector<std::uint8_t>* bm = seen.bitmap(current_area);
        minimap.render_with_seen(*level, px, py, comps,
                                 bm ? bm->data() : nullptr, level->w, level->h);
      }
      minimap_dirty = false;
    }
    if (minimap_ok) minimap.to_framebuffer(fb, /*ox=*/1, /*oy=*/8, /*rows=*/0xC0);
    if (!para.active) tl.add(8, 2, level->name, 14, PX_UI);   // 文字層:關卡名
    add_lang_badge();
    tl.add(8, 188, tr.tr("Map  -  Esc: back"), 7, PX_UI);     // 圖例(i18n)
  };
  // sprite/scene/viewport 靜態檢視:像素層已於前面建好;文字層每幀補上標籤。
  auto draw_static_text = [&]() {
    if (sprite_mode) tl.add(8, 4, sprite_name, 15, PX_UI);
  };
  auto render_now = [&]() {
    tl.clear();                                      // 每幀重建文字層
    if (state == S_COMBAT) { draw_encounter(); return; }  // 遭遇 / 戰鬥畫面
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
      return;
    }
    if (!menu_mode) { draw_static_text(); return; }  // sprite/scene/viewport:像素層靜態,只補文字
    if (state == S_MENU) draw_menu();
    else draw_branch();
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
    std::fprintf(stderr, "char sheet: showing character %d/%zu (\"%s\")%s\n",
                 sheet.idx + 1, party.size(), party.at((std::size_t)sheet.idx).name.c_str(),
                 show_inventory ? " [inventory]" : "");
  }
  // --encounter N:進遭遇畫面(headless 可 --dump 驗證圖層;互動下 F 戰鬥 / R 逃跑)。
  if (encounter_mode) {
    begin_encounter(encounter_id);
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
    // --combat-rounds N:自動打 N 回合(headless 驗證戰報 / 確定性)。群戰走 group_round。
    for (int r = 0; r < combat_rounds && !enc.over; ++r) {
      if (enc.group && cast_spell_id < 0) group_round(); else combat_round();
    }
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
  }
  render_now();

  if (!dump.empty()) {
    if (vid.dump_ppm(fb, dump))
      std::fprintf(stderr, "dumped composed frame (%dx%d) to %s\n", vid.out_w(), vid.out_h(), dump.c_str());
    else
      std::fprintf(stderr, "dump failed: %s\n", dump.c_str());
  }

  int frames = 0;
  for (;;) {
    render_now();
    vid.present(fb);
    render::Input in = vid.poll();
    // 建角命名階段:'q' 是合法名字字元(如 "Quinn"),不應觸發離開。
    //   poll 把 Q 同時設 quit 與 text_char='q'/'Q' → 命名時改當文字輸入,吃掉 quit。
    if (in.quit && state == S_CREATE && cg.active && cg.phase == CharGenUi::PhName &&
        in.text_char) {
      in.quit = false;
    }
    if (in.quit) break;
    // F4:即時循環切換語系 → 重載字串/段落書 → 重譯所有 widget。
    // 因每幀重繪(render_now),畫面立即變為新語言;事件文字重跑該關腳本重譯。
    if (in.cycle_lang && !locales.empty()) {
      locale_idx = (locale_idx + 1) % (int)locales.size();
      load_locale(locales[locale_idx]);
      relocalize();                                      // 選單/branch 重譯
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
    // 角色屬性表啟用時:接管輸入(切角色/關閉),暫停移動。
    //   ↑↓ 或數字 1-4 切角色;Esc 關閉。F4(語系)已於上方處理。
    if (sheet.active) {
      if (in.back) { sheet.close(); }                    // Esc:關閉回遊戲
      else if (in.up) sheet.prev();
      else if (in.down) sheet.next();
      else if (in.key >= '1' && in.key <= '9') sheet.select(in.key - '0');
      else if (in.key == 'E') sheet.toggle_view();       // E:屬性表 ⇄ 物品欄
      else if (in.key == 'V') sheet.close();             // V 再按一次 → 關閉
      if (max_frames >= 0 && ++frames >= max_frames) break;
      continue;                                          // 屬性表期間不處理移動
    }
    // Read Paragraph 長段落捲動 overlay 啟用時:接管輸入,暫停移動。
    //   ↑↓:逐行捲動;PgUp/PgDn / Space / Enter / I / K:逐頁;Esc:關閉回遊戲。
    //   (放在 msg 之前;兩者互斥,paragraph 觸發時 msg 不會 active。)
    if (para.active) {
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
          if (level) state = S_GAME; else break;          // 有地圖回遊戲,否則(--encounter)離開
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
      else if (in.key >= '1' && in.key <= '9' && party.size() > 0)
        sheet.open((int)party.size(), in.key - '1');
      else {
        if (in.left  || in.key == 'J') dir = (dir + 3) % 4;   // 左轉
        if (in.right || in.key == 'L') dir = (dir + 1) % 4;   // 右轉
        if (in.up    || in.key == 'I') {                      // 前進
          int nx = px + dx4[dir], ny = py + dy4[dir];
          if (level && level->walkable(nx, ny)) {
            px = nx; py = ny;
            mark_seen_here();   // 對齊 refresh_viewport:踏上新格即標記 seen
          }
        }
        if (in.key == 'K') std::fprintf(stderr, "open door (stub)\n");
        // 事件格(對拍 op_71:tile 值變了才觸發);事件文字 → 開訊息檢視器(分頁捲動)
        if (level) {
          int tv = level->tile(px, py);
          if (tv > 1 && tv != last_event_tile) {
            event_msg = run_event((std::uint8_t)tv); last_event_tile = tv;
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
    else if (state == S_MENU) {
      if (in.back) break;                                // 選單按 Esc = 離開
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
