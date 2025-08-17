cmake_minimum_required(VERSION 3.30)
project(strataGGus LANGUAGES CXX)
set(app strataGGus)
 
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(ThirdPartyDependencies)

set(sources
    src/main.cpp
    src/strataGGus/strataGGus.cpp
)

set(include_dirs
    SYSTEM
    src
)

add_executable(${app} ${sources})
target_include_directories(${app} PRIVATE ${include_dirs})

target_compile_features(${app} PRIVATE cxx_std_20)
target_link_libraries(${app} PRIVATE ${libraries})

option(ENABLE_PROFILING "Enable Profiling Flags" OFF)
message(STATUS "Profiling enabled: ${ENABLE_PROFILING}")

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    message(WARNING "CMAKE_BUILD_TYPE is not set! Defaulting to Debug")
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Build type" FORCE)
endif()

set(is_gcc "$<CXX_COMPILER_ID:GNU>")
set(is_clang "$<CXX_COMPILER_ID:Clang>")
set(is_msvc "$<OR:$<CXX_COMPILER_ID:MSVC>,$<AND:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_FRONTEND_VARIANT:MSVC>>>")

target_compile_definitions(${app} PRIVATE
    $<$<CONFIG:Debug>:DEBUG>
    $<$<NOT:$<CONFIG:Debug>>:NDEBUG>
    $<$<BOOL:${ENABLE_PROFILING}>:PROFILE_BUILD>
)

target_compile_options(${app} PRIVATE 
    # Linux
    $<$<AND:$<PLATFORM_ID:Linux>,$<CONFIG:Debug>>:
        -fno-inline
        -fstack-protector-strong
        -fno-omit-frame-pointer
        -Wall -Wextra -Wpedantic
        -Wwrite-strings
        $<${is_gcc}:-ggdb>
        $<${is_clang}:-glldb>
    >
    $<$<AND:$<BOOL:${ENABLE_PROFILING}>,$<PLATFORM_ID:Linux>>:
        -fno-omit-frame-pointer
    >

    # Windows
    $<$<AND:$<PLATFORM_ID:Windows>,$<CONFIG:Debug>,${is_msvc}>:
        /Oy-
        /W4
        /permissive-
        /Zc:strictStrings
    >
    $<$<AND:$<BOOL:${ENABLE_PROFILING}>,${is_msvc}>:
        /Oy-
        /Gh
        /GH
    >
    $<$<CXX_COMPILER_ID:MSVC>:/MP>    
)
target_link_options(${app} PRIVATE
    $<$<AND:$<PLATFORM_ID:Linux>,$<CONFIG:Debug>>:
        -rdynamic
    >
    $<$<AND:$<BOOL:${ENABLE_PROFILING}>,${is_msvc}>:
        /PROFILE
    >
)
include(Tests)
