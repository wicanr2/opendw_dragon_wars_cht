// main — OpenDW Remake app(R2 SDL2 開窗)。
//
// 已完成小段:
//   B  bundle bytecode → VM op_78 → i18n 繁中 → CJK+8×8 渲染 → SDL 顯示(在地化選單)
//   A  --sprite NAME:從 bundle 載 .spr 美術顯示在視窗(不碰 DATA1)
//   C  互動骨架:poll() → Input 事件
//   D  快捷字母選單 + 狀態分支,操作與說明書一致(見 docs/CONTROLS.md):
//      B=開始新遊戲、C=繼續舊遊戲;↑↓/Enter 為輔助;Esc 返回 / Q 離開。
//
// 字型全部來自 assets(自包含:dw8x8.bin + cjk24.atlas),執行期不依賴原始遊戲檔。
//
// 用法:opendw_remake [--bundle DIR] [--font RAW] [--atlas ATLAS] [--menu TSV]
//                     [--pc N] [--sprite NAME] [--frames N] [--dump PPM] [--press CH]
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <cctype>
#include <optional>
#include <string>
#include <vector>
#include "resource/provider.hpp"
#include "vm/interpreter.hpp"
#include "render/font.hpp"
#include "render/cjk_font.hpp"
#include "render/framebuffer.hpp"
#include "render/sprite.hpp"
#include "render/picture.hpp"
#include "render/viewport.hpp"
#include "render/viewport_compose.hpp"
#include "render/sdl_video.hpp"
#include "resource/level.hpp"
#include "resource/paragraphs.hpp"
#include "i18n/strings.hpp"
using namespace dw;

static std::vector<std::string> lines_of(const std::string& s) {
  std::vector<std::string> o; std::string c;
  for (char ch : s) { if (ch == '\r' || ch == '\n') { o.push_back(c); c.clear(); } else c.push_back(ch); }
  o.push_back(c); return o;
}

static void draw_mixed(render::Framebuffer& fb, const render::Font8x8& font,
                       const render::CjkFont& cjk, int x, int y, const std::string& z,
                       std::uint8_t col, std::uint8_t bg) {
  const char* p = z.c_str();
  while (*p) {
    std::uint32_t cp = render::utf8_next(p);
    if (cp < 0x80) { font.draw_char(fb, x, y + 8, (std::uint8_t)cp, col, bg); x += 8; }
    else { cjk.draw(fb, x, y, cp, col); x += 24; }
  }
}

struct Opt { char hot; std::string label; };   // 快捷字母 + 在地化文字

// tile 型(word_11C8)→ framebuffer 顏色:0=void/牆、1=地面、其他=特殊/事件格。
static std::uint8_t tile_color(std::uint8_t t) {
  if (t == 0) return 8;            // 牆/void = 灰
  if (t == 1) return 1;            // 地面 = 深藍
  return (std::uint8_t)(t & 0x0F); // 特殊格 = 以 tile 值當調色盤索引(各類各色)
}

int main(int argc, char** argv) {
  std::string bundle = "assets/bundle";
  std::string font_raw = "assets/fonts/dw8x8.bin";
  std::string atlas = "assets/fonts/cjk24.atlas";
  std::string locale = "zh-TW";   // 多國語系:i18n 取 assets/i18n/<locale>/;未來 ja 等
  std::string menu_tsv;           // 空 = 由 locale 推導
  int start_pc = 20, max_frames = -1, press = 0, map_area = -1;
  int at_x = -1, at_y = -1;       // --at x y:把玩家放到指定格(headless 驗證事件文字)
  std::string dump, sprite_name, scene_name;
  bool viewport_mode = false;   // --viewport:顯示原版第一人稱 viewport 靜態框架
  bool fp_mode = false;         // --fp:S_GAME 用第一人稱 viewport(取代俯視彩格)
  for (int i = 1; i < argc; ++i) {
    auto eq = [&](const char* f) { return !std::strcmp(argv[i], f); };
    if (eq("--bundle") && i + 1 < argc) bundle = argv[++i];
    else if (eq("--font") && i + 1 < argc) font_raw = argv[++i];
    else if (eq("--atlas") && i + 1 < argc) atlas = argv[++i];
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
    else if (eq("--viewport")) viewport_mode = true;   // 顯示原版 viewport 靜態框架
    else if (eq("--fp")) fp_mode = true;               // 第一人稱 viewport(透視牆面)
  }

  auto font = render::Font8x8::load_table(font_raw);
  if (!font) { std::fprintf(stderr, "font load failed: %s\n", font_raw.c_str()); return 1; }
  const bool scene_mode = !scene_name.empty();
  const bool sprite_mode = !sprite_name.empty();
  const bool menu_mode = !scene_mode && !sprite_mode && !viewport_mode && map_area < 0;
  render::Framebuffer fb;

  // 多國語系:i18n 字串表由 locale 推導(可 --locale ja 切換);CJK atlas 可隨 locale 替換。
  if (menu_tsv.empty()) menu_tsv = "assets/i18n/" + locale + "/menu.tsv";
  auto cjk = render::CjkFont::load(atlas);          // 全模式共用(選單/地圖/事件文字)
  auto tr = i18n::Strings::load(menu_tsv);          // tr() 查不到則回退英文 → 未翻譯也能顯示
  if (!cjk || !tr) { std::fprintf(stderr, "atlas/i18n load failed (locale=%s)\n", locale.c_str()); return 1; }
  // 併入事件文字譯表(events.tsv):踩到事件格時的在地化來源(同 locale)。可缺檔。
  std::string events_tsv = "assets/i18n/" + locale + "/events.tsv";
  if (tr->merge(events_tsv))
    std::fprintf(stderr, "i18n: merged %s (total %zu)\n", events_tsv.c_str(), tr->size());

  std::string header;
  std::vector<Opt> opts;
  int sel = 0;
  enum { S_MENU, S_BRANCH, S_GAME } state = S_MENU;
  std::string branch_label;
  int px = 0, py = 0, dir = 1;     // 玩家位置/朝向(0=N,1=E,2=S,3=W)
  const int dx4[4] = {0, 1, 0, -1}, dy4[4] = {-1, 0, 1, 0};
  const char* dirch = "^>v<";
  std::optional<res::Level> level;
  int level_res = -1;             // 當前關卡資源 index(= area + 0x46;= word_3AE8)
  std::string event_msg;          // 踩到事件格時跑 script emit 的文字
  int last_event_tile = -1;       // 對拍 op_71:tile 值變了才觸發

  // 事件腳本跨資源 call(op_58)的資源提供者:從 bundle 載(自包含,不需 DATA1)。
  // tag = DATA1 section;BundleProvider 讀 assets/bundle/scripts/<tag>.bin(解壓後)。
  // 事件 script 經 op_58 載入的 tag 聯集已預先抽進 bundle(見 manifest event_script_tags)。
  res::BundleProvider event_provider(bundle);

  // Read paragraph 段落書(防拷手冊繁中全文):隨 locale 載 bundle/paragraphs/<locale>/。
  // 缺檔則 book 為空 → run_event 回退顯示「Read paragraph N」(對齊原版)。
  auto book = res::ParagraphBook::load(bundle + "/paragraphs", locale);
  if (book) std::fprintf(stderr, "paragraphs: loaded %zu (locale=%s)\n", book->size(), locale.c_str());
  else std::fprintf(stderr, "paragraphs: none for locale=%s (fallback to 'Read paragraph N')\n", locale.c_str());

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
    // op_58 / 子 script / op_0F 跨資源讀:依 tag 從 bundle 載(自包含)。
    // Read paragraph 流程的段落號 N 是 op_0F 從「關卡資源本身」(index=level_res)
    // 的回返 offset 讀出的;關卡資源 = 這份 .lvl,bundle/scripts 沒有它 → 直接回傳
    // level bytes,讓 op_0F 取得正確 N(對齊 DATA1Provider 以 index 解析同一份資源)。
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
      std::string t = tr->tr(s);
      // 偵測「Read paragraph 」前綴(原文判定,翻譯前):此後緊接的數字即段落號。
      if (s.rfind("Read paragraph", 0) == 0) read_para_pending = true;
      if (!out.empty()) out += ' ';
      out += t;
    });
    ip.run();
    return out;
  };

  // 進入某區地圖:載入真實關卡 .lvl + 找第一個可走格當起點
  auto enter_map = [&](int area) {
    level = res::Level::load_file(bundle + "/maps/" + std::to_string(area) + ".lvl");
    if (!level) { std::fprintf(stderr, "level load failed: area %d\n", area); return false; }
    level_res = area + 0x46;       // 關卡資源 index(對拍 level_events:word_3AE8)
    px = py = 0; dir = 1;
    for (int y = 0; y < level->h && py == 0 && px == 0; ++y)
      for (int x = 0; x < level->w; ++x)
        if (level->tile(x, y) == 1) { px = x; py = y; y = level->h; break; }
    std::fprintf(stderr, "enter map area %d: \"%s\" %dx%d start=(%d,%d)\n",
                 area, level->name.c_str(), level->w, level->h, px, py);
    return true;
  };
  if (map_area >= 0) {
    if (!enter_map(map_area)) return 1;
    state = S_GAME;
    // --at:把玩家放到指定格;若是事件格(tile>1)立刻跑事件腳本(headless 驗證)。
    if (at_x >= 0 && at_y >= 0 && level && level->in_bounds(at_x, at_y)) {
      px = at_x; py = at_y;
      int tv = level->tile(px, py);
      if (tv > 1) {
        event_msg = run_event((std::uint8_t)tv); last_event_tile = tv;
        std::fprintf(stderr, "at (%d,%d) tile=0x%02X event=\"%s\"\n", px, py, tv, event_msg.c_str());
      }
    }
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
    font->draw_string(fb, 8, 8, sprite_name.c_str(), 15, 0);
    std::fprintf(stderr, "sprite %s %dx%d (bundle, no DATA1)\n", sprite_name.c_str(), sp->w, sp->h);
  } else if (menu_mode) {
    // ── B:VM 在地化選單 → D:快捷字母選項 ──(cjk/tr 已於頂層依 locale 載入)
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
    if (en.size() > 1) { header = tr->tr(en[0]); first_opt = 1; }   // 第一行為提示
    for (std::size_t i = first_opt; i < en.size(); ++i) {
      char hot = 0;
      for (char ch : en[i]) if (std::isalpha((unsigned char)ch)) { hot = std::toupper((unsigned char)ch); break; }
      opts.push_back({hot, tr->tr(en[i])});
    }
    std::fprintf(stderr, "menu: header=\"%s\" options=%zu (hotkeys:", header.c_str(), opts.size());
    for (auto& o : opts) std::fprintf(stderr, " %c", o.hot ? o.hot : '?');
    std::fprintf(stderr, ")\n");

    // --press 模擬:直接觸發對應快捷字母(headless 驗證分支)
    if (press) for (std::size_t i = 0; i < opts.size(); ++i)
      if (opts[i].hot == press) {
        sel = (int)i;
        if (opts[i].hot == 'B') { enter_map(1); state = S_GAME; }  // 開始新遊戲→波卡城
        else { state = S_BRANCH; branch_label = opts[i].label; }
      }
  }

  auto draw_menu = [&]() {
    fb.clear(1);
    { int x = 8; for (std::uint32_t cp : {U'火', U'龍', U'之', U'戰'}) { cjk->draw(fb, x, 8, cp, 14); x += 24; } }
    int y = 48;
    if (!header.empty()) { draw_mixed(fb, *font, *cjk, 16, y, header, 7, 1); y += 28; }
    for (std::size_t i = 0; i < opts.size(); ++i) {
      bool cur = (int)i == sel;
      std::uint8_t col = cur ? 14 : 15;
      int x = 16;
      if (cur) { font->draw_char(fb, x, y + 8, (std::uint8_t)'>', 14, 1); }
      x = 36;
      if (opts[i].hot) {                       // 快捷字母,如 "B)"
        font->draw_char(fb, x, y + 8, (std::uint8_t)opts[i].hot, 11, 1); x += 8;
        font->draw_char(fb, x, y + 8, (std::uint8_t)')', col, 1); x += 12;
      }
      draw_mixed(fb, *font, *cjk, x, y, opts[i].label, col, 1);
      y += 28;
    }
  };
  auto draw_branch = [&]() {
    fb.clear(1);
    { int x = 8; for (std::uint32_t cp : {U'火', U'龍', U'之', U'戰'}) { cjk->draw(fb, x, 8, cp, 14); x += 24; } }
    draw_mixed(fb, *font, *cjk, 16, 70, branch_label, 14, 1);
    // 提示用 ASCII(CJK atlas 僅含遊戲在地化字,避免缺字)
    font->draw_string(fb, 16, 120, "(game screen - to be implemented)", 7, 1);
    font->draw_string(fb, 16, 156, "Esc: back   Q: quit", 8, 1);
  };
  auto draw_game = [&]() {
    // F:真實關卡俯視圖(從 .lvl 解出的 tile 格)+ 玩家朝向。移動鍵見 docs/CONTROLS.md。
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
    font->draw_char(fb, ox + px * cs, oy + py * cs, (std::uint8_t)dirch[dir], 15, 0);  // 玩家
    // 標題(關卡名為 ASCII,可用 8×8 字)+ 控制提示
    font->draw_string(fb, 8, 4, level->name.c_str(), 14, 0);
    int hint_y = oy + H * cs + 6;
    font->draw_string(fb, 8, hint_y, "I:fwd  J/L:turn  K:door  Esc:back", 7, 0);
    // 踩到事件格 → 顯示事件文字(已於 run_event 在地化;英文 8×8、中文 12×12 半尺寸,自動換行)
    if (!event_msg.empty()) {
      const std::string& z = event_msg;
      int my = hint_y + 12, mx0 = 4, mx = mx0, maxx = render::kW - 4;
      const char* p = z.c_str();
      // 段落可能很長(Read paragraph 全文):超出畫面底部即停(fb.put 本身已 clip)。
      while (*p && my < render::kH) {
        std::uint32_t cp = render::utf8_next(p);
        if (cp == ' ' && mx > maxx - 24) { mx = mx0; my += 13; continue; }   // 詞界換行
        if (cp < 0x80) { font->draw_char(fb, mx, my + 2, (std::uint8_t)cp, 15, 0); mx += 8; }
        else { cjk->draw_half(fb, mx, my, cp, 15); mx += 12; }
        if (mx > maxx) { mx = mx0; my += 13; }
      }
    }
  };
  // F+:第一人稱 viewport(透視牆面)。port 自 opendw refresh_viewport →
  //   update_viewport(靜態框架)→ ui_update_viewport(上螢幕)。對拍 verify_fp 4/4。
  auto draw_game_fp = [&]() {
    fb.clear(0);
    if (!level) return;
    render::ViewportDecoder dec;
    // 牆面/地面/天空 sprite blit 進 viewport_memory(已對拍 golden 10880B)。
    render::render_first_person(*level, px, py, dir, dec, comps);
    // 靜態透視框架疊上(vp0..vp3,= 原版 update_viewport)。
    if (vpt_ok)
      dec.compose_frame(vpt[0].data(), vpt[1].data(), vpt[2].data(), vpt[3].data());
    dec.to_framebuffer(fb);   // 160×136 @ (16,8)
    // UI:關卡名 + 控制提示
    font->draw_string(fb, 8, 4, level->name.c_str(), 14, 0);
    font->draw_string(fb, 8, 150, "I:fwd  J/L:turn  K:door  Esc:back", 7, 0);
    // 事件文字:viewport 下方訊息區(已於 run_event 在地化;可切語系)。
    // 320×200 下空間有限:中文用半尺寸 12×12(draw_half,每行約 26 字)、英文 8×8,
    // 自動換行,讓整句事件文字落在 viewport 下方可見區。
    if (!event_msg.empty()) {
      const std::string& z = event_msg;
      int my = 158, mx0 = 4, mx = mx0, maxx = render::kW - 4;
      const char* p = z.c_str();
      // 段落可能很長(Read paragraph 全文):超出畫面底部即停(fb.put 本身已 clip)。
      while (*p && my < render::kH) {
        std::uint32_t cp = render::utf8_next(p);
        if (cp == ' ' && mx > maxx - 24) { mx = mx0; my += 13; continue; }   // 詞界換行
        if (cp < 0x80) { font->draw_char(fb, mx, my + 2, (std::uint8_t)cp, 15, 0); mx += 8; }
        else { cjk->draw_half(fb, mx, my, cp, 15); mx += 12; }
        if (mx > maxx) { mx = mx0; my += 13; }
      }
    }
  };
  auto render_now = [&]() {
    if (state == S_GAME) { if (fp_mode) draw_game_fp(); else draw_game(); return; }   // 地圖(--map 或選單 B 進入)
    if (!menu_mode) return;                          // sprite/scene:靜態,已畫
    if (state == S_MENU) draw_menu();
    else draw_branch();
  };
  render_now();

  if (!dump.empty()) {
    std::FILE* d = std::fopen(dump.c_str(), "wb");
    if (d) { fb.write_ppm(d); std::fclose(d); std::fprintf(stderr, "dumped frame to %s\n", dump.c_str()); }
  }

  render::SdlVideo vid;
  if (!vid.open(3)) { std::fprintf(stderr, "SDL open failed\n"); return 1; }
  int frames = 0;
  for (;;) {
    render_now();
    vid.present(fb);
    render::Input in = vid.poll();
    if (in.quit) break;
    if (state == S_GAME) {                               // F:真實地圖移動(對齊說明書)
      if (in.back) { if (menu_mode) state = S_MENU; else break; }   // Esc:選單進入→返回;--map→離開
      else {
        if (in.left  || in.key == 'J') dir = (dir + 3) % 4;   // 左轉
        if (in.right || in.key == 'L') dir = (dir + 1) % 4;   // 右轉
        if (in.up    || in.key == 'I') {                      // 前進
          int nx = px + dx4[dir], ny = py + dy4[dir];
          if (level && level->walkable(nx, ny)) { px = nx; py = ny; }
        }
        if (in.key == 'K') std::fprintf(stderr, "open door (stub)\n");
        // 事件格(對拍 op_71:tile 值變了才觸發);事件文字走 i18n,可切語系
        if (level) {
          int tv = level->tile(px, py);
          if (tv > 1 && tv != last_event_tile) { event_msg = run_event((std::uint8_t)tv); last_event_tile = tv; }
          else if (tv <= 1) { last_event_tile = -1; event_msg.clear(); }
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
          else { state = S_BRANCH; branch_label = opts[trig].label; }
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
