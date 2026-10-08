

find_package(hip REQUIRED)
# include(${HIP_PATH}/cmake/)

project(sm_test VERSION 0.1.0 LANGUAGES CXX HIP)
set(CMAKE_CXX_COMPILER "$ENV{ROCM_PATH}/bin/amdclang")

add_executable(test_sm
    test_sm.cpp
  )

target_link_libraries(test_sm
    PUBLIC
    rtl
    GTest::gtest_main    
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

include(GoogleTest)
gtest_discover_tests(test_sm)
