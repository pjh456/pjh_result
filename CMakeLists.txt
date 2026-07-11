cmake_minimum_required(VERSION 3.20)
project(pjh_result LANGUAGES CXX)

add_library(pjh_result INTERFACE)
target_include_directories(pjh_result INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
target_compile_features(pjh_result INTERFACE cxx_std_20)

option(PJH_RESULT_BUILD_TESTS "Build tests" ${PROJECT_IS_TOP_LEVEL})
option(PJH_RESULT_BUILD_EXAMPLES "Build examples" ${PROJECT_IS_TOP_LEVEL})

if(PJH_RESULT_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(PJH_RESULT_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()
