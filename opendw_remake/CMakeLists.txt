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
add_library(dwr_render STATIC src/render/picture.cpp src/render/font.cpp src/render/cjk_font.cpp src/render/sprite.cpp)
add_library(dwr_i18n STATIC src/i18n/strings.cpp src/i18n/string_table.cpp)
target_include_directories(dwr_i18n PUBLIC src)
target_include_directories(dwr_render PUBLIC src)

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
find_package(SDL2 QUIET)
if(SDL2_FOUND)
  add_executable(opendw_remake src/main.cpp src/render/sdl_video.cpp)
  target_link_libraries(opendw_remake PRIVATE dwr_vm dwr_render dwr_i18n SDL2::SDL2)
  message(STATUS "SDL2 found: opendw_remake app enabled")
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
