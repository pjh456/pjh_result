# ponytail: plain CTest + asserts. No GoogleTest until tests outgrow asserts.
add_executable(test_smoke test_smoke.cpp)
target_link_libraries(test_smoke PRIVATE pjh_result)
add_test(NAME smoke COMMAND test_smoke)
