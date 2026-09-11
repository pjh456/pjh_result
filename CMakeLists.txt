cmake_minimum_required(VERSION 3.20)
project(pjh_result VERSION 0.1.0 LANGUAGES CXX)

add_library(pjh_result INTERFACE)
add_library(pjh::result ALIAS pjh_result)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

target_include_directories(pjh_result INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_compile_features(pjh_result INTERFACE cxx_std_20)

# CMake < 3.21 无 PROJECT_IS_TOP_LEVEL；用版本无关惯用式判定顶层工程
if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    set(PJH_RESULT_IS_TOP_LEVEL ON)
else()
    set(PJH_RESULT_IS_TOP_LEVEL OFF)
endif()

option(PJH_RESULT_BUILD_TESTS "Build tests" ${PJH_RESULT_IS_TOP_LEVEL})
option(PJH_RESULT_BUILD_EXAMPLES "Build examples" ${PJH_RESULT_IS_TOP_LEVEL})

if(PJH_RESULT_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(PJH_RESULT_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

set(pjh_result_INSTALL_CMAKEDIR ${CMAKE_INSTALL_LIBDIR}/cmake/pjh_result)

install(TARGETS pjh_result EXPORT pjh_resultTargets)
install(EXPORT pjh_resultTargets
    FILE pjh_resultTargets.cmake
    DESTINATION ${pjh_result_INSTALL_CMAKEDIR})
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

configure_package_config_file(cmake/pjh_resultConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/pjh_resultConfig.cmake
    INSTALL_DESTINATION ${pjh_result_INSTALL_CMAKEDIR})
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/pjh_resultConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion)
install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/pjh_resultConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/pjh_resultConfigVersion.cmake
    DESTINATION ${pjh_result_INSTALL_CMAKEDIR})
