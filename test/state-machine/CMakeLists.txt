
create_unit_test(sm _ legacy)
create_unit_test_pDSTy(sm scenarios)
create_unit_test_pDSTy(sm stress)


# ---------------------------------------------------------------------------------------------- 

# add_executable(test_sm
#     test_sm.cpp
#     ${SCABBARD_PATH}/default_stream_helper.c
#   )

# target_link_libraries(test_sm
#     PUBLIC
#     rtl
#     GTest::gtest_main
#     hip::host
#   )

# target_include_directories(test_sm
#     PUBLIC
#     $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../rtl/include>
#     $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../lib/include>
#     $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
#     PRIVATE
#     hip::host
#   )

# target_compile_options(test_sm PUBLIC -g -fgpu-default-stream=${DEFAULT_STREAM_TYPE})

# add_dependencies(test_sm rtl)


# # ---------------------------------------------------------------------------------------------- 

# add_executable(test_sm_scenarios
#     test_sm_scenarios.cpp
#     ${SCABBARD_PATH}/default_stream_helper.c
#   )

# target_link_libraries(test_sm_scenarios
#     PUBLIC
#     rtl
#     GTest::gtest_main
#     hip::host
#   )

# target_include_directories(test_sm_scenarios
#     PUBLIC
#     $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../rtl/include>
#     $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../lib/include>
#     $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
#     PRIVATE
#     hip::host
#   )

# target_compile_options(test_sm_scenarios PUBLIC -g -fgpu-default-stream=${DEFAULT_STREAM_TYPE})

# add_dependencies(test_sm_scenarios rtl)

# # ---------------------------------------------------------------------------------------------- 

# add_executable(test_sm_stress
#     test_sm_stress.cpp
#     ${SCABBARD_PATH}/default_stream_helper.c
#   )

# target_link_libraries(test_sm_stress
#     PUBLIC
#     rtl
#     GTest::gtest_main
#     hip::host
#   )

# target_include_directories(test_sm_stress
#     PUBLIC
#     $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../rtl/include>
#     $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../lib/include>
#     $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
#     PRIVATE
#     hip::host
#   )

# target_compile_options(test_sm_stress PUBLIC -g -fgpu-default-stream=${DEFAULT_STREAM_TYPE})

# add_dependencies(test_sm_stress rtl)

# ---------------------------------------------------------------------------------------------- 
