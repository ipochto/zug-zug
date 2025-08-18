cmake_minimum_required(VERSION 3.30)
project(zug-zug LANGUAGES CXX)
set(app zug-zug)
set(engine_lib engine)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    message(WARNING "CMAKE_BUILD_TYPE is not set! Defaulting to Debug")
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Build type" FORCE)
endif()

option(ENABLE_PROFILING "Enable Profiling Flags" OFF)
message(STATUS "Profiling enabled: ${ENABLE_PROFILING}")

include(ThirdPartyDependencies)

set(engine_sources
    src/zug-zug/zug-zug.cpp
)
set(engine_headers
    src/scripts/sol2.hpp
    src/zug-zug/zug-zug.hpp
    src/utils/filesystem.hpp
)

set(engine_include_dirs
    SYSTEM
    src
)

set(engine_libraries
    cxxopts::cxxopts
    fmt::fmt
    spdlog::spdlog
    lua::lua51
    sol2::sol2
)

add_library(${engine_lib} STATIC)
target_sources(${engine_lib} PRIVATE
    ${engine_sources} 
    ${engine_headers}
)
target_include_directories(${engine_lib} PUBLIC ${engine_include_dirs})

target_compile_features(${engine_lib} PRIVATE cxx_std_20)
target_link_libraries(${engine_lib} PUBLIC ${engine_libraries})

set(is_gcc "$<CXX_COMPILER_ID:GNU>")
set(is_clang "$<CXX_COMPILER_ID:Clang>")
set(is_msvc "$<OR:$<CXX_COMPILER_ID:MSVC>,$<AND:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_FRONTEND_VARIANT:MSVC>>>")

target_compile_definitions(${engine_lib} PUBLIC
    $<$<CONFIG:Debug>:DEBUG>
    $<$<NOT:$<CONFIG:Debug>>:NDEBUG>
    $<$<BOOL:${ENABLE_PROFILING}>:PROFILE_BUILD>
)

target_compile_options(${engine_lib} PRIVATE 
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
target_link_options(${engine_lib} PRIVATE
    $<$<AND:$<PLATFORM_ID:Linux>,$<CONFIG:Debug>>:
        -rdynamic
    >
    $<$<AND:$<BOOL:${ENABLE_PROFILING}>,${is_msvc}>:
        /PROFILE
    >
)
add_library(zug-zug::engine ALIAS ${engine_lib})

add_executable(${app} src/main.cpp)
target_include_directories(${app} PRIVATE ${engine_include_dirs})
target_link_libraries(${app} PRIVATE zug-zug::engine)

include(Tests)
