# Android NDK:把引擎各 .cpp 編成 libmain.so(SDLActivity 載入)。
# 需先把 SDL2 + SDL2_ttf 的 Android 原始碼放到 ../SDL、../SDL2_ttf(見 android/README.md §1)。
cmake_minimum_required(VERSION 3.18)
project(opendw_remake_android CXX C)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 引擎根(opendw_remake/)。
set(ENGINE ${CMAKE_CURRENT_SOURCE_DIR}/../../../..)

# SDL2 + SDL2_ttf（Android 原始碼子專案;由 app/jni/ 下放置）。
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../SDL      SDL_build)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../SDL2_ttf SDL2_ttf_build)

# 引擎來源（對齊上層 CMakeLists 的 lib 切分,Android 全併入 libmain.so）。
file(GLOB_RECURSE ENGINE_SRC
  ${ENGINE}/src/resource/*.cpp ${ENGINE}/src/vm/*.cpp ${ENGINE}/src/render/*.cpp
  ${ENGINE}/src/i18n/*.cpp ${ENGINE}/src/game/*.cpp ${ENGINE}/src/audio/*.cpp)

add_library(main SHARED ${ENGINE}/src/main.cpp ${ENGINE_SRC})
target_include_directories(main PRIVATE ${ENGINE}/src)
target_link_libraries(main PRIVATE SDL2 SDL2_ttf SDL2main)
# __ANDROID__ 下 main.cpp 走 SDL_main 進入點;觸控由 sdl_video poll 處理(SDL_FINGER*)。
