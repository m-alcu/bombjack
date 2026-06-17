cmake_minimum_required(VERSION 3.16)
project(bombjack CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(bombjack src/main.cpp)

# Link SDL3. Prefer pkg-config (reliable on Linux); fall back to the SDL3 CMake
# config package (common on macOS/vcpkg/Windows).
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(SDL3 IMPORTED_TARGET sdl3)
endif()

if(TARGET PkgConfig::SDL3)
    target_link_libraries(bombjack PRIVATE PkgConfig::SDL3)
else()
    find_package(SDL3 REQUIRED CONFIG)
    target_link_libraries(bombjack PRIVATE SDL3::SDL3)
endif()

if(NOT MSVC)
    target_compile_options(bombjack PRIVATE -Wall -Wextra)
endif()
