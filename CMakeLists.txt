cmake_minimum_required(VERSION 3.20)
project(PD2ExplorationAutomap VERSION 0.2.0 LANGUAGES CXX)
set(PD2_RELEASE_LABEL "0.2.0-beta.5")

# The game-facing hooks use MSVC inline assembly and the 32-bit Windows ABI.
if(NOT WIN32 OR NOT MSVC)
    message(FATAL_ERROR "The runtime requires Windows and the MSVC x86 compiler.")
endif()
if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
    message(FATAL_ERROR "Configure for 32-bit Windows: use a Visual Studio generator with -A Win32.")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
# Link the MSVC runtime statically so the plugin needs no separate CRT package.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

function(automap_options target)
    target_include_directories(${target} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_compile_options(${target} PRIVATE /W4 /WX /EHsc)
endfunction()

add_library(ExplorationMask SHARED src/ExplorationRuntime.cpp)
automap_options(ExplorationMask)

# Default tests use synthetic data and mocked callbacks, without game assets.
include(CTest)
if(BUILD_TESTING)
    foreach(test IN ITEMS mask_tests runtime_tests styled_tests)
        add_executable(${test} tests/${test}.cpp)
        automap_options(${test})
        add_test(NAME ${test} COMMAND ${test})
        set_tests_properties(${test} PROPERTIES TIMEOUT 120)
    endforeach()
endif()
