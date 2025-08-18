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
        tests/dummy.cpp
    )
    target_compile_features(tests PRIVATE cxx_std_20)
    target_link_libraries(tests PRIVATE
        doctest::doctest
        zug-zug::engine
    )

    doctest_discover_tests(tests)
endif()