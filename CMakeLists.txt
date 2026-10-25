cmake_minimum_required(VERSION 3.16)

# Minimum macOS version (must be set before project()). Without it the binary
# targets the build machine's SDK version and won't launch on older macOS.
# 13.0 matches the minimum supported by Qt 6.
if(NOT CMAKE_OSX_DEPLOYMENT_TARGET)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "13.0" CACHE STRING "Minimum macOS version" FORCE)
endif()

project(gamepad-tool VERSION 1.4.2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# macOS universal binary (Intel + Apple Silicon)
if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64" CACHE STRING "Build universal binary")
endif()

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# Try Qt6 first, fall back to Qt5
find_package(Qt6 QUIET COMPONENTS Core Gui Widgets Network Test)
if(Qt6_FOUND)
    set(QT_VERSION_MAJOR 6)
    message(STATUS "Using Qt ${Qt6_VERSION}")
else()
    find_package(Qt5 REQUIRED COMPONENTS Core Gui Widgets Network Test)
    set(QT_VERSION_MAJOR 5)
    message(STATUS "Using Qt ${Qt5_VERSION}")
endif()

# SDL2: prefer system install, fall back to building from source.
# When building a universal binary, skip system SDL2 (likely single-arch from
# Homebrew) and build from source so both architectures are covered.
list(LENGTH CMAKE_OSX_ARCHITECTURES _osx_arch_count)
if(_osx_arch_count LESS 2)
    find_package(SDL2 QUIET)
    if(NOT SDL2_FOUND)
        find_package(PkgConfig QUIET)
        if(PkgConfig_FOUND)
            pkg_check_modules(SDL2 QUIET IMPORTED_TARGET sdl2)
        endif()
    endif()
endif()
if(NOT SDL2_FOUND AND NOT TARGET PkgConfig::SDL2)
    include(FetchContent)
    set(SDL2_DISABLE_INSTALL ON CACHE BOOL "" FORCE)
    set(SDL_TEST OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
        SDL2
        URL https://github.com/libsdl-org/SDL/releases/download/release-2.32.10/SDL2-2.32.10.tar.gz
        URL_HASH SHA256=5f5993c530f084535c65a6879e9b26ad441169b3e25d789d83287040a9ca5165
    )
    FetchContent_MakeAvailable(SDL2)
    message(STATUS "SDL2 built from source via FetchContent")
endif()

add_subdirectory(src)

enable_testing()
add_subdirectory(tests)

# CPack configuration for Linux packages
if(UNIX AND NOT APPLE)
    set(CPACK_PACKAGE_NAME "gamepad-tool")
    set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Simple GUI tool to create and modify gamepad mappings for SDL2")
    set(CPACK_PACKAGE_VENDOR "General Arcade")
    set(CPACK_PACKAGE_CONTACT "info@generalarcade.com")
    set(CPACK_PACKAGE_HOMEPAGE_URL "https://generalarcade.com/gamepadtool/")

    # DEB
    set(CPACK_DEBIAN_PACKAGE_DEPENDS "libqt5widgets5, libqt5network5, libsdl2-2.0-0")
    set(CPACK_DEBIAN_PACKAGE_SECTION "utils")

    # TGZ
    set(CPACK_GENERATOR "DEB;TGZ")
    include(CPack)
endif()
