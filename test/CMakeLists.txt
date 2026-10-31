

set(GTEST_RELEASE_HASH 063de7e9578f82b369302001269680b4b1553359)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
set(DOWNLOAD_EXTRACT_TIMESTAMP true)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/${GTEST_RELEASE_HASH}.zip)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)


find_package(hip REQUIRED)
# include(${HIP_PATH}/cmake/)
set(CMAKE_CXX_COMPILER "$ENV{ROCM_PATH}/bin/amdclang++")

enable_testing()

list(APPEND CMAKE_CXX_IMPLICIT_INCLUDE_DIRECTORIES "${CMAKE_BUILD_DIR}/_deps/googletest-src/googletest/include")

set(MY_GTESTS test_all_gtests test_all_gtests_pt)
set(MY_GTEST_MAIN_CPP "${CMAKE_CURRENT_SOURCE_DIR}/GTestMain.cpp")
set(MY_GTEST_FILES ${MY_GTEST_MAIN_CPP})

function(__create_unit_test_recipe tName DEFAULT_STREAM_TYPE)
  add_executable(${tName}
      ${ARGN} # all arguments after the last expected one
      ${SCABBARD_PATH}/default_stream_helper.c
      ${MY_GTEST_MAIN_CPP}
    )

  target_link_libraries(${tName}
      PUBLIC
      rtl
      GTest::gtest_main
      hip::host
    )

  target_include_directories(${tName}
      PUBLIC
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../rtl/include>
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../lib/include>
      $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
      PRIVATE
      hip::host
    )
  
    target_compile_options(${tName} PUBLIC -g -fgpu-default-stream=${DEFAULT_STREAM_TYPE})
  if(DEFAULT_STREAM_TYPE MATCHES "per-thread")
    target_compile_options(${tName} PUBLIC -DHIP_API_PER_THREAD_DEFAULT_STREAM)
  endif()

  add_dependencies(${tName} rtl)
endfunction()

function(create_unit_test SUIT_NAME SUIT_VARIANT DEFAULT_STREAM_TYPE)
  if(SUIT_VARIANT STREQUAL "_")
    set(fName "test_${SUIT_NAME}")
  else()
    set(fName "test_${SUIT_NAME}_${SUIT_VARIANT}")
  endif()
  if (DEFAULT_STREAM_TYPE MATCHES "legacy")
    set(tName "${fName}")
  else()
    set(tName "${fName}_pt")
  endif()

  __create_unit_test_recipe(${tName} ${DEFAULT_STREAM_TYPE} "${fName}.cpp")

  list(APPEND MY_GTEST_FILES "${fName}.cpp")
  list(APPEND MY_GTESTS ${tName})
endfunction()

macro(create_unit_test_pDSTy SUIT_NAME SUIT_VARIANT)
  create_unit_test(${SUIT_NAME} ${SUIT_VARIANT} "legacy")
  create_unit_test(${SUIT_NAME} ${SUIT_VARIANT} "per-thread")
endmacro()


# ---------------------------------------------------------------------------------------------- 


add_subdirectory("state-machine")



# ---------------------------------------------------------------------------------------------- 

__create_unit_test_recipe(test_all_gtests "legacy" ${MY_GTEST_FILES})
__create_unit_test_recipe(test_all_gtests_pt "per-thread" ${MY_GTEST_FILES})

include(GoogleTest)

foreach(t IN LISTS MY_GTESTS)
  gtest_discover_tests(${t})
endforeach()
