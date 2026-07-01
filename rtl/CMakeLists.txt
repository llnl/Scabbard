cmake_minimum_required(VERSION 3.16)

# -- Safety Pass -- used to mark all functions as not instrumentable to scabbard
add_library(safety_pass MODULE src/SafetyPass.cpp)
target_link_libraries(safety_pass PUBLIC LLVMSupport)
target_compile_features(safety_pass PRIVATE cxx_range_for cxx_auto_type)
# LLVM is (typically) built with no C++ RTTI. We need to match that;
# otherwise, we'll get linker errors about missing RTTI data.
set_target_properties(safety_pass PROPERTIES COMPILE_FLAGS "-fno-rtti")
# Get proper shared-library behavior (where symbols are not necessarily
# resolved when the shared library is linked) on OS X.
if(APPLE)
    set_target_properties(safety_pass PROPERTIES LINK_FLAGS "-undefined dynamic_lookup")
endif(APPLE)
# -- end Safety Pass --

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/default_stream_helper.cpp ${SCABBARD_PATH}/default_stream_helper.cpp)


find_package(hip REQUIRED)
# include(${HIP_PATH}/cmake/)

project(rtl VERSION 0.1.0 LANGUAGES CXX HIP)
set(CMAKE_CXX_COMPILER "$ENV{ROCM_PATH}/bin/amdclang")

configure_file(${CMAKE_SOURCE_DIR}/config/sub-version.h.config
                ./src/version.h)

add_library(rtl STATIC
    src/defs.cpp
    src/Runtime.cpp
    src/StateMachine.cpp
    src/ReportWriter.cpp
  )
add_dependencies(rtl safety_pass)
set_property(TARGET rtl PROPERTY LANGUAGE CXX)
set_property(TARGET rtl PROPERTY CXX_STANDARD 17)

target_include_directories(rtl
    PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE
    src
    ./include
    ../lib/include
  )

target_link_libraries(rtl
    PUBLIC
    $<BUILD_INTERFACE:${SCABBARD_PATH}/libscabbard.so>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_LIBDIR}/libscabbard.so>
    PRIVATE
    hip::host
    # libscabbard
  )

target_compile_options(rtl PRIVATE 
        -Wno-switch -Wno-undefined-inline
        -fgpu-rdc -flto -foffload-lto=full 
        -fpass-plugin=${SCABBARD_PATH}/libsafety_pass.so #use safety pass to mark all fn's as not to be instrumented.
      )
target_link_options(rtl PRIVATE -fgpu-rdc -flto -foffload-lto=full )

if(SCABBARD_USE_COMPRESSION)
target_link_libraries(rtl PUBLIC ZLIB::ZLIB)
target_compile_options(rtl PRIVATE "-DSCABBARD_USE_COMPRESSION=\"${SCABBARD_COMPRESSION_LEVEL}\"")
endif()


add_library(rtl.device STATIC
    src/device-defs.cpp
  )
set_property(TARGET rtl.device PROPERTY LANGUAGE HIP)
set_property(TARGET rtl.device PROPERTY HIP_STANDARD 17)
set_source_files_properties(src/device-defs.cpp PROPERTIES LANGUAGE HIP)

target_include_directories(rtl.device
  PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
  PRIVATE
  src
  ./include
  ../lib/include
)

target_link_libraries(rtl.device
  PUBLIC
  $<BUILD_INTERFACE:${SCABBARD_PATH}/libscabbard.so>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_LIBDIR}/libscabbard.so>
  PRIVATE
  hip::device
  # libscabbard
)

# set_property(TARGET rtl PROPERTY HIP_ARCHITECTURES gfx700 gfx701 gfx801 gfx803 gfx900 gfx902 gfx906
#                                                       gfx908 gfx90a gfx90c gfx940 gfx941 gfx942 gfx1010
#                                                       gfx1030 gfx1031 gfx1032 gfx1033 gfx1034 gfx1035
#                                                       gfx1036 gfx1100 gfx1101 gfx1102 gfx1103 gfx1150
#                                                       gfx1151 gfx1152)



set(all_amdgpu_architectures "gfx700;gfx701;gfx801;gfx803;gfx900;gfx902;gfx906"
                             "gfx908;gfx90a;gfx90c;gfx940;gfx941;gfx942;gfx1010"
                             "gfx1030;gfx1031;gfx1032;gfx1033;gfx1034;gfx1035"
                             "gfx1036;gfx1100;gfx1101;gfx1102;gfx1103;gfx1150"
                             "gfx1151"                                          # gfx1152 had to be removed due to required additaional specifiers
  )
set(all_nvptx_architectures "sm_35;sm_37;sm_50;sm_52;sm_53;sm_60;sm_61;sm_62"
                            "sm_70;sm_72;sm_75;sm_80;sm_86;sm_87;sm_89;sm_90"
  )
set(all_gpu_architectures
    # "${all_amdgpu_architectures};${all_nvptx_architectures}"
    "${all_amdgpu_architectures}"
  )

set(TARGET_DEVICE_ARCHITECTURES "all" CACHE STRING
  "List of device architectures to be used to compile the OpenMP DeviceRTL.")

if(TARGET_DEVICE_ARCHITECTURES STREQUAL "all")
set(TARGET_DEVICE_ARCHITECTURES ${all_gpu_architectures})
elseif(TARGET_DEVICE_ARCHITECTURES STREQUAL "auto" OR
     TARGET_DEVICE_ARCHITECTURES STREQUAL "native")
if(NOT TARGET_NVPTX_ARCH AND NOT TARGET_AMDGPU_ARCH)
  target_error_say(
    "Could not find 'amdgpu-arch' and 'nvptx-arch' tools required for 'auto'")
elseif(NOT TARGET_FOUND_NVIDIA_GPU AND NOT TARGET_FOUND_AMDGPU_GPU)
  target_error_say("No AMD or NVIDIA GPU found on the system when using 'auto'")
endif()
set(TARGET_DEVICE_ARCHITECTURES
    "${TARGET_NVPTX_DETECTED_ARCH_LIST};${TARGET_AMDGPU_DETECTED_ARCH_LIST}")
endif()
list(REMOVE_DUPLICATES TARGET_DEVICE_ARCHITECTURES)

# list(JOIN TARGET_DEVICE_ARCHITECTURES " " KERNEL_OFFLOAD_ARCHS)
# set_property(TARGET rtl PROPERTY HIP_ARCHITECTURES TARGET_DEVICE_ARCHITECTURES)


list(JOIN TARGET_DEVICE_ARCHITECTURES "," KERNEL_OFFLOAD_ARCHS)
target_compile_options(rtl.device PRIVATE -x hip --offload-arch=${KERNEL_OFFLOAD_ARCHS} -foffload-lto -fgpu-rdc -Wno-undefined-inline)
target_link_options(rtl.device PRIVATE -x hip --offload-arch=${KERNEL_OFFLOAD_ARCHS} -foffload-lto -fgpu-rdc ) # not linking -> not needed



# set(LIBTRACE_DEVICE_PATH_IR_TMP ${CMAKE_CURRENT_BINARY_DIR}/rtl-device.o.tmp)
# set(LIBTRACE_DEVICE_PATH_IR ${SCABBARD_PATH}/rtl-device.a)

# add_custom_target(rtl-device
#     ${ROCM_PATH}/bin/hipcc ${CMAKE_CXX_FLAGS} -g -x hip --offload-arch=${KERNEL_OFFLOAD_ARCHS} -std=c++17 -I../lib/include -I./include -foffload-lto -fgpu-rdc -c -o ${LIBTRACE_DEVICE_PATH_IR_TMP} ./src/device-defs.cpp
#     COMMAND ar r ${LIBTRACE_DEVICE_PATH_IR} ${LIBTRACE_DEVICE_PATH_IR_TMP}
#     # COMMAND ${LLVM_HOME}/bin/clang-offload-bundler -type=o 
#     # COMMAND awk '/\; __CLANG_OFFLOAD_BUNDLE____START__ host.+/{p=1} /\; __CLANG_OFFLOAD_BUNDLE____END__ host.+/{p=0\; next}!p' ${LIBTRACE_DEVICE_PATH_IR_TMP} > ${LIBTRACE_DEVICE_PATH_IR}
#     WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
#     DEPENDS ./src/device-defs.cpp 
#             ./include/scabbard/rtl/globals.hpp 
#             ./include/scabbard/rtl/calls.hpp 
#             ./include/scabbard/rtl/AsyncQueue.hpp 
#             ../lib/include/scabbard/TraceData.hpp 
#             ../lib/include/scabbard/Enums.hpp
#     SOURCES ./src/device-defs.cpp 
#             ./include/scabbard/rtl/globals.hpp 
#             ./include/scabbard/rtl/calls.hpp 
#             ./include/scabbard/rtl/AsyncQueue.hpp 
#             ../lib/include/scabbard/TraceData.hpp 
#             ../lib/include/scabbard/Enums.hpp
#     BYPRODUCTS ${LIBTRACE_DEVICE_PATH_IR}
#                ${LIBTRACE_DEVICE_PATH_IR_TMP}
#   )


# set(LIBTRACE_DEVICE_PATH_IR_TMP ${CMAKE_CURRENT_BINARY_DIR}/rtl-device.ll.tmp)
# set(LIBTRACE_DEVICE_PATH_IR ${SCABBARD_PATH}/rtl-device.ll)

# add_custom_target(rtl-device
#     ${ROCM_PATH}/bin/hipcc ${CMAKE_CXX_FLAGS} -g -x hip -std=c++17 -I../lib/include -I./include -S -emit-llvm -o ${LIBTRACE_DEVICE_PATH_IR_TMP} ./src/device-defs.cpp
#     COMMAND awk '/\; __CLANG_OFFLOAD_BUNDLE____START__ host.+/{p=1} /\; __CLANG_OFFLOAD_BUNDLE____END__ host.+/{p=0\; next}!p' ${LIBTRACE_DEVICE_PATH_IR_TMP} > ${LIBTRACE_DEVICE_PATH_IR}
#     WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
#     DEPENDS ./src/device-defs.cpp 
#             ./include/scabbard/rtl/globals.hpp 
#             ./include/scabbard/rtl/calls.hpp 
#             ./include/scabbard/rtl/AsyncQueue.hpp 
#             ../lib/include/scabbard/TraceData.hpp 
#             ../lib/include/scabbard/Enums.hpp
#     SOURCES ./src/device-defs.cpp 
#             ./include/scabbard/rtl/globals.hpp 
#             ./include/scabbard/rtl/calls.hpp 
#             ./include/scabbard/rtl/AsyncQueue.hpp 
#             ../lib/include/scabbard/TraceData.hpp 
#             ../lib/include/scabbard/Enums.hpp
#     BYPRODUCTS ${LIBTRACE_DEVICE_PATH_IR}
#                ${LIBTRACE_DEVICE_PATH_IR_TMP}
#   )

# add_dependencies(rtl rtl-device) # uncomment to auto build the manually linked GPU module



install(TARGETS rtl rtl.device EXPORT libtraceConfig
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  )
  

# this is an llvm instr only lib, there are no include files
# install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

export(TARGETS rtl rtl.device FILE libtraceConfig.cmake)
