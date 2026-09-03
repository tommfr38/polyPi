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
  set(POLYPI_SDL_LIBS ${SDL2_LIBRARY})
else()
  find_package(SDL2 REQUIRED)
  # imported targets carry their own include dirs; SDL2main supplies WinMain
  # for the WIN32 (no console window) subsystem.
  set(POLYPI_SDL_LIBS SDL2::SDL2)
  if(TARGET SDL2::SDL2main)
    list(APPEND POLYPI_SDL_LIBS SDL2::SDL2main)
  endif()
endif()

# --- app icon ---
set(ICON_DIR ${CMAKE_CURRENT_SOURCE_DIR}/assets/icon)
set(MACOSX_ICON ${ICON_DIR}/polypi.icns)
set(POLYPI_PLATFORM_SOURCES "")

if(APPLE)
  set_source_files_properties(${MACOSX_ICON} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
  list(APPEND POLYPI_PLATFORM_SOURCES ${MACOSX_ICON})
elseif(WIN32)
  list(APPEND POLYPI_PLATFORM_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/polypi.rc)
endif()

add_executable(polyPi WIN32 MACOSX_BUNDLE
  ${POLYPI_PLATFORM_SOURCES}
  src/main.cpp
  src/theme.cpp
  src/pi_worker.cpp
  src/particles.cpp
  src/updater.cpp
  src/widgets.cpp
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
  ${POLYPI_SDL_LIBS}
  OpenGL::GL
  ${GMP_LIBRARY}
  CURL::libcurl
  Threads::Threads
)

if(APPLE)
  set_target_properties(polyPi PROPERTIES
    MACOSX_BUNDLE_BUNDLE_NAME "polyPi"
    MACOSX_BUNDLE_DISPLAY_NAME "polyPi"
    MACOSX_BUNDLE_GUI_IDENTIFIER "dev.polypi.app"
    MACOSX_BUNDLE_BUNDLE_VERSION "0.1.1"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "0.1.1"
    MACOSX_BUNDLE_ICON_FILE "polypi.icns"
  )
endif()

if(APPLE)
  target_link_libraries(polyPi PRIVATE "-framework CoreFoundation" "-framework Cocoa" "-framework IOKit")
endif()

target_compile_definitions(polyPi PRIVATE POLYPI_VERSION="0.1.0")

if(WIN32)
  # ship the dependency DLLs beside the exe so it runs outside the build tree
  add_custom_command(TARGET polyPi POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy -t $<TARGET_FILE_DIR:polyPi> $<TARGET_RUNTIME_DLLS:polyPi>
    COMMAND_EXPAND_LISTS)
endif()
