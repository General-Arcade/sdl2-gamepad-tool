enable_testing()

# Helper function to reduce boilerplate for QtTest targets
function(add_gamepad_test TEST_NAME)
    set(options "")
    set(oneValueArgs "")
    set(multiValueArgs SOURCES INCLUDE_DIRS LINK_SDL)
    cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    add_executable(${TEST_NAME} ${ARG_SOURCES})

    target_include_directories(${TEST_NAME} PRIVATE ${ARG_INCLUDE_DIRS})

    if(QT_VERSION_MAJOR EQUAL 6)
        target_link_libraries(${TEST_NAME} PRIVATE Qt6::Core Qt6::Test)
    else()
        target_link_libraries(${TEST_NAME} PRIVATE Qt5::Core Qt5::Test)
    endif()

    if(ARG_LINK_SDL)
        if(TARGET SDL2::SDL2)
            target_link_libraries(${TEST_NAME} PRIVATE SDL2::SDL2)
        elseif(TARGET PkgConfig::SDL2)
            target_link_libraries(${TEST_NAME} PRIVATE PkgConfig::SDL2)
        endif()
    endif()

    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
endfunction()

# SDLPollEvent state transition tests
add_gamepad_test(tst_sdlpollevent
    SOURCES
        tst_sdlpollevent.cpp
        ${CMAKE_SOURCE_DIR}/src/core/sdlpollevent.cpp
        ${CMAKE_SOURCE_DIR}/src/core/sdlpollevent.h
        ${CMAKE_SOURCE_DIR}/src/core/GamepadMapper.cpp
        ${CMAKE_SOURCE_DIR}/src/core/GamepadMapper.h
        ${CMAKE_SOURCE_DIR}/src/core/GamepadGuid.cpp
        ${CMAKE_SOURCE_DIR}/src/core/GamepadGuid.h
        ${CMAKE_SOURCE_DIR}/src/core/Logger.cpp
        ${CMAKE_SOURCE_DIR}/src/core/Logger.h
    INCLUDE_DIRS
        ${CMAKE_SOURCE_DIR}/src/core
        ${CMAKE_SOURCE_DIR}/src/ui
        ${CMAKE_SOURCE_DIR}/src/network
        ${CMAKE_SOURCE_DIR}/src/platform
    LINK_SDL TRUE
)

# GamepadMapper binding logic and string generation tests
add_gamepad_test(tst_gamepadmapper
    SOURCES
        tst_gamepadmapper.cpp
        ${CMAKE_SOURCE_DIR}/src/core/GamepadMapper.cpp
        ${CMAKE_SOURCE_DIR}/src/core/GamepadMapper.h
        ${CMAKE_SOURCE_DIR}/src/core/GamepadGuid.cpp
        ${CMAKE_SOURCE_DIR}/src/core/GamepadGuid.h
    INCLUDE_DIRS
        ${CMAKE_SOURCE_DIR}/src/core
    LINK_SDL TRUE
)

# GamepadDatabase file operations tests
add_gamepad_test(tst_gamepaddatabase
    SOURCES
        tst_gamepaddatabase.cpp
        ${CMAKE_SOURCE_DIR}/src/core/GamepadDatabase.cpp
        ${CMAKE_SOURCE_DIR}/src/core/GamepadDatabase.h
        ${CMAKE_SOURCE_DIR}/src/core/GamepadMapping.cpp
        ${CMAKE_SOURCE_DIR}/src/core/GamepadMapping.h
        ${CMAKE_SOURCE_DIR}/src/core/Logger.cpp
        ${CMAKE_SOURCE_DIR}/src/core/Logger.h
    INCLUDE_DIRS
        ${CMAKE_SOURCE_DIR}/src/core
    LINK_SDL TRUE
)

# Environment interface tests
add_executable(tst_environment tst_environment.cpp
    ${CMAKE_SOURCE_DIR}/src/core/Logger.cpp
    ${CMAKE_SOURCE_DIR}/src/core/Logger.h
)

if(APPLE)
    target_sources(tst_environment PRIVATE ${CMAKE_SOURCE_DIR}/src/platform/EnvironmentMac.cpp)
elseif(WIN32)
    target_sources(tst_environment PRIVATE ${CMAKE_SOURCE_DIR}/src/platform/EnvironmentWin.cpp)
else()
    target_sources(tst_environment PRIVATE ${CMAKE_SOURCE_DIR}/src/platform/EnvironmentLinux.cpp)
endif()

target_include_directories(tst_environment PRIVATE
    ${CMAKE_SOURCE_DIR}/src/platform
    ${CMAKE_SOURCE_DIR}/src/core
)

if(QT_VERSION_MAJOR EQUAL 6)
    target_link_libraries(tst_environment PRIVATE Qt6::Core Qt6::Test)
    if(APPLE)
        target_link_libraries(tst_environment PRIVATE Qt6::Widgets)
    endif()
else()
    target_link_libraries(tst_environment PRIVATE Qt5::Core Qt5::Test)
    if(APPLE)
        target_link_libraries(tst_environment PRIVATE Qt5::Widgets)
    endif()
endif()

add_test(NAME tst_environment COMMAND tst_environment)

# Logger tests
add_gamepad_test(tst_logger
    SOURCES
        tst_logger.cpp
        ${CMAKE_SOURCE_DIR}/src/core/Logger.cpp
        ${CMAKE_SOURCE_DIR}/src/core/Logger.h
    INCLUDE_DIRS
        ${CMAKE_SOURCE_DIR}/src/core
)

# JoystickEnumerator tests
add_gamepad_test(tst_joystickenumerator
    SOURCES
        tst_joystickenumerator.cpp
        ${CMAKE_SOURCE_DIR}/src/core/JoystickEnumerator.cpp
        ${CMAKE_SOURCE_DIR}/src/core/JoystickEnumerator.h
        ${CMAKE_SOURCE_DIR}/src/core/GamepadGuid.cpp
        ${CMAKE_SOURCE_DIR}/src/core/GamepadGuid.h
    INCLUDE_DIRS
        ${CMAKE_SOURCE_DIR}/src/core
    LINK_SDL TRUE
)

# Steam config VDF parsing and atomic update tests
add_gamepad_test(tst_steamconfig
    SOURCES
        tst_steamconfig.cpp
        ${CMAKE_SOURCE_DIR}/src/core/SteamConfig.cpp
        ${CMAKE_SOURCE_DIR}/src/core/SteamConfig.h
        ${CMAKE_SOURCE_DIR}/src/core/GamepadMapping.cpp
        ${CMAKE_SOURCE_DIR}/src/core/GamepadMapping.h
    INCLUDE_DIRS
        ${CMAKE_SOURCE_DIR}/src/core
)
