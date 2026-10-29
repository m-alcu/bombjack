cmake_minimum_required(VERSION 3.16)
project(bombjack CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(bombjack
    src/main.cpp
    src/debug_tools.cpp
    src/game/game.cpp
    src/game/round.cpp
    src/game/player.cpp
    src/game/enemy.cpp
    src/game/enemy_anim.cpp
    src/game/powerball.cpp
    src/game/bonus.cpp
    src/game/effects.cpp
    src/game/hiscore.cpp
    src/game/attract.cpp
    src/render/gfx.cpp
    src/render/textures.cpp
    src/render/background.cpp
    src/render/palette.cpp
    src/render/world.cpp
    src/render/hud.cpp
    src/render/screens.cpp
    src/render/render.cpp
    src/platform/audio.cpp
    src/platform/input.cpp
)
target_include_directories(bombjack PRIVATE src)

# Absolute path to the repo so the game can find assets/sounds/ regardless of
# the working directory it is launched from.
target_compile_definitions(bombjack PRIVATE ASSET_ROOT="${CMAKE_SOURCE_DIR}")

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
