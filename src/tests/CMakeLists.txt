# tests CMakeLists.txt
cmake_minimum_required(VERSION 3.13.4)
project(opendw-tests CXX)

# 尋找 Check 庫
find_package(PkgConfig REQUIRED)
pkg_check_modules(CHECK REQUIRED check)

# 定義測試目標名稱
set(CORE_TEST_NAME opendw_test)
set(CJK_TEST_NAME opendw_cjk_test)
set(TRANSLATION_TEST_NAME opendw_translation_test)

# 核心測試
set(CORE_SOURCES
    test_compress.cpp
    test_opendw.cpp
    test_vga.cpp
)

add_executable(${CORE_TEST_NAME} ${CORE_SOURCES})
target_include_directories(${CORE_TEST_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/src/lib
    ${CHECK_INCLUDE_DIRS}
)
target_link_libraries(${CORE_TEST_NAME} PRIVATE
    dragon
    ${CHECK_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
)
target_compile_definitions(${CORE_TEST_NAME} PRIVATE
    "DATA_DIR=\"${CMAKE_SOURCE_DIR}/data\""
    "TEST_DATA_DIR=\"${CMAKE_SOURCE_DIR}/tests/data\""
)
add_test(NAME ${CORE_TEST_NAME} COMMAND ${CORE_TEST_NAME})

# 中文渲染測試
set(CJK_SOURCES
    test_cjk.cpp
)

add_executable(${CJK_TEST_NAME} ${CJK_SOURCES})
target_include_directories(${CJK_TEST_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/src/lib
    ${CMAKE_SOURCE_DIR}/src/fe
    ${CHECK_INCLUDE_DIRS}
)
target_link_libraries(${CJK_TEST_NAME} PRIVATE
    dragon
    ${CHECK_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
)
target_compile_definitions(${CJK_TEST_NAME} PRIVATE
    "DATA_DIR=\"${CMAKE_SOURCE_DIR}/data\""
    "FONTS_DIR=\"${CMAKE_BINARY_DIR}/fonts\""
)
add_test(NAME ${CJK_TEST_NAME} COMMAND ${CJK_TEST_NAME})

# 翻譯驗證測試
set(TRANSLATION_SOURCES
    test_translation.cpp
)

add_executable(${TRANSLATION_TEST_NAME} ${TRANSLATION_SOURCES})
target_include_directories(${TRANSLATION_TEST_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/src/lib
    ${CHECK_INCLUDE_DIRS}
)
target_link_libraries(${TRANSLATION_TEST_NAME} PRIVATE
    dragon
    ${CHECK_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
)
target_compile_definitions(${TRANSLATION_TEST_NAME} PRIVATE
    "DATA_DIR=\"${CMAKE_SOURCE_DIR}/data\""
    "TRANSLATION_DIR=\"${CMAKE_SOURCE_DIR}/docs\""
)
add_test(NAME ${TRANSLATION_TEST_NAME} COMMAND ${TRANSLATION_TEST_NAME})

# 警告選項
if(MSVC)
    target_compile_options(${CORE_TEST_NAME} PRIVATE /W4 /WX)
    target_compile_options(${CJK_TEST_NAME} PRIVATE /W4 /WX)
    target_compile_options(${TRANSLATION_TEST_NAME} PRIVATE /W4 /WX)
else()
    target_compile_options(${CORE_TEST_NAME} PRIVATE -Wall -Werror)
    target_compile_options(${CJK_TEST_NAME} PRIVATE -Wall -Werror)
    target_compile_options(${TRANSLATION_TEST_NAME} PRIVATE -Wall -Werror)
endif()

# 安裝測試執行檔（可選）
install(TARGETS ${CORE_TEST_NAME} ${CJK_TEST_NAME} ${TRANSLATION_TEST_NAME}
        DESTINATION bin
        OPTIONAL)
