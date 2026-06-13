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
#include "render/sdl_video.hpp"
#include "resource/level.hpp"
#include "resource/paragraphs.hpp"
#include "i18n/strings.hpp"
#include "game/party.hpp"
#include "game/savegame.hpp"
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

  void open(int n, int start = 0) {
    count = n < 1 ? 0 : n;
    idx = start;
    if (count > 0) { if (idx < 0) idx = 0; if (idx >= count) idx = count - 1; }
    active = count > 0;
  }
  void close() { active = false; }
  void prev() { if (count > 0) idx = (idx - 1 + count) % count; }
  void next() { if (count > 0) idx = (idx + 1) % count; }
  // 數字鍵 1-count 直選;越界忽略。回傳是否命中。
  bool select(int n) {
    if (n >= 1 && n <= count) { idx = n - 1; return true; }
    return false;
  }
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
  int start_pc = 20, max_frames = -1, press = 0, map_area = -1;
  int at_x = -1, at_y = -1;       // --at x y:把玩家放到指定格(headless 驗證事件文字)
  int msg_page = 0;               // --msg-page N:訊息檢視器先翻到第 N 頁再 dump(headless 驗證分頁)
  int read_para = -1;             // --read-para N:直接開段落 N 進訊息檢視(headless 驗證長段落)
  int char_sheet = -1;            // --char-sheet N:直接開第 N 名(1-based)角色屬性表(headless 驗證)
  std::string dump, sprite_name, scene_name;
  std::string save_path = "save/slot0.sav";  // 存/讀檔預設路徑(cwd 可寫處;見 .gitignore)
  std::string load_path;        // --load <path>:啟動即讀檔還原(進遊戲)
  bool selftest_save = false;   // --selftest-save:headless round-trip 自測(印 PASS/FAIL)
  bool viewport_mode = false;   // --viewport:顯示原版第一人稱 viewport 靜態框架
  bool fp_mode = false;         // --fp:S_GAME 用第一人稱 viewport(取代俯視彩格)
  for (int i = 1; i < argc; ++i) {
    auto eq = [&](const char* f) { return !std::strcmp(argv[i], f); };
    if (eq("--bundle") && i + 1 < argc) bundle = argv[++i];
    else if (eq("--font") && i + 1 < argc) font_raw = argv[++i];
    else if (eq("--font-ttf") && i + 1 < argc) font_ttf = argv[++i];   // 文字層 host TTF
    else if (eq("--scale") && i + 1 < argc) scale = std::atoi(argv[++i]);  // 視窗整數倍率
    else if (eq("--menu") && i + 1 < argc) menu_tsv = argv[++i];
    else if (eq("--locale") && i + 1 < argc) locale = argv[++i];   // 切語系(zh-TW / ja / …)
    else if (eq("--pc") && i + 1 < argc) start_pc = std::atoi(argv[++i]);
    else if (eq("--frames") && i + 1 < argc) max_frames = std::atoi(argv[++i]);
    else if (eq("--dump") && i + 1 < argc) dump = argv[++i];
    else if (eq("--sprite") && i + 1 < argc) sprite_name = argv[++i];
    else if (eq("--scene") && i + 1 < argc) scene_name = argv[++i];
    else if (eq("--map") && i + 1 < argc) map_area = std::atoi(argv[++i]);   // 直接進某區地圖
    else if (eq("--at") && i + 2 < argc) { at_x = std::atoi(argv[++i]); at_y = std::atoi(argv[++i]); }  // 玩家落點(測試)
    else if (eq("--press") && i + 1 < argc) press = std::toupper((unsigned char)argv[++i][0]);  // 模擬按鍵(測試)
    else if (eq("--msg-page") && i + 1 < argc) msg_page = std::atoi(argv[++i]);   // 訊息檢視先翻到第 N 頁再 dump
    else if (eq("--read-para") && i + 1 < argc) read_para = std::atoi(argv[++i]); // 直接開段落 N 進訊息檢視
    else if (eq("--char-sheet") && i + 1 < argc) char_sheet = std::atoi(argv[++i]); // 直接開第 N 名角色屬性表
    else if (eq("--load") && i + 1 < argc) load_path = argv[++i];        // 啟動讀檔還原
    else if (eq("--save-path") && i + 1 < argc) save_path = argv[++i];   // 覆寫存/讀檔路徑
    else if (eq("--selftest-save")) selftest_save = true;               // round-trip 自測
    else if (eq("--viewport")) viewport_mode = true;   // 顯示原版 viewport 靜態框架
    else if (eq("--fp")) fp_mode = true;               // 第一人稱 viewport(透視牆面)
  }
  if (scale < 1) scale = 1;

  auto font = render::Font8x8::load_table(font_raw);
  if (!font) { std::fprintf(stderr, "font load failed: %s\n", font_raw.c_str()); return 1; }
  const bool scene_mode = !scene_name.empty();
  const bool sprite_mode = !sprite_name.empty();
  const bool menu_mode = !scene_mode && !sprite_mode && !viewport_mode && map_area < 0;
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
  enum { S_MENU, S_BRANCH, S_GAME } state = S_MENU;
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
  int level_res = -1;             // 當前關卡資源 index(= area + 0x46;= word_3AE8)
  int current_area = -1;          // 當前所在區域(存檔用;= level_res - 0x46)
  // 持久 VM 遊戲狀態(對拍 opendw game_state.unknown[256]):跨事件保留,存檔/讀檔的核心欄位。
  // run_event 跑事件腳本時以此為初值並回寫,使旗標(門/開關/劇情)能持久累積。
  std::array<std::uint8_t, 256> game_state{};
  std::string event_msg;          // 踩到事件格時跑 script emit 的文字(原文,F4 重排用)
  int last_event_tile = -1;       // 對拍 op_71:tile 值變了才觸發
  MsgViewer msg;                  // 訊息/段落檢視器(分頁捲動;active 時暫停移動)
  CharSheet sheet;                // 角色屬性表檢視子狀態(V / 數字 1-4 進;active 時暫停移動)

  // 事件腳本跨資源 call(op_58)的資源提供者:從 bundle 載(自包含,不需 DATA1)。
  // tag = DATA1 section;BundleProvider 讀 assets/bundle/scripts/<tag>.bin(解壓後)。
  // 事件 script 經 op_58 載入的 tag 聯集已預先抽進 bundle(見 manifest event_script_tags)。
  res::BundleProvider event_provider(bundle);

  // 段落書 book 已於 load_locale 載入(隨 F4 切語系重載)。

  // 踩到特殊格 → 跑該關事件腳本,回傳 emit 的文字(對拍 opendw op_71/run_level_script)。
  // 對齊 level_events:設 script_res/data_res = level_res,並掛 resource_provider 讓 op_58 能跑。
  auto run_event = [&](std::uint8_t tv) -> std::string {
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
    return true;
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
    return s;
  };
  // 把 SaveState 套回目前遊戲(重載該 area + 還原位置/朝向/game_state/party)。回傳是否成功。
  auto apply_state = [&](const game::SaveState& s) {
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

  if (map_area >= 0) {
    if (!enter_map(map_area)) return 1;
    state = S_GAME;
    // --read-para N:直接取段落 N 的在地化原文當訊息(headless 驗證長段落分頁)。
    if (read_para >= 0 && book) {
      auto para = book->text(read_para);
      if (para) { event_msg = *para; last_event_tile = -1;
        std::fprintf(stderr, "read-para %d: %zu bytes\n", read_para, event_msg.size()); }
      else std::fprintf(stderr, "read-para %d: not found\n", read_para);
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

  // 第一人稱 viewport 資源(--fp 或選單 B 進遊戲時用):元件 bundle + 靜態框架模板。
  render::ComponentStore comps(bundle + "/components");
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
        if (opts[i].hot == 'B') { enter_map(1); state = S_GAME; }  // 開始新遊戲→波卡城
        else { state = S_BRANCH; branch_label = opts[i].label; branch_label_en = opts[i].en; }
      }
  }

  // ── 雙層渲染:像素層(framebuffer)由各 draw_* 建;文字層(CJK/ASCII)丟給 SdlVideo::text()。
  //    headless 條件:有 --dump 且未設 DISPLAY → 用 dummy driver 合成高解析畫面。──
  const bool headless = !dump.empty() && std::getenv("DISPLAY") == nullptr;
  render::SdlVideo vid;
  if (!vid.open(scale, "OpenDW Remake — 火龍之戰", font_ttf, headless)) {
    std::fprintf(stderr, "SDL open failed\n"); return 1;
  }
  render::TextLayer& tl = vid.text();
  // 原生字級(視窗 px)隨 scale 等比(基準為 scale=3):標題大字、CJK 內文、ASCII 提示。
  const int PX_TITLE = 48 * scale / 3;   // 標題「火龍之戰」
  const int PX_BODY  = 24 * scale / 3;   // CJK 內文(選單/事件/段落)
  const int PX_UI    = 16 * scale / 3;   // ASCII UI(關卡名/控制提示)

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
  const int MB_LINE_H = PX_BODY / scale + 3;         // 行距(虛擬座標;CJK 字高/scale + 間距)
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

  // ── 角色屬性表框幾何(320×200 虛擬座標)──
  // 落在畫面中央偏左(避開右側隊伍面板區),框較高以容納所有屬性列。
  const int CS_X = 8, CS_Y = 20, CS_W = 200, CS_H = render::kH - CS_Y - 8;
  const int CS_PAD = 6;
  const int CS_LINE_H = PX_BODY / scale + 3;   // 屬性列行距(虛擬座標)
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
    tl.add(CS_VAL_X, iy, "1-4  Up/Down  Esc", 8, PX_UI);
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
    party.draw_status_panel(fb, tl, PX_UI);
    // 文字層:關卡名 + 控制提示 + 事件文字(自動換行)。
    tl.add(8, 2, level->name, 14, PX_UI);
    add_lang_badge();
    int hint_y = oy + H * cs + 6;
    if (!msg.active && !sheet.active)                    // 訊息檢視 / 屬性表期間隱藏控制提示(避免穿透框)
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
    party.draw_status_panel(fb, tl, PX_UI);
    // 文字層:關卡名 + 控制提示 + viewport 下方事件/段落文字(自動換行)。
    tl.add(8, 2, level->name, 14, PX_UI);
    add_lang_badge();
    if (!msg.active && !sheet.active)                    // 訊息檢視 / 屬性表期間隱藏控制提示(避免穿透框)
      tl.add(8, 150, "I:fwd  J/L:turn  K:door  V:stats  S:save  Esc:back", 7, PX_UI);
    // 事件/段落文字改走訊息檢視器(draw_msg_overlay,疊在最上層;見 render_now)。
  };
  // sprite/scene/viewport 靜態檢視:像素層已於前面建好;文字層每幀補上標籤。
  auto draw_static_text = [&]() {
    if (sprite_mode) tl.add(8, 4, sprite_name, 15, PX_UI);
  };
  auto render_now = [&]() {
    tl.clear();                                      // 每幀重建文字層
    if (state == S_GAME) {
      if (fp_mode) draw_game_fp(); else draw_game();
      if (msg.active) draw_msg_overlay();            // 訊息檢視器疊在地圖/viewport 上層
      if (sheet.active) draw_char_sheet();           // 角色屬性表疊在最上層
      return;
    }
    if (!menu_mode) { draw_static_text(); return; }  // sprite/scene/viewport:像素層靜態,只補文字
    if (state == S_MENU) draw_menu();
    else draw_branch();
  };
  // headless / 啟動即有訊息(--at 事件格 或 --read-para):進訊息檢視器。
  // --msg-page N:在 dump 前先翻到第 N 頁(驗證分頁:第 1 頁含 ▼、第 2 頁續顯示)。
  if (state == S_GAME && !event_msg.empty()) {
    open_msg(event_msg);
    for (int p = 0; p < msg_page && msg.active; ++p) msg.advance();
    std::fprintf(stderr, "msg viewer: %d lines, %d/page → %d pages; showing page %d\n",
                 (int)msg.lines.size(), msg.lines_per_page, msg.page_count(), msg.page + 1);
  }
  // --char-sheet N:headless 直接開第 N 名(1-based)角色屬性表(驗證屬性值 / 在地化 / 版面)。
  if (state == S_GAME && char_sheet >= 1 && party.size() > 0) {
    sheet.open((int)party.size(), char_sheet - 1);
    std::fprintf(stderr, "char sheet: showing character %d/%zu (\"%s\")\n",
                 sheet.idx + 1, party.size(), party.at((std::size_t)sheet.idx).name.c_str());
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
    if (in.quit) break;
    // F4:即時循環切換語系 → 重載字串/段落書 → 重譯所有 widget。
    // 因每幀重繪(render_now),畫面立即變為新語言;事件文字重跑該關腳本重譯。
    if (in.cycle_lang && !locales.empty()) {
      locale_idx = (locale_idx + 1) % (int)locales.size();
      load_locale(locales[locale_idx]);
      relocalize();                                      // 選單/branch 重譯
      if (state == S_GAME && level && last_event_tile > 1) {
        event_msg = run_event((std::uint8_t)last_event_tile);  // 事件文字換語言
        if (msg.active) msg.reflow(tl.wrap(event_msg, MB_TEXT_W, PX_BODY));  // 當前頁就地重排
      }
      continue;                                          // 本幀不再處理其他輸入
    }
    // 角色屬性表啟用時:接管輸入(切角色/關閉),暫停移動。
    //   ↑↓ 或數字 1-4 切角色;Esc 關閉。F4(語系)已於上方處理。
    if (sheet.active) {
      if (in.back) { sheet.close(); }                    // Esc:關閉回遊戲
      else if (in.up) sheet.prev();
      else if (in.down) sheet.next();
      else if (in.key >= '1' && in.key <= '9') sheet.select(in.key - '0');
      else if (in.key == 'V') sheet.close();             // V 再按一次 → 關閉
      if (max_frames >= 0 && ++frames >= max_frames) break;
      continue;                                          // 屬性表期間不處理移動
    }
    // 訊息檢視器啟用時:接管輸入(翻頁/關閉),暫停移動,翻頁鍵不誤觸移動。
    if (msg.active) {
      if (in.back) { msg.close(); }                      // Esc:直接關閉回遊戲
      else if (in.select || in.down || in.up || in.key == 'I')
        msg.advance();                                   // Space/Enter/↓/I:下一頁;末頁→關閉
      if (max_frames >= 0 && ++frames >= max_frames) break;
      continue;                                          // 訊息檢視期間不處理移動
    }
    if (state == S_GAME) {                               // F:真實地圖移動(對齊說明書)
      if (in.back) { if (menu_mode) state = S_MENU; else break; }   // Esc:選單進入→返回;--map→離開
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
          if (level && level->walkable(nx, ny)) { px = nx; py = ny; }
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
            if (!event_msg.empty()) open_msg(event_msg);   // 進訊息檢視(暫停移動)
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
          if (opts[trig].hot == 'B') { enter_map(1); state = S_GAME; }  // 開始新遊戲→波卡城(area 1)
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
