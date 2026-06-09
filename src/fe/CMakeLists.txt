# fe (前端) CMakeLists.txt
cmake_minimum_required(VERSION 3.13.4)
project(opendw-fe C)

set(TARGET_NAME sdldragon)

# 定義來源檔案
set(SOURCES main.c vga_sdl.c)

# 加入可執行檔
add_executable(${TARGET_NAME} ${SOURCES})

# 加入 SDL2 與 libdragon 的 include 路徑
target_include_directories(${TARGET_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/src/lib
    ${SDL2_INCLUDE_DIRS}
)

# 連結 SDL2 與 dragon 核心
target_link_libraries(${TARGET_NAME} PRIVATE
    dragon
    ${SDL2_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
)

# 也建置 ndragon（無顯示驅動）
add_executable(ndragon main.c vga_null.c)
target_include_directories(ndragon PRIVATE ${CMAKE_SOURCE_DIR}/src/lib)
target_link_libraries(ndragon PRIVATE dragon)

# 安裝
install(TARGETS ${TARGET_NAME} ndragon DESTINATION bin)

# 警告選項
if(MSVC)
    target_compile_options(${TARGET_NAME} PRIVATE /W4)
    target_compile_options(ndragon PRIVATE /W4)
else()
    target_compile_options(${TARGET_NAME} PRIVATE -Wall)
    target_compile_options(ndragon PRIVATE -Wall)
endif()

# 定義資源目錄巨集
target_compile_definitions(${TARGET_NAME} PRIVATE
    "DATA_DIR=\"${CMAKE_SOURCE_DIR}/data\""
    "FONTS_DIR=\"${CMAKE_BINARY_DIR}/fonts\""
)
target_compile_definitions(ndragon PRIVATE
    "DATA_DIR=\"${CMAKE_SOURCE_DIR}/data\""
)
