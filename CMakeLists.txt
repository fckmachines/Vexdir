cmake_minimum_required(VERSION 3.20)
project(Vexdir VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(FTXUI_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(FTXUI_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(FTXUI_BUILD_TESTS OFF CACHE BOOL "" FORCE)

include(FetchContent)
FetchContent_Declare(
  ftxui
  GIT_REPOSITORY https://github.com/ArthurSonzogni/FTXUI.git
  GIT_TAG v7.0.3
  GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(ftxui)

add_executable(vexdir src/main.cpp)
target_link_libraries(vexdir PRIVATE ftxui::ftxui)

if(MSVC)
  target_compile_options(vexdir PRIVATE /W4 /utf-8)
else()
  target_compile_options(vexdir PRIVATE -Wall -Wextra -Wpedantic)
endif()
