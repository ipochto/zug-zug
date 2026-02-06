include(CTest)

if(BUILD_TESTING)
    find_package(doctest CONFIG QUIET)
    if (NOT doctest_FOUND)
        message(STATUS "doctest not found, fetching with FetchContent...")
        FetchContent_Declare(
            doctest
            GIT_REPOSITORY https://github.com/doctest/doctest.git
            GIT_TAG        v2.4.12
        )
        FetchContent_MakeAvailable(doctest)
        set(CMAKE_MODULE_PATH
            ${CMAKE_MODULE_PATH}
            "${doctest_SOURCE_DIR}/scripts/cmake"
        )
    endif()

    include(doctest)

    add_executable(tests
        tests/main.cpp
        tests/zug-zug/scripts/lua/test_limitedAlloc.cpp
        tests/zug-zug/scripts/lua/test_sandbox_libs.cpp
        tests/zug-zug/scripts/lua/test_sandbox_fs.cpp
        tests/zug-zug/scripts/lua/test_timeoutGuard.cpp
        tests/utils/test_filesystem.cpp
    )
    target_compile_features(tests PRIVATE cxx_std_20)
    target_link_libraries(tests PRIVATE
        doctest::doctest
        zug-zug::engine
    )
    target_compile_options(tests PRIVATE
        $<$<AND:$<BOOL:${ENABLE_COVERAGE}>,$<PLATFORM_ID:Linux>>:
            $<${is_gcc}: --coverage>
            $<${is_clang}: -fprofile-instr-generate -fcoverage-mapping>
        >
    )
    target_link_options(tests PRIVATE
        $<$<AND:$<BOOL:${ENABLE_COVERAGE}>,$<PLATFORM_ID:Linux>>:
            $<${is_gcc}: --coverage>
            $<${is_clang}: -fprofile-instr-generate>
        >
    )
    doctest_discover_tests(tests)
endif()