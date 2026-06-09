# OpenDW Dragon Wars 中文化專案
# 最低版本：Ubuntu 20.04LTS 使用 3.16.3
cmake_minimum_required(VERSION 3.13.4)
project(opendw VERSION 0.02 LANGUAGES C CXX)

# 啟用測試
enable_testing()

# 設定 C/C++ 標準
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 設定 cmake 模組路徑
set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake/)

# 尋找 SDL2（必要）
find_package(SDL2 REQUIRED)
message(STATUS "SDL2_FOUND=${SDL2_FOUND}")
message(STATUS "SDL2_INCLUDE_DIRS=${SDL2_INCLUDE_DIRS}")
message(STATUS "SDL2_LIBRARIES=${SDL2_LIBRARIES}")

# 尋找執行緒庫
find_package(Threads REQUIRED)

# 尋找 Check 測試庫（可選）
find_package(Check)
if(Check_FOUND)
    message(STATUS "Check library found")
else()
    message(STATUS "Check library not found - tests will be disabled")
endif()

# 安裝中文字型到建置目錄
set(FONTS_DIR "${CMAKE_BINARY_DIR}/fonts")
file(MAKE_DIRECTORY ${FONTS_DIR})

# 檢查系統中文字型
set(CJK_FONT_PATHS
    "/usr/share/fonts/truetype/wqy/wqy-zenhei.ttc"
    "/usr/share/fonts/truetype/wqy/wqy-microhei.ttc"
    "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc"
    "/usr/share/fonts/truetype/noto/NotoSansCJK-Regular.ttc"
    "/usr/share/fonts/truetype/droid/DroidSansFallbackFull.ttf"
)

set(CJK_FONT_FOUND FALSE)
foreach(font_path ${CJK_FONT_PATHS})
    if(EXISTS ${font_path})
        # 複製到建置目錄
        get_filename_component(font_name ${font_path} NAME)
        configure_file(${font_path} ${FONTS_DIR}/${font_name} COPYONLY)
        set(CJK_FONT_FOUND TRUE)
        message(STATUS "Found CJK font: ${font_path}")
        break()
    endif()
endforeach()

if(NOT CJK_FONT_FOUND)
    message(WARNING "No CJK font found - Chinese display will not work")
endif()

# 定義資源目錄
set(DATA_DIR "${CMAKE_SOURCE_DIR}/data")
set(FONTS_OUTPUT_DIR "${FONTS_DIR}")

# 加入子目錄
add_subdirectory(src)

# 安裝目標
install(TARGETS sdldragon DESTINATION bin)
install(DIRECTORY ${FONTS_OUTPUT_DIR} DESTINATION share/opendw)

# 安裝遊戲資料（如果有）
if(EXISTS ${DATA_DIR})
    install(DIRECTORY ${DATA_DIR} DESTINATION share/opendw/data)
endif()

# 摘要
message(STATUS "")
message(STATUS "=== OpenDW Dragon Wars 建置摘要 ===")
message(STATUS "Project: ${PROJECT_NAME} v${PROJECT_VERSION}")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "CJK Font dir: ${FONTS_DIR}")
message(STATUS "Data dir: ${DATA_DIR}")
message(STATUS "=====================================")
