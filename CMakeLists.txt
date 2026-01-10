cmake_minimum_required(VERSION 3.20)

project(p2_screenshot LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(p2_lib
  src/path_utils.cpp
  src/time_utils.cpp
  src/display_enum.cpp
  src/capture_dxgi.cpp
  src/capture_gdi.cpp
  src/encode_wic.cpp
  src/logging.cpp
  src/process_utils.cpp
  src/win_helpers.cpp
)

target_include_directories(p2_lib PUBLIC src)

target_compile_definitions(p2_lib PUBLIC NOMINMAX _WIN32_WINNT=0x0A00)

target_compile_options(p2_lib PUBLIC
  $<$<CXX_COMPILER_ID:MSVC>:/W4 /permissive- /utf-8>
)

target_link_libraries(p2_lib PUBLIC
  d3d11
  dxgi
  windowscodecs
  ole32
  user32
  gdi32
)

add_executable(p2_screenshot src/main.cpp)
target_link_libraries(p2_screenshot PRIVATE p2_lib)

enable_testing()

add_executable(p2_tests
  tests/test_main.cpp
)
target_link_libraries(p2_tests PRIVATE p2_lib)
add_test(NAME unit_integration COMMAND p2_tests)

add_executable(p2_e2e
  tests/e2e_smoke.cpp
)
target_link_libraries(p2_e2e PRIVATE p2_lib)
add_test(NAME e2e_smoke COMMAND p2_e2e $<TARGET_FILE:p2_screenshot>)
