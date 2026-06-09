# src 上層 CMakeLists.txt

# 加入子目錄
add_subdirectory(lib)
add_subdirectory(fe)
add_subdirectory(tools)

# 如果有 Check 庫，才加入測試
if(Check_FOUND OR CHECK_FOUND)
    add_subdirectory(tests)
endif()
