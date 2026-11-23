cmake_minimum_required(VERSION 3.24)
project(moji_rtxui LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# RTXUI is an experimental C++23 TUI library. It is not vendored on this
# branch: clone it with fetch_rtxui.ps1, or point RTXUI_DIR at an existing
# checkout. The PoC is independent from src/tui and build.bat.
set(RTXUI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/rtxui"
    CACHE PATH "Path to the RTXUI source checkout")

if(NOT EXISTS "${RTXUI_DIR}/CMakeLists.txt")
  message(FATAL_ERROR
    "RTXUI not found at ${RTXUI_DIR}.\n"
    "Run rtxui_poc/fetch_rtxui.ps1, or configure with "
    "-DRTXUI_DIR=<path-to-RTXUI>.")
endif()

add_subdirectory("${RTXUI_DIR}" "${CMAKE_BINARY_DIR}/rtxui" EXCLUDE_FROM_ALL)

add_executable(moji-rtxui
  src/main.cpp
  ../src/core/util.cpp
  ../src/core/model.cpp
  ../src/core/elist.cpp
  ../src/core/store.cpp
  ../src/core/repository.cpp
)

target_include_directories(moji-rtxui PRIVATE
  ../src
  ../third_party
)

target_compile_definitions(moji-rtxui PRIVATE
  UNICODE
  _UNICODE
  NOMINMAX
  WIN32_LEAN_AND_MEAN
)

target_link_libraries(moji-rtxui PRIVATE rtxui)
if(WIN32)
  target_link_libraries(moji-rtxui PRIVATE shell32)
endif()
if(MINGW)
  target_link_options(moji-rtxui PRIVATE
    -static -static-libgcc -static-libstdc++)
endif()
