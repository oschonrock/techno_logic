if(BUILD_TESTING)
    fetchcontent_declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.15.2
    )
    fetchcontent_makeavailable(googletest)
    include(CTest)
    add_executable(tests test.cpp)
    target_link_libraries(tests PRIVATE GTest::gtest_main techno_logic_internal)
    target_compile_options(tests PRIVATE ${PROJECT_COMPILE_OPTIONS})

    include(GoogleTest)
    gtest_discover_tests(tests)
endif()
