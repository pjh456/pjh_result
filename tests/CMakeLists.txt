# doctest 为可选依赖：仅当本仓库顶层构建测试时才按需拉取（FetchContent）
# 我们自带 test_main.cpp，故关掉 doctest 自带的 main 静态库，省构建
include(FetchContent)
FetchContent_Declare(doctest
    GIT_REPOSITORY https://github.com/doctest/doctest.git
    GIT_TAG v2.5.0)

set(DOCTEST_WITH_MAIN_IN_STATIC_LIB OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(doctest)

set(PJH_RESULT_TEST_SOURCES
    test_main.cpp
    test_construct.cpp
    test_unwrap.cpp
    test_combinators.cpp
    test_lifetime.cpp
    test_void.cpp
    test_macros.cpp
    test_query.cpp
    test_compare.cpp
    test_option_construct.cpp
    test_option_unwrap.cpp
    test_option_lifetime.cpp
    test_option_void.cpp
    test_option_combinators.cpp
    test_option_mutate.cpp
    test_option_interop.cpp
    test_option_compare.cpp
    test_interop.cpp
    test_context.cpp
    test_diagnostic.cpp
    test_borrow.cpp)

add_executable(pjh_result_tests ${PJH_RESULT_TEST_SOURCES})
target_link_libraries(pjh_result_tests PRIVATE pjh_result doctest)
target_include_directories(pjh_result_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

include(${doctest_SOURCE_DIR}/scripts/cmake/doctest.cmake)
doctest_discover_tests(pjh_result_tests)

# 负向编译测试：nothrow-move 约束应拒绝“移动会抛”的类型。
# 目标 EXCLUDE_FROM_ALL，仅由测试按需构建；构建失败 == 测试通过（WILL_FAIL）。
add_executable(compile_fail_throwing_move EXCLUDE_FROM_ALL compile_fail/throwing_move.cpp)
target_link_libraries(compile_fail_throwing_move PRIVATE pjh_result)
target_include_directories(compile_fail_throwing_move PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

add_test(
    NAME compile_fail.throwing_move
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target compile_fail_throwing_move)
set_tests_properties(compile_fail.throwing_move PROPERTIES WILL_FAIL TRUE)

add_executable(compile_fail_option_throwing_move EXCLUDE_FROM_ALL compile_fail/option_throwing_move.cpp)
target_link_libraries(compile_fail_option_throwing_move PRIVATE pjh_result)
target_include_directories(compile_fail_option_throwing_move PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

add_test(
    NAME compile_fail.option_throwing_move
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target compile_fail_option_throwing_move)
set_tests_properties(compile_fail.option_throwing_move PROPERTIES WILL_FAIL TRUE)

# 负向编译测试：kind() 返回 void 的类型不满足 Diagnostic concept。
add_executable(compile_fail_non_diagnostic EXCLUDE_FROM_ALL compile_fail/non_diagnostic.cpp)
target_link_libraries(compile_fail_non_diagnostic PRIVATE pjh_result)
target_include_directories(compile_fail_non_diagnostic PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

add_test(
    NAME compile_fail.non_diagnostic
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target compile_fail_non_diagnostic)
set_tests_properties(compile_fail.non_diagnostic PROPERTIES WILL_FAIL TRUE)
