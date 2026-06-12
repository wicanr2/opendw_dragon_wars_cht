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

enable_testing()
add_executable(vm_selftest tests/vm_selftest.cpp)
target_link_libraries(vm_selftest PRIVATE dwr_vm)
add_test(NAME vm_selftest COMMAND vm_selftest)

# 差異測試:remake VM trace,與 opendw oracle 對拍(見 tools_build/diff_trace.sh)。
add_executable(trace_remake tools/verify/trace_remake.cpp)
target_link_libraries(trace_remake PRIVATE dwr_vm)

# --- render 模組(R2:framebuffer + 全螢幕圖解碼 + 8×8 字)---
add_library(dwr_render STATIC src/render/picture.cpp src/render/font.cpp src/render/cjk_font.cpp)
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

# 後續階段(R2+)再加入 SDL2 / freetype:
#   find_package(SDL2 REQUIRED); find_package(Freetype REQUIRED)
#   add_executable(opendw_remake src/main.cpp ...)
