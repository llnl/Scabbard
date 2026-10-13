

find_package(hip REQUIRED)
# include(${HIP_PATH}/cmake/)

project(sm_test VERSION 0.1.0 LANGUAGES CXX HIP)
set(CMAKE_CXX_COMPILER "$ENV{ROCM_PATH}/bin/amdclang++")


list(APPEND CMAKE_CXX_IMPLICIT_INCLUDE_DIRECTORIES "${CMAKE_BUILD_DIR}/_deps/googletest-src/googletest/include")

# ---------------------------------------------------------------------------------------------- 

add_executable(test_sm
    test_sm.cpp
    ${SCABBARD_PATH}/default_stream_helper.c
  )

target_link_libraries(test_sm
    PUBLIC
    rtl
    GTest::gtest_main
    hip::host
  )

target_include_directories(test_sm
    PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../rtl/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../lib/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE
    hip::host
  )

add_dependencies(test_sm rtl)


# ---------------------------------------------------------------------------------------------- 

add_executable(test_sm_scenarios
    test_sm_scenarios.cpp
    ${SCABBARD_PATH}/default_stream_helper.c
  )

target_link_libraries(test_sm_scenarios
    PUBLIC
    rtl
    GTest::gtest_main
    hip::host
  )

target_include_directories(test_sm_scenarios
    PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../rtl/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../lib/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE
    hip::host
  )

add_dependencies(test_sm_scenarios rtl)

# ---------------------------------------------------------------------------------------------- 

include(GoogleTest)
gtest_discover_tests(test_sm)
gtest_discover_tests(test_sm_scenarios)
