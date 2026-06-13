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
add_library(dwr_render STATIC src/render/picture.cpp src/render/font.cpp src/render/cjk_font.cpp src/render/sprite.cpp src/render/viewport.cpp src/render/viewport_compose.cpp)
target_link_libraries(dwr_render PUBLIC dwr_resource)
add_library(dwr_i18n STATIC src/i18n/strings.cpp src/i18n/string_table.cpp)
target_include_directories(dwr_i18n PUBLIC src)
target_include_directories(dwr_render PUBLIC src)

# --- game 模組(隊伍角色資料 + 右側狀態面板渲染 + 戰鬥結算)---
add_library(dwr_game STATIC src/game/party.cpp src/game/combat.cpp)
target_include_directories(dwr_game PUBLIC src)
target_link_libraries(dwr_game PUBLIC dwr_render)

# --- save 模組(存檔/讀檔;自包含,無 render/SDL 相依,可獨立測試)---
add_library(dwr_save STATIC src/game/savegame.cpp)
target_include_directories(dwr_save PUBLIC src)

# 存檔 round-trip 自測(不需 SDL;印 PASS/FAIL)。
add_executable(verify_save tools/verify/verify_save.cpp)
target_link_libraries(verify_save PRIVATE dwr_save)
add_test(NAME verify_save COMMAND verify_save)

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
  add_executable(opendw_remake
    src/main.cpp
    src/render/sdl_video.cpp
    src/render/text_layer.cpp
    src/game/party_panel.cpp)
  target_link_libraries(opendw_remake PRIVATE dwr_vm dwr_render dwr_i18n dwr_game dwr_save SDL2::SDL2)
  if(DWR_HAVE_TTF)
    target_include_directories(opendw_remake PRIVATE ${SDL2_TTF_INCLUDE_DIRS})
    target_link_libraries(opendw_remake PRIVATE ${SDL2_TTF_LIBRARIES})
    message(STATUS "SDL2 + SDL2_ttf found: opendw_remake two-layer renderer enabled")
  else()
    message(FATAL_ERROR "SDL2_ttf not found (required for text layer). Install libsdl2-ttf-dev.")
  endif()
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

# tools/extract:全 40 關事件腳本 op_58 跨資源 tag 聯集 → bundle/scripts/<tag>.bin。
add_executable(extract_eventscripts tools/extract/extract_eventscripts.cpp)
target_link_libraries(extract_eventscripts PRIVATE dwr_vm)

# tools/extract:怪物記錄(res31)→ bundle/monsters/monsters.bin(自包含)。
add_executable(extract_monsters tools/extract/extract_monsters.cpp)
target_link_libraries(extract_monsters PRIVATE dwr_resource)

# tools/verify:戰鬥切片確定性 PASS/FAIL(怪物萃取 vs oracle 名單 + RNG + 結算可重現)。
add_executable(verify_combat tools/verify/verify_combat.cpp)
target_link_libraries(verify_combat PRIVATE dwr_game)
add_test(NAME verify_combat
         COMMAND verify_combat ${CMAKE_SOURCE_DIR}/assets/bundle)

# tools/verify:把原版戰鬥腳本(res 3)餵進 remake VM 的調查工具(gap 分析,非 ctest)。
#   印跑了幾步、停在哪個 opcode/pc。見 docs/42_COMBAT_BYTECODE.md。
add_executable(probe_combat_script tools/verify/probe_combat_script.cpp)
target_link_libraries(probe_combat_script PRIVATE dwr_vm dwr_game)

# tools/verify:戰鬥 bytecode 確定性執行(res3 跑到攻擊迴圈、軌跡可重現、無缺 opcode)。
#   注意:此 ctest 不驗 HP-vs-oracle(roster pipeline 未完整逆向,見 docs/42 §9)。
add_executable(verify_combat_script tools/verify/verify_combat_script.cpp)
target_link_libraries(verify_combat_script PRIVATE dwr_vm dwr_game)
add_test(NAME verify_combat_script
         COMMAND verify_combat_script ${CMAKE_SOURCE_DIR}/assets/bundle)

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

# tools/verify:掃所有關卡找會改 gs[2](area)/入口 gs[0/1/3] 的事件(換場逆向)。
add_executable(probe_areaswitch tools/verify/probe_areaswitch.cpp)
target_link_libraries(probe_areaswitch PRIVATE dwr_vm)

# tools/verify:地圖換場/同區傳送的確定性 PASS/FAIL 驗證(ctest)。
add_executable(verify_areaswitch tools/verify/verify_areaswitch.cpp)
target_link_libraries(verify_areaswitch PRIVATE dwr_vm)
add_test(NAME verify_areaswitch
         COMMAND verify_areaswitch ${CMAKE_SOURCE_DIR}/assets/bundle)

# tools/verify:op_58 跨資源 halt 統計(補齊前/後)+ 確定性 PASS/FAIL(ctest)。
add_executable(verify_op58 tools/verify/verify_op58.cpp)
target_link_libraries(verify_op58 PRIVATE dwr_vm)
add_test(NAME verify_op58
         COMMAND verify_op58 ${CMAKE_SOURCE_DIR}/assets/bundle)

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

# tools/extract:抽各關用到的牆/地/天空元件資源 → bundle/components/<tag>.bin。
add_executable(extract_components tools/extract/extract_components.cpp)
target_link_libraries(extract_components PRIVATE dwr_render dwr_resource)
