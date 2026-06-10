cmake_minimum_required(VERSION 3.16)
project(opendw_remake LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT MSVC)
  add_compile_options(-Wall -Wextra)
endif()

# --- resource 模組(R0:archive + text_codec,無外部相依)---
add_library(dwr_resource STATIC
  src/resource/text_codec.cpp
  src/resource/archive.cpp
)
target_include_directories(dwr_resource PUBLIC src)

# --- R0 驗證工具 ---
add_executable(verify_r0 tools/verify/verify_r0.cpp)
target_link_libraries(verify_r0 PRIVATE dwr_resource)

enable_testing()
# round-trip / differential 測試在 R1 起接入(對拍 opendw)。

# 後續階段(R2+)再加入 SDL2 / freetype:
#   find_package(SDL2 REQUIRED); find_package(Freetype REQUIRED)
#   add_executable(opendw_remake src/main.cpp ...)
