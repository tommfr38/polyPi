cmake_minimum_required(VERSION 3.16)
project(polyPi CXX C)

# Single source of truth for the version: the compile definition below and the
# macOS bundle properties both read this, so they cannot disagree.
set(POLYPI_VERSION "0.1.2")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

if(MSVC)
  # CMake defaults to /EHsc, whose "c" half tells the compiler that extern "C"
  # functions never throw - so it skips the unwind code for them. The GMP
  # allocation hooks in pi_worker.cpp are extern "C" and throw bad_alloc on
  # purpose, which under /EHsc terminates instead of unwinding (warning C4297).
  # /EHs is the same model without that assumption.
  string(REPLACE "/EHsc" "/EHs" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
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
    MACOSX_BUNDLE_BUNDLE_VERSION "${POLYPI_VERSION}"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${POLYPI_VERSION}"
    MACOSX_BUNDLE_ICON_FILE "polypi.icns"
  )
endif()

if(APPLE)
  target_link_libraries(polyPi PRIVATE "-framework CoreFoundation" "-framework Cocoa" "-framework IOKit")
endif()

target_compile_definitions(polyPi PRIVATE POLYPI_VERSION="${POLYPI_VERSION}")

if(WIN32)
  # ship the dependency DLLs beside the exe so it runs outside the build tree
  add_custom_command(TARGET polyPi POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy -t $<TARGET_FILE_DIR:polyPi> $<TARGET_RUNTIME_DLLS:polyPi>
    COMMAND_EXPAND_LISTS)
endif()

# --- self-test ---
# Links the engine and the worker without the UI, so CI can actually run the
# threaded chunk-and-merge path instead of only compiling it. Off by default,
# and kept out of the app's output directory so it never lands in a release.
option(POLYPI_BUILD_TESTS "Build the engine/worker self-test" OFF)

if(POLYPI_BUILD_TESTS)
  add_executable(pi_selftest
    ${CMAKE_CURRENT_SOURCE_DIR}/../tools/test_worker.cpp
    src/pi_worker.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/pi_engine.c
  )
  target_include_directories(pi_selftest PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../core
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${GMP_INCLUDE_DIR}
  )
  target_link_libraries(pi_selftest PRIVATE ${GMP_LIBRARY} Threads::Threads)
  set_target_properties(pi_selftest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests
    RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/tests
    RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/tests
  )
  # On Windows the vcpkg toolchain's applocal deployment puts gmp.dll next to
  # each executable it builds, including this one.

  enable_testing()
  add_test(NAME selftest COMMAND pi_selftest)
endif()
