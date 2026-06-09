# lib (核心引擎) CMakeLists.txt
cmake_minimum_required(VERSION 3.13.4)
project(opendw-lib C)

set(TARGET_NAME dragon)

# 定義來源檔案
set(SOURCES
    bufio.c bufio.h
    compress.c compress.h
    engine.c engine.h
    log.c log.h
    mouse.c mouse.h
    offsets.c offsets.h
    player.c player.h
    resource.c resource.h
    state.c state.h
    tables.c tables.h
    timers.c timers.h
    ui.c ui.h
    utils.c utils.h
    vga.c vga.h
)

# 建立靜態庫
add_library(${TARGET_NAME} STATIC ${SOURCES})

# Include 路徑
target_include_directories(${TARGET_NAME} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# 警告選項
if(MSVC)
    target_compile_options(${TARGET_NAME} PRIVATE /W4)
else()
    target_compile_options(${TARGET_NAME} PRIVATE -Wall)
endif()

# 定義資料目錄
target_compile_definitions(${TARGET_NAME} PRIVATE
    "DATA_DIR=\"${CMAKE_SOURCE_DIR}/data\""
)
