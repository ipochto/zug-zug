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

find_package(doctest CONFIG QUIET)
if (NOT doctest_FOUND)
    message(STATUS "doctest not found, fetching with FetchContent...")
    FetchContent_Declare(
        doctest
        GIT_REPOSITORY https://github.com/doctest/doctest.git 
        GIT_TAG        v2.4.12
    )
    FetchContent_MakeAvailable(doctest)
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
        GIT_TAG v1.15.3
    )
    FetchContent_MakeAvailable(spdlog)
endif()

set(libraries
    cxxopts::cxxopts
    doctest::doctest
    fmt::fmt
    spdlog::spdlog)