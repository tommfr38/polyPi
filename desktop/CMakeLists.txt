cmake_minimum_required(VERSION 3.16)
project(polyPi CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui)

find_package(OpenGL REQUIRED)
find_package(Threads REQUIRED)
find_package(CURL REQUIRED)

# GMP: brew keg on macOS, system package / vcpkg elsewhere.
find_path(GMP_INCLUDE_DIR gmp.h
  HINTS /opt/homebrew/include /usr/local/include $ENV{GMP_ROOT}/include)
find_library(GMP_LIBRARY NAMES gmp
  HINTS /opt/homebrew/lib /usr/local/lib $ENV{GMP_ROOT}/lib)
if(NOT GMP_INCLUDE_DIR OR NOT GMP_LIBRARY)
  message(FATAL_ERROR "GMP not found. On macOS: brew install gmp. On Windows: vcpkg install gmp, or set GMP_ROOT.")
endif()

if(APPLE)
  find_path(SDL2_INCLUDE_DIR SDL.h PATH_SUFFIXES SDL2 HINTS /opt/homebrew/include)
  find_library(SDL2_LIBRARY NAMES SDL2 HINTS /opt/homebrew/lib)
else()
  find_package(SDL2 REQUIRED)
endif()

add_executable(polyPi
  src/main.cpp
  src/theme.cpp
  src/pi_worker.cpp
  src/particles.cpp
  src/updater.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../core/pi_engine.c
  ${IMGUI_DIR}/imgui.cpp
  ${IMGUI_DIR}/imgui_draw.cpp
  ${IMGUI_DIR}/imgui_tables.cpp
  ${IMGUI_DIR}/imgui_widgets.cpp
  ${IMGUI_DIR}/imgui_demo.cpp
  ${IMGUI_DIR}/backends/imgui_impl_sdl2.cpp
  ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)

target_include_directories(polyPi PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../core
  ${GMP_INCLUDE_DIR}
  ${IMGUI_DIR}
  ${IMGUI_DIR}/backends
  ${SDL2_INCLUDE_DIR}
  ${CURL_INCLUDE_DIRS}
)

target_link_libraries(polyPi PRIVATE
  ${SDL2_LIBRARY}
  OpenGL::GL
  ${GMP_LIBRARY}
  CURL::libcurl
  Threads::Threads
)

if(APPLE)
  target_link_libraries(polyPi PRIVATE "-framework CoreFoundation" "-framework Cocoa" "-framework IOKit")
endif()

target_compile_definitions(polyPi PRIVATE POLYPI_VERSION="0.1.0")

if(WIN32)
  target_link_libraries(polyPi PRIVATE mingw32)
endif()
