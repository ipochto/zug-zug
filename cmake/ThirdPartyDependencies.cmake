include(FetchContent)

find_package(cxxopts CONFIG QUIET)
if (NOT cxxopts_FOUND)
    message(STATUS "cxxopts not found, fetching with FetchContent...")
    FetchContent_Declare(
        cxxopts
        GIT_REPOSITORY https://github.com/jarro2783/cxxopts.git
        GIT_TAG        v3.3.1
    )
    FetchContent_MakeAvailable(cxxopts)
endif()

find_package(fmt CONFIG QUIET)
if (NOT fmt_FOUND)
    message(STATUS "fmt not found, fetching with FetchContent...")
    FetchContent_Declare(
        fmt
        GIT_REPOSITORY https://github.com/fmtlib/fmt.git
        GIT_TAG        11.2.0
    )
    FetchContent_MakeAvailable(fmt)
endif()

find_package(spdlog CONFIG QUIET)
if (NOT spdlog_FOUND)
    message(STATUS "spdlog not found, fetching with FetchContent...")
    FetchContent_Declare(
        spdlog
        GIT_REPOSITORY https://github.com/gabime/spdlog.git
        GIT_TAG        v1.15.3
    )
    FetchContent_MakeAvailable(spdlog)
endif()

find_package(Lua 5.1 EXACT QUIET)
if (Lua_FOUND)
    add_library(lua::lua51 UNKNOWN IMPORTED)
    set_target_properties(lua::lua51 PROPERTIES
        IMPORTED_LOCATION             "${LUA_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${LUA_INCLUDE_DIR}"
    )
else()
    message(STATUS "Lua not found, fetching with FetchContent...")
    FetchContent_Declare(
        lua51
        URL https://www.lua.org/ftp/lua-5.1.5.tar.gz
        URL_HASH SHA256=2640fc56a795f29d28ef15e13c34a47e223960b0240e8cb0a82d9b0738695333
        PATCH_COMMAND ${CMAKE_COMMAND} -E copy
            "${CMAKE_SOURCE_DIR}/cmake/third-party/lua51-CMakeLists.txt"
            "<SOURCE_DIR>/CMakeLists.txt"        
    )
    FetchContent_MakeAvailable(lua51)
endif()

FetchContent_Declare(
  sol2
  GIT_REPOSITORY https://github.com/ThePhD/sol2.git
  GIT_TAG        v3.5.0
)
FetchContent_MakeAvailable(sol2)

set(libraries
    cxxopts::cxxopts
    fmt::fmt
    spdlog::spdlog
    lua::lua51
    sol2::sol2
)