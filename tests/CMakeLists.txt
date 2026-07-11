# doctest 经 git submodule 引入（tests/third_party/doctest），离线可控
# 我们自带 test_main.cpp，故关掉 doctest 自带的 main 静态库，省构建
set(DOCTEST_WITH_MAIN_IN_STATIC_LIB OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/doctest)

set(PJH_RESULT_TEST_SOURCES
    test_main.cpp
    test_construct.cpp
    test_unwrap.cpp
    test_combinators.cpp
    test_lifetime.cpp
    test_void.cpp
    test_macros.cpp)

add_executable(pjh_result_tests ${PJH_RESULT_TEST_SOURCES})
target_link_libraries(pjh_result_tests PRIVATE pjh_result doctest)
target_include_directories(pjh_result_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

include(${CMAKE_CURRENT_SOURCE_DIR}/third_party/doctest/scripts/cmake/doctest.cmake)
doctest_discover_tests(pjh_result_tests)
