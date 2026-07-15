cmake_minimum_required(VERSION 3.16)
project(instr VERSION 0.1.0)

configure_file(${CMAKE_SOURCE_DIR}/config/sub-version.h.config
                ./src/version.h)

add_library(instr MODULE
    src/ScabbardPass.cpp
    src/registry.cpp
  )
add_dependencies(instr rtl rtl.device)

# add_library(testpass MODULE
#     src/TestPass/TestPass.cpp
#   )

# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti")
# uncomment below to do full datarace checkign between CPU and GPU
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D__SCABBARD_TRACE_HOST_WRITE_TO_GPU_READ")

# add lib instr
target_include_directories(instr
    PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE
    src
    ./include
    ../lib/include
  )

target_link_libraries(instr
    PUBLIC
    LLVMSupport
    $<BUILD_INTERFACE:${SCABBARD_PATH}/libscabbard.so>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_LIBDIR}/libscabbard.so>
  )

target_compile_features(instr PRIVATE cxx_range_for cxx_auto_type)

# LLVM is (typically) built with no C++ RTTI. We need to match that;
# otherwise, we'll get linker errors about missing RTTI data.
set_target_properties(instr PROPERTIES
    COMPILE_FLAGS "-fno-rtti"
  )

# Get proper shared-library behavior (where symbols are not necessarily
# resolved when the shared library is linked) on OS X.
if(APPLE)
    set_target_properties(instr PROPERTIES
        LINK_FLAGS "-undefined dynamic_lookup"
      )
endif(APPLE)
