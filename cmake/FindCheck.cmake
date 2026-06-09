# FindCheck.cmake - Find the Check unit testing library
# This module defines:
#   CHECK_FOUND - System has Check
#   CHECK_INCLUDE_DIRS - The Check include directories
#   CHECK_LIBRARIES - The libraries needed to use Check

find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
    pkg_check_modules(_CHECK QUIET check)
endif()

find_path(CHECK_INCLUDE_DIR
    NAMES check.h
    HINTS ${_CHECK_INCLUDE_DIRS}
)

find_library(CHECK_LIBRARY
    NAMES check
    HINTS ${_CHECK_LIBRARY_DIRS}
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Check
    REQUIRED_VARS CHECK_LIBRARY CHECK_INCLUDE_DIR
    VERSION_VAR _CHECK_VERSION
)

if(Check_FOUND)
    set(CHECK_LIBRARIES ${CHECK_LIBRARY})
    set(CHECK_INCLUDE_DIRS ${CHECK_INCLUDE_DIR})
endif()

mark_as_advanced(CHECK_INCLUDE_DIR CHECK_LIBRARY)
