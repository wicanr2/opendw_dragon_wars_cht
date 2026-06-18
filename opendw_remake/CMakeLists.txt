cmake_minimum_required(VERSION 3.16)
project(opendw_remake LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT MSVC)
  add_compile_options(-Wall -Wextra)
endif()

# --- resource 模組(R0:archive + text_codec + decompress,無外部相依)---
add_library(dwr_resource STATIC
  src/resource/text_codec.cpp
  src/resource/archive.cpp
  src/resource/decompress.cpp
  src/resource/provider.cpp
  src/resource/level.cpp
  src/resource/paragraphs.cpp
)
target_include_directories(dwr_resource PUBLIC src)

# --- R0 驗證工具(對拍 opendw)---
add_executable(verify_r0 tools/verify/verify_r0.cpp)
target_link_libraries(verify_r0 PRIVATE dwr_resource)

add_executable(verify_decompress tools/verify/verify_decompress.cpp)
target_link_libraries(verify_decompress PRIVATE dwr_resource)

# --- vm 模組(R1:script 虛擬 CPU)---
add_library(dwr_vm STATIC src/vm/interpreter.cpp)
target_include_directories(dwr_vm PUBLIC src)
target_link_libraries(dwr_vm PUBLIC dwr_resource)

enable_testing()
add_executable(vm_selftest tests/vm_selftest.cpp)
target_link_libraries(vm_selftest PRIVATE dwr_vm)
add_test(NAME vm_selftest COMMAND vm_selftest)

# 差異測試:remake VM trace,與 opendw oracle 對拍(見 tools_build/diff_trace.sh)。
add_executable(trace_remake tools/verify/trace_remake.cpp)
target_link_libraries(trace_remake PRIVATE dwr_vm)

# --- render 模組(R2:framebuffer + 全螢幕圖解碼 + 8×8 字)---
add_library(dwr_render STATIC src/render/picture.cpp src/render/font.cpp src/render/cjk_font.cpp src/render/sprite.cpp src/render/viewport.cpp src/render/viewport_compose.cpp src/render/minimap.cpp src/render/ui_pieces.cpp)
target_link_libraries(dwr_render PUBLIC dwr_resource)
add_library(dwr_i18n STATIC src/i18n/strings.cpp src/i18n/string_table.cpp)
target_include_directories(dwr_i18n PUBLIC src)
target_include_directories(dwr_render PUBLIC src)

# --- game 模組(隊伍角色資料 + 右側狀態面板渲染 + 戰鬥結算)---
add_library(dwr_game STATIC src/game/party.cpp src/game/equipment.cpp src/game/combat.cpp src/game/combat_loop.cpp src/game/spells.cpp src/game/seen_map.cpp src/game/terrain_state.cpp src/game/real_terrain.cpp src/game/chargen.cpp src/game/progression.cpp src/game/shop.cpp src/game/recruit.cpp)
target_include_directories(dwr_game PUBLIC src)
target_link_libraries(dwr_game PUBLIC dwr_render dwr_vm)

# --- save 模組(存檔/讀檔;自包含,無 render/SDL 相依,可獨立測試)---
add_library(dwr_save STATIC src/game/savegame.cpp)
target_include_directories(dwr_save PUBLIC src)

# 存檔 round-trip 自測(不需 SDL;印 PASS/FAIL)。
add_executable(verify_save tools/verify/verify_save.cpp)
target_link_libraries(verify_save PRIVATE dwr_save)
add_test(NAME verify_save COMMAND verify_save)

# 探索互動(門/密門/陷阱/地形法術)機制自測(不需 SDL;印 PASS/FAIL)。
add_executable(verify_terrain tools/verify/verify_terrain.cpp)
target_link_libraries(verify_terrain PRIVATE dwr_game)
add_test(NAME verify_terrain COMMAND verify_terrain ${CMAKE_SOURCE_DIR}/assets/bundle)

# golden 測試:remake 渲染 title,與 golden 對拍(見 tools_build/render_golden.sh)。
add_executable(render_title tools/verify/render_title.cpp)
target_link_libraries(render_title PRIVATE dwr_render dwr_resource)

# 8×8 字渲染樣本(視覺驗證)。
add_executable(render_text tools/verify/render_text.cpp)
target_link_libraries(render_text PRIVATE dwr_render)

# CJK 混排 demo(8×8 ASCII + 24×24 中文)。
add_executable(render_cjk_demo tools/verify/render_cjk_demo.cpp)
target_link_libraries(render_cjk_demo PRIVATE dwr_render)

# --- R2 SDL2 開窗 app(可選:偵測到 SDL2 才建)---
# 雙層渲染:像素層(framebuffer 整數放大)+ 文字層(SDL2_ttf 高解析 CJK)。
# SDL2_ttf 只供 opendw_remake target;其他 verify_*/extract_* 不需 SDL。
find_package(SDL2 QUIET)
if(SDL2_FOUND)
  find_package(PkgConfig QUIET)
  set(DWR_HAVE_TTF FALSE)
  if(PkgConfig_FOUND)
    pkg_check_modules(SDL2_TTF QUIET SDL2_ttf)
    if(SDL2_TTF_FOUND)
      set(DWR_HAVE_TTF TRUE)
    endif()
  endif()
  # --- audio 模組(R7:PC speaker 風格方波音效;需 SDL2 audio)---
  #   對外窄介面 play(SoundId);內部隱藏 SDL2 audio device + 方波合成。
  #   真值層級:func_5060 索引→語意 + dx/bx = opendw 真值;Hz/ms 換算 = remake 設計
  #   (見 src/audio/sound.hpp 檔頭)。
  add_library(dwr_audio STATIC src/audio/sound.cpp)
  target_include_directories(dwr_audio PUBLIC src)
  target_link_libraries(dwr_audio PUBLIC SDL2::SDL2)

  add_executable(opendw_remake
    src/main.cpp
    src/render/sdl_video.cpp
    src/render/text_layer.cpp
    src/game/party_panel.cpp)
  target_link_libraries(opendw_remake PRIVATE dwr_vm dwr_render dwr_i18n dwr_game dwr_save dwr_audio SDL2::SDL2)

  # tools/verify:音效子系統確定性 PASS/FAIL(headless,SDL_AUDIODRIVER=dummy)。
  #   驗:init OK(實體/dummy/靜音皆成功)+ op_90 func_5060 索引→SoundId 對映 +
  #   grounded dx/bx 真值(door/wall/effect_88)+ VM sound sink 派發路徑 + 乾淨退出。
  add_executable(verify_audio tools/verify/verify_audio.cpp)
  target_link_libraries(verify_audio PRIVATE dwr_audio dwr_vm)
  add_test(NAME verify_audio COMMAND verify_audio)
  set_tests_properties(verify_audio PROPERTIES
    ENVIRONMENT "SDL_AUDIODRIVER=dummy;SDL_VIDEODRIVER=dummy")

  if(DWR_HAVE_TTF)
    target_include_directories(opendw_remake PRIVATE ${SDL2_TTF_INCLUDE_DIRS})
    target_link_libraries(opendw_remake PRIVATE ${SDL2_TTF_LIBRARIES})
    message(STATUS "SDL2 + SDL2_ttf found: opendw_remake two-layer renderer enabled")
  else()
    message(FATAL_ERROR "SDL2_ttf not found (required for text layer). Install libsdl2-ttf-dev.")
  endif()

  # ──────────────────────────────────────────────────────────────────────────
  # 安裝 / 打包規則(可攜發佈包:引擎 binary + 自包含 assets;不含原始遊戲檔)
  #
  #   佈局(GNUInstallDirs):
  #     <prefix>/bin/opendw-remake          實體 binary(install RENAME)
  #     <prefix>/bin/opendw-remake.sh        啟動器 wrapper(cd 至 assets 後 exec binary)
  #     <prefix>/share/opendw-remake/assets/ 自包含資產(bundle / fonts / i18n)
  #     <prefix>/share/doc/opendw-remake/    README
  #
  #   玩家自備合法原版遊戲;本專案資產已自包含,執行期不需原始 DRAGON.COM/DATA1/DATA2。
  #   字型不打包(授權):binary 內建 CJK 字型回退搜尋,或以 DWR_FONT 環境變數指定。
  # ──────────────────────────────────────────────────────────────────────────
  include(GNUInstallDirs)

  # 啟動器 wrapper:由模板產生(填入安裝佈局假設)。
  configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/packaging/opendw-remake.sh.in
    ${CMAKE_CURRENT_BINARY_DIR}/opendw-remake.sh
    @ONLY)

  # 實體 binary 以連字號命名安裝(對齊啟動器 wrapper 預期的 bin/opendw-remake)。
  install(PROGRAMS $<TARGET_FILE:opendw_remake>
          DESTINATION ${CMAKE_INSTALL_BINDIR}
          RENAME opendw-remake)
  install(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/opendw-remake.sh
          DESTINATION ${CMAKE_INSTALL_BINDIR})
  # 自包含資產(不含原始遊戲檔;assets/ 內無 DRAGON.COM/DATA1/DATA2,見 .gitignore)。
  install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/assets
          DESTINATION ${CMAKE_INSTALL_DATADIR}/opendw-remake)
  install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/README.md
          DESTINATION ${CMAKE_INSTALL_DOCDIR})

  # CPack:預設產 .tar.gz 可攜包(make package / cpack -G TGZ)。
  set(CPACK_PACKAGE_NAME "opendw-remake")
  set(CPACK_PACKAGE_VENDOR "opendw_dragon_wars_cht")
  set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "火龍之戰 (Dragon Wars, 1989) C++20/SDL2 繁中重製")
  set(CPACK_PACKAGE_VERSION "0.1.0")
  set(CPACK_GENERATOR "TGZ")
  set(CPACK_SOURCE_GENERATOR "TGZ")
  set(CPACK_PACKAGE_FILE_NAME
      "opendw-remake-${CPACK_PACKAGE_VERSION}-${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}")
  include(CPack)
else()
  message(STATUS "SDL2 not found: skip opendw_remake app (only build/test libs)")
endif()

# R3 端到端:DATA1 → text_codec → i18n → 渲染(英/中選單)。
add_executable(render_menu_demo tools/verify/render_menu_demo.cpp)
target_link_libraries(render_menu_demo PRIVATE dwr_resource dwr_render dwr_i18n)

# R6 起步:從 asset bundle 載 sprite 渲染(不依賴 DATA1,見 docs/adr/0001)。
add_executable(render_sprite_bundle tools/verify/render_sprite_bundle.cpp)
target_link_libraries(render_sprite_bundle PRIVATE dwr_render)

# tools/extract:DATA1 → bundle 字串表(offset→english)。
add_executable(extract_strings tools/extract/extract_strings.cpp)
target_link_libraries(extract_strings PRIVATE dwr_resource)

# tools/extract:全螢幕場景圖資源(res N,解壓 32000B)→ bundle .pic。
add_executable(extract_scene tools/extract/extract_scene.cpp)
target_link_libraries(extract_scene PRIVATE dwr_resource)

# 文字脫離 DATA1:從 bundle 字串表取選單渲染(英/中)。
add_executable(render_menu_bundle tools/verify/render_menu_bundle.cpp)
target_link_libraries(render_menu_bundle PRIVATE dwr_render dwr_i18n)

# ResourceProvider 對拍:BundleProvider == Data1Provider(未編輯資源)。
add_executable(verify_provider tools/verify/verify_provider.cpp)
target_link_libraries(verify_provider PRIVATE dwr_resource)

# 完整閉環:bundle bytecode → VM op_78 → i18n → 渲染。
add_executable(render_vm_text tools/verify/render_vm_text.cpp)
target_link_libraries(render_vm_text PRIVATE dwr_vm dwr_render dwr_i18n)

# tools/extract:關卡(level)資源解析(R4 真實地圖)。
add_executable(extract_level tools/extract/extract_level.cpp)
target_link_libraries(extract_level PRIVATE dwr_resource)

# tools/extract:關卡事件腳本(特殊格→script→emit),驗證 tile→段落。
add_executable(level_events tools/extract/level_events.cpp)
target_link_libraries(level_events PRIVATE dwr_vm)

# tools/extract:跑主線 area 事件腳本(BundleProvider 供 op_58),抽 emit 英文鍵
# + 回報 quest gate halt opcode(雙用:字串萃取 + gate 驗證)。
add_executable(mainline_events tools/extract/mainline_events.cpp)
target_link_libraries(mainline_events PRIVATE dwr_vm)

# tools/extract:全 40 關事件腳本 op_58 跨資源 tag 聯集 → bundle/scripts/<tag>.bin。
add_executable(extract_eventscripts tools/extract/extract_eventscripts.cpp)
target_link_libraries(extract_eventscripts PRIVATE dwr_vm)

# tools/extract:怪物記錄(res31)→ bundle/monsters/monsters.bin(自包含)。
add_executable(extract_monsters tools/extract/extract_monsters.cpp)
target_link_libraries(extract_monsters PRIVATE dwr_resource)

# tools/extract:從 DATA1 萃取 byte-grounded 物品樣本 → bundle/items/items.bin(自包含)。
add_executable(extract_items tools/extract/extract_items.cpp)
target_link_libraries(extract_items PRIVATE dwr_resource)

# tools/extract:從 DRAGON.COM com 0x6AE0 萃取 UI chrome ui_pieces → bundle/viewport/ui_pieces.bin。
#   自包含(執行期不需 DRAGON.COM);byte-for-byte 同原版 ui_pieces 資料(對拍 opendw ui_load)。
add_executable(extract_ui_pieces tools/extract/extract_ui_pieces.cpp)

# tools/verify:物品/裝備格式解碼 PASS/FAIL(類型/售價/真實 DATA1 物品對拍 + 13 格物品欄 + AC 聚合)。
# grounded in docs/44 §2(fraterrisus Equipment Format)。
add_executable(verify_equipment tools/verify/verify_equipment.cpp)
target_link_libraries(verify_equipment PRIVATE dwr_game)
add_test(NAME verify_equipment
         COMMAND verify_equipment ${CMAKE_SOURCE_DIR}/assets/bundle)

# tools/verify:戰鬥切片確定性 PASS/FAIL(怪物萃取 vs oracle 名單 + RNG + 結算可重現)。
add_executable(verify_combat tools/verify/verify_combat.cpp)
target_link_libraries(verify_combat PRIVATE dwr_game)
add_test(NAME verify_combat
         COMMAND verify_combat ${CMAKE_SOURCE_DIR}/assets/bundle)

# tools/verify:完整戰鬥迴圈(4 人 vs 怪群、多回合、勝負、XP)確定性 PASS/FAIL。
#   驗:同 seed 兩次跑 → 事件流/HP/XP byte-identical;勝利每員 +80 XP;勝負判定;
#   公式仍走 resolve_attack(原版真值,combat_loop 不重算)。
add_executable(verify_combat_loop tools/verify/verify_combat_loop.cpp)
target_link_libraries(verify_combat_loop PRIVATE dwr_game)
add_test(NAME verify_combat_loop
         COMMAND verify_combat_loop ${CMAKE_SOURCE_DIR}/assets/bundle)

# tools/verify:4 種特殊攻擊 + Dodge(強力一擊 / 卸武裝 / 前進 / 快速戰鬥 / 閃避)。
#   真值層級:命中/傷害/卸武裝/DV 修正係數 = remake 設計 grounded 手冊(opendw 戰鬥結算
#   未實作、res3 戰鬥 bytecode 全閉環受阻,見 combat.hpp/combat_loop.hpp 檔頭);底層單次
#   攻擊仍走 resolve_attack(命中/傷害公式 = bytecode 真值),特殊攻擊只套修正不重寫公式。
#   驗:強力一擊傷害加成、卸武裝降骰、前進 AV bonus、快速=一般、閃避提 DV 降被命中、確定性。
add_executable(verify_combat_special tools/verify/verify_combat_special.cpp)
target_link_libraries(verify_combat_special PRIVATE dwr_game)
add_test(NAME verify_combat_special COMMAND verify_combat_special)

# tools/verify:UI 主題接線(theme wiring)——
#   驗:三主題(dos/amiga/x68000)+ F8 循環 wrap;per-theme palette(amiga 自帶盤);
#   誠實標示(amiga 完整 / x68000 partial);Amiga title.pic planar 解碼 + 檔頭 palette(金龍金)。
add_executable(verify_theme tools/verify/verify_theme.cpp)
target_link_libraries(verify_theme PRIVATE dwr_render)
add_test(NAME verify_theme
         COMMAND verify_theme ${CMAKE_SOURCE_DIR}/assets/bundle)

# tools/verify:可通關收官 —— 終戰 Namtar 可勝 + 確定性(combat_loop)。
#   驗:預設隊伍(第 0 名持受祝福自由之劍)vs Namtar Boss,多 seed 皆 Victory + 隊伍存活;
#   同 seed 兩次執行一致(確定性)。Boss 屬性/祝福加成 = remake 平衡設計(誠實標示,
#   見 combat.hpp);單次攻擊公式 = bytecode 真值;結局序列為 remake 組合(見 main enter_ending)。
add_executable(verify_ending tools/verify/verify_ending.cpp)
target_link_libraries(verify_ending PRIVATE dwr_game)
add_test(NAME verify_ending
         COMMAND verify_ending ${CMAKE_SOURCE_DIR}/assets/bundle)

# tools/verify:grounding 觀測工具(非 ctest)——
#   probe_encounter_id:跑 area 事件格 + headless_encounter,印 op_8A 怪物 id(終戰怪物 id 來源)。
#   probe_namtar_balance:載入隊伍 + Namtar Boss 跑確定性戰鬥,印初值/結果(供調平衡)。
add_executable(probe_encounter_id tools/verify/probe_encounter_id.cpp)
target_link_libraries(probe_encounter_id PRIVATE dwr_vm dwr_resource)
add_executable(probe_namtar_balance tools/verify/probe_namtar_balance.cpp)
target_link_libraries(probe_namtar_balance PRIVATE dwr_game)

# tools/verify:建角流程確定性 PASS/FAIL(新遊戲 / 建立人物)。
#   驗:建出的 record 解析回 CharacterRecord round-trip(名/屬性/性別/HP/STUN/PWR);
#   屬性在合法範圍;effective_av/dv == DEX/4(SDA);點數守恆(配點前後總點數一致);
#   確定性(同 DraftCharacter → 同 512B)。grounded in docs/44 §1 + docs/33 手冊。
add_executable(verify_chargen tools/verify/verify_chargen.cpp)
target_link_libraries(verify_chargen PRIVATE dwr_game)
add_test(NAME verify_chargen COMMAND verify_chargen)

# tools/verify:RPG 成長迴圈確定性 PASS/FAIL(升級 XP→Level+2AP+HP/STUN/STR、
#   X 配點 AP 守恆 + effective_av 隨 DEX、使用物品 U 效果+消耗、裝備穿脫 AV/AC 隨變、
#   技能檢定等級→成功率、包紮回 HP、512B raw round-trip)。
#   grounded:升級+2AP=SDA(docs/44 L103)、STR/HP 隨等級=手冊;XP 曲線/技能難度=remake(誠實標)。
add_executable(verify_progression tools/verify/verify_progression.cpp)
target_link_libraries(verify_progression PRIVATE dwr_game)
add_test(NAME verify_progression COMMAND verify_progression)

# tools/verify:次要指令(刪除 / 改名 / 重排 / 物品轉移 / 丟棄)確定性 PASS/FAIL。
#   真值層級:remake 設計 grounded 手冊(D/R 手冊 147、O 重排 CONTROLS、Item Discard/Transfer)。
#   驗 raw[] 同步 + 512B round-trip(存檔相容)。
add_executable(verify_party_ops tools/verify/verify_party_ops.cpp)
target_link_libraries(verify_party_ops PRIVATE dwr_game)
add_test(NAME verify_party_ops COMMAND verify_party_ops)

# tools/verify:法術系統確定性 PASS/FAIL(法術表筆數/效果值對拍手冊 + 施法扣 Power +
#   傷害/治療在手冊範圍 + 確定性)。grounded in docs/33 手冊;非 oracle(見 spells.hpp 檔頭)。
add_executable(verify_spells tools/verify/verify_spells.cpp)
target_link_libraries(verify_spells PRIVATE dwr_game)
add_test(NAME verify_spells
         COMMAND verify_spells ${CMAKE_SOURCE_DIR}/assets/bundle)

# tools/verify:商店買賣確定性 PASS/FAIL(買=gold[81] 扣對 + 物品入背包 + 窮買不到 +
#   背包滿擋下;賣=gold 加半價 + 物品移除 + 空格擋下;stock.json 載入;512B raw round-trip)。
#   grounded:23B 物品格式/售價編碼=fraterrisus(docs/44 §2);買賣邏輯/curated 庫存=remake 設計(誠實標)。
add_executable(verify_shop tools/verify/verify_shop.cpp)
target_link_libraries(verify_shop PRIVATE dwr_game)
add_test(NAME verify_shop
         COMMAND verify_shop ${CMAKE_SOURCE_DIR}/assets/bundle)

# tools/verify:酒館招募確定性 PASS/FAIL(NPC 入隊 + identifier[77] gate 防重複 +
#   隊伍上限 7 + 未知 id 擋下 + 隊伍 round-trip byte-for-byte + 招募後 raw[77] 正確)。
#   grounded:512B 格式 + identifier[77] + NPC 名(fraterrisus);NPC 屬性/招募邏輯=remake 設計(誠實標)。
add_executable(verify_recruit tools/verify/verify_recruit.cpp)
target_link_libraries(verify_recruit PRIVATE dwr_game)
add_test(NAME verify_recruit
         COMMAND verify_recruit ${CMAKE_SOURCE_DIR}/assets/bundle)

# tools/verify:把原版戰鬥腳本(res 3)餵進 remake VM 的調查工具(gap 分析,非 ctest)。
#   印跑了幾步、停在哪個 opcode/pc。見 docs/42_COMBAT_BYTECODE.md。
add_executable(probe_combat_script tools/verify/probe_combat_script.cpp)
target_link_libraries(probe_combat_script PRIVATE dwr_vm dwr_game)

# probe_combat_round:驅動 res18/res4 動作指派狀態機,抵達 res3 actor 迴圈,觀察怪物 HP 結算。
add_executable(probe_combat_round tools/verify/probe_combat_round.cpp)
target_link_libraries(probe_combat_round PRIVATE dwr_vm dwr_game)

# probe_monster_stats:跑 res3 戰鬥 setup,dump 怪物群緩衝 + setup driver 算出的 AV/AC。
add_executable(probe_monster_stats tools/verify/probe_monster_stats.cpp)
target_link_libraries(probe_monster_stats PRIVATE dwr_vm dwr_game)

# tools/verify:戰鬥 bytecode 確定性執行(res3 跑到攻擊迴圈、軌跡可重現、無缺 opcode)。
#   注意:此 ctest 不驗 HP-vs-oracle(roster pipeline 未完整逆向,見 docs/42 §9)。
add_executable(verify_combat_script tools/verify/verify_combat_script.cpp)
target_link_libraries(verify_combat_script PRIVATE dwr_vm dwr_game)
add_test(NAME verify_combat_script
         COMMAND verify_combat_script ${CMAKE_SOURCE_DIR}/assets/bundle)

# verify_combat_round:res3 全戰鬥閉環守護(actor 迴圈 → to-hit → 傷害 → 怪物 HP 扣減 + 確定性)。
add_executable(verify_combat_round tools/verify/verify_combat_round.cpp)
target_link_libraries(verify_combat_round PRIVATE dwr_vm dwr_game)
add_test(NAME verify_combat_round
         COMMAND verify_combat_round ${CMAKE_SOURCE_DIR}/assets/bundle)

# tools/verify:怪物圖區渲染 vs oracle golden(byte-for-byte;sprite 渲染管線無漂移)。
add_executable(verify_encounter_golden tools/verify/verify_encounter_golden.cpp)
target_link_libraries(verify_encounter_golden PRIVATE dwr_render)
add_test(NAME verify_encounter_golden_spider
         COMMAND verify_encounter_golden
         ${CMAKE_SOURCE_DIR}/assets/bundle/sprites/196_spider.spr
         ${CMAKE_SOURCE_DIR}/assets/bundle/sprites/golden/196_spider.viewport.ppm)
add_test(NAME verify_encounter_golden_wolf
         COMMAND verify_encounter_golden
         ${CMAKE_SOURCE_DIR}/assets/bundle/sprites/168_wolf.spr
         ${CMAKE_SOURCE_DIR}/assets/bundle/sprites/golden/168_wolf.viewport.ppm)

# tools/verify:遊戲內 UI chrome(石磚邊框 + Dragon Wars logo + pillar)渲染 vs oracle golden
#   (byte-for-byte;UiPieces::draw_chrome 對拍 opendw draw_ui_piece/ui_draw/ui_header_draw)。
add_executable(verify_ui_pieces_golden tools/verify/verify_ui_pieces_golden.cpp)
target_link_libraries(verify_ui_pieces_golden PRIVATE dwr_render)
add_test(NAME verify_ui_pieces_golden
         COMMAND verify_ui_pieces_golden
         ${CMAKE_SOURCE_DIR}/assets/bundle/viewport/ui_pieces.bin
         ${CMAKE_SOURCE_DIR}/assets/bundle/viewport/golden/ui_pieces.chrome.ppm)

# tools/verify:掃所有關卡找會改 gs[2](area)/入口 gs[0/1/3] 的事件(換場逆向)。
add_executable(probe_areaswitch tools/verify/probe_areaswitch.cpp)
target_link_libraries(probe_areaswitch PRIVATE dwr_vm)

# tools/verify:地圖換場/同區傳送的確定性 PASS/FAIL 驗證(ctest)。
add_executable(verify_areaswitch tools/verify/verify_areaswitch.cpp)
target_link_libraries(verify_areaswitch PRIVATE dwr_vm)
add_test(NAME verify_areaswitch
         COMMAND verify_areaswitch ${CMAKE_SOURCE_DIR}/assets/bundle)

# tools/verify:主線跨區連通 flood-fill(P0-3)。世界圖樞紐換區由 DRAGON.COM 反組譯
# 反推(opendw 未實作);本工具為觀測/追蹤用,非 byte-for-byte 對拍 gate。
add_executable(verify_mainline_reachable tools/verify/verify_mainline_reachable.cpp)
target_link_libraries(verify_mainline_reachable PRIVATE dwr_vm)

# tools/verify:世界圖逐格盤點(area 0 每個特殊格 → script bytes → 解碼 IDX)。
# grounding 用(docs/54 盤點表來源),非 ctest gate。
add_executable(dump_worldmap_tiles tools/verify/dump_worldmap_tiles.cpp)
target_link_libraries(dump_worldmap_tiles PRIVATE dwr_resource)

# tools/verify:逐 area dump 牆屬性 word_11C6 nibble + tile 型分佈(門/陷阱逆向 grounding;
# docs/57 §2 結論來源),非 ctest gate。
add_executable(dump_wall_attr tools/verify/dump_wall_attr.cpp)
target_link_libraries(dump_wall_attr PRIVATE dwr_resource)

# tools/verify:逐 area dump data_56C6 牆/門型表(兩半)+ 牆 nibble→tag + hilo 牆型碼
# (門牆識別深掘;docs/57 §2「可識別前方牆型 byte、門語意受阻」結論來源),非 ctest gate。
add_executable(dump_door_table tools/verify/dump_door_table.cpp)
target_link_libraries(dump_door_table PRIVATE dwr_resource dwr_render)
add_executable(dump_door_cells tools/verify/dump_door_cells.cpp)
target_link_libraries(dump_door_cells PRIVATE dwr_resource dwr_render)

# tools/verify:渲染 19 個元件 sprite frame(路徑 A:視覺分類門 vs 牆/岩),非 ctest gate。
add_executable(render_components tools/verify/render_components.cpp)
target_link_libraries(render_components PRIVATE dwr_render)

# tools/verify:第一人稱 viewport 單格組景(路徑 A:目視門牆候選 vs 一般牆),非 ctest gate。
add_executable(render_fp_cell tools/verify/render_fp_cell.cpp)
target_link_libraries(render_fp_cell PRIVATE dwr_render dwr_resource)

# tools/verify:路徑 B —— 逐關逐事件格跑 VM,偵測扣 HP/Stun 行為 → 真實陷阱格,非 ctest gate。
add_executable(detect_traps tools/verify/detect_traps.cpp)
target_link_libraries(detect_traps PRIVATE dwr_vm)

# tools/verify:路徑 B(技能版)—— 逐關逐事件格跑 VM,攔截 op_5D 角色屬性讀取,
# 偵測讀取 skill 欄(0x20-0x3A)的事件格 → 技能檢定觸發點。非 ctest gate。
add_executable(detect_skill_checks tools/verify/detect_skill_checks.cpp)
target_link_libraries(detect_skill_checks PRIVATE dwr_vm)

# tools/verify:非世界圖子區入口探測(跑母區事件 script,觀測 gs 寫入 / op58 tag /
# halt opcode)。用於判定 Phoebus(6)/Byzanople(9) 等子區是否卡在未實作 opcode。
add_executable(probe_subarea_entry tools/verify/probe_subarea_entry.cpp)
target_link_libraries(probe_subarea_entry PRIVATE dwr_vm)

add_executable(trace_subarea_dyn tools/verify/trace_subarea_dyn.cpp)
target_link_libraries(trace_subarea_dyn PRIVATE dwr_vm)

add_executable(trace_quest_gates tools/verify/trace_quest_gates.cpp)
target_link_libraries(trace_quest_gates PRIVATE dwr_vm)

add_executable(render_endgame tools/verify/render_endgame.cpp)
target_link_libraries(render_endgame PRIVATE dwr_vm dwr_render dwr_i18n)

add_executable(verify_city_entry tools/verify/verify_city_entry.cpp)
target_link_libraries(verify_city_entry PRIVATE dwr_vm)
add_test(NAME verify_city_entry
         COMMAND verify_city_entry ${CMAKE_SOURCE_DIR}/assets/bundle)

# tools/verify:wrap 邊界關卡(flag&2)載入 + 環繞 flood-fill 連通 + 不污染非 wrap 路徑。
# wrap 在 oracle opendw 為 exit(1) 未實作 → 本驗證為自洽 + 不變式守護,非 byte-for-byte 對拍。
add_executable(verify_wrap tools/verify/verify_wrap.cpp)
target_link_libraries(verify_wrap PRIVATE dwr_resource)
add_test(NAME verify_wrap
         COMMAND verify_wrap ${CMAKE_SOURCE_DIR}/assets/bundle)

# tools/verify:op_58 跨資源 halt 統計(補齊前/後)+ 確定性 PASS/FAIL(ctest)。
add_executable(verify_op58 tools/verify/verify_op58.cpp)
target_link_libraries(verify_op58 PRIVATE dwr_vm)
add_test(NAME verify_op58
         COMMAND verify_op58 ${CMAKE_SOURCE_DIR}/assets/bundle)

# i18n 覆蓋率 / fallback 契約:鎖「多語系、日文 ready」架構。
add_executable(verify_i18n tools/verify/verify_i18n.cpp)
target_link_libraries(verify_i18n PRIVATE dwr_i18n)
add_test(NAME verify_i18n
         COMMAND verify_i18n ${CMAKE_SOURCE_DIR}/assets/i18n)

# app 層整合 smoke:headless 跑遍 main.cpp 各模式,斷言不崩 + dump 確定性。
if(TARGET opendw_remake)
  add_test(NAME smoke_app
           COMMAND bash ${CMAKE_SOURCE_DIR}/tools/verify/smoke_test.sh
                   $<TARGET_FILE:opendw_remake> ${CMAKE_SOURCE_DIR})
endif()

# 全 40 關第一人稱 viewport 像素對拍(byte-for-byte vs opendw golden)+ viewport 模板解碼。
# remake 最關鍵的正確性保證,先前未進 ctest;此處納入 CI 守護(154 case + 模板)。
add_test(NAME render_sweep
         COMMAND bash ${CMAKE_SOURCE_DIR}/tools/verify/render_sweep.sh
                 $<TARGET_FILE_DIR:verify_fp_sweep> ${CMAKE_SOURCE_DIR})

# viewport 中間階段對拍(level 1):compose(FOV→繪製指令)+ fp(最終 viewport_memory)。
# render_sweep 只驗最終像素;這兩個守護 compose/select 中間階段不漂移。
add_test(NAME verify_compose_l1
         COMMAND verify_compose ${CMAKE_SOURCE_DIR}/assets/bundle/maps/1.lvl
                 ${CMAKE_SOURCE_DIR}/assets/bundle/maps/golden)
add_test(NAME verify_fp_l1
         COMMAND verify_fp ${CMAKE_SOURCE_DIR}/assets/bundle/maps/1.lvl
                 ${CMAKE_SOURCE_DIR}/assets/bundle/maps/golden
                 ${CMAKE_SOURCE_DIR}/assets/bundle/components)

# viewport 解碼對拍:remake ViewportDecoder vs golden .vpmem (byte-for-byte)。
add_executable(verify_viewport tools/verify/verify_viewport.cpp)
target_link_libraries(verify_viewport PRIVATE dwr_render)

# viewport 組景 step1 對拍:remake sample_fov vs golden_compose (FOV 取樣 + 牆 nibble)。
add_executable(verify_fov tools/verify/verify_fov.cpp)
target_link_libraries(verify_fov PRIVATE dwr_render dwr_resource)

add_executable(verify_compose tools/verify/verify_compose.cpp)
target_link_libraries(verify_compose PRIVATE dwr_render dwr_resource)

# viewport 組景 step3 像素對拍:remake render_first_person vs golden_pixel (10880B)。
add_executable(verify_fp tools/verify/verify_fp.cpp)
target_link_libraries(verify_fp PRIVATE dwr_render dwr_resource)

# 全 40 關第一人稱像素對拍廣度掃描 (每關多可走格 × 4 朝向)。
add_executable(verify_fp_sweep tools/verify/verify_fp_sweep.cpp)
target_link_libraries(verify_fp_sweep PRIVATE dwr_render dwr_resource)

# 隊伍狀態面板:載 bundle 預設隊伍,印 record 欄位 + dump 像素層(狀態條)PPM。
# 文字層(名字)需 SDL2_ttf,故此工具同 opendw_remake 條件下建。
if(SDL2_FOUND AND DWR_HAVE_TTF)
  add_executable(verify_party_panel tools/verify/verify_party_panel.cpp
    src/render/text_layer.cpp
    src/game/party_panel.cpp)
  target_link_libraries(verify_party_panel PRIVATE dwr_game SDL2::SDL2)
  target_include_directories(verify_party_panel PRIVATE ${SDL2_TTF_INCLUDE_DIRS})
  target_link_libraries(verify_party_panel PRIVATE ${SDL2_TTF_LIBRARIES})
endif()

# 俯視 automap 像素對拍:remake Minimap vs golden_minimap .mmem (byte-for-byte)。
add_executable(verify_automap tools/verify/verify_automap.cpp)
target_link_libraries(verify_automap PRIVATE dwr_render dwr_resource)
add_test(NAME verify_automap_l1
         COMMAND verify_automap ${CMAKE_SOURCE_DIR}/assets/bundle/maps/1.lvl
                 ${CMAKE_SOURCE_DIR}/assets/bundle/maps/golden
                 ${CMAKE_SOURCE_DIR}/assets/bundle/viewport
                 ${CMAKE_SOURCE_DIR}/assets/bundle/components)
# area 0 (Dilmun) 大型 wraparound overworld 的 automap 對拍(鎖稽核 #1 回歸:
#   單趟 per-pass viewport_memory 在大維度/wrap 關卡也須與 golden byte-for-byte;
#   on-screen 完整鋪滿由 render_full 8 趟疊圖達成,headless dump 目視驗證)。
add_test(NAME verify_automap_l0
         COMMAND verify_automap ${CMAKE_SOURCE_DIR}/assets/bundle/maps/0.lvl
                 ${CMAKE_SOURCE_DIR}/assets/bundle/maps/golden
                 ${CMAKE_SOURCE_DIR}/assets/bundle/viewport
                 ${CMAKE_SOURCE_DIR}/assets/bundle/components)

# fog of war seen 累積確定性驗證(走固定路徑 → seen 集合 = 預期聯集 + 揭露正確性)。
add_executable(verify_seen tools/verify/verify_seen.cpp)
target_link_libraries(verify_seen PRIVATE dwr_render dwr_resource dwr_game)
add_test(NAME verify_seen_l1
         COMMAND verify_seen ${CMAKE_SOURCE_DIR}/assets/bundle/maps/1.lvl
                 ${CMAKE_SOURCE_DIR}/assets/bundle/viewport
                 ${CMAKE_SOURCE_DIR}/assets/bundle/components)

# tools/extract:抽各關用到的牆/地/天空元件資源 → bundle/components/<tag>.bin。
add_executable(extract_components tools/extract/extract_components.cpp)
target_link_libraries(extract_components PRIVATE dwr_render dwr_resource)
