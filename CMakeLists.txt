cmake_minimum_required(VERSION 3.20)
project(Vexdir VERSION 0.1.2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Link the Microsoft C/C++ runtime into vexdir.exe: no MSVCP140.dll or VCRUNTIME140.dll.
if(MSVC)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

add_executable(vexdir src/main.cpp)
if(MSVC)
  target_compile_options(vexdir PRIVATE /W4 /utf-8)
  target_compile_definitions(vexdir PRIVATE UNICODE _UNICODE)
else()
  target_compile_options(vexdir PRIVATE -Wall -Wextra -Wpedantic)
endif()
