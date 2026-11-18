cmake_minimum_required(VERSION 3.24)
project(graph LANGUAGES CXX HIP)

list(APPEND CMAKE_MODULE_PATH "$ENV{SCABBARD_PATH}")
include(scabbard)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_HIP_STANDARD 17)
set(CMAKE_HIP_STANDARD_REQUIRED ON)

# GPU target(s): override with -DCMAKE_HIP_ARCHITECTURES=gfx90a;gfx942 ...
if(NOT DEFINED CMAKE_HIP_ARCHITECTURES)
  set(CMAKE_HIP_ARCHITECTURES native)
endif()

# Default-stream flavour. OFF = legacy (one shared, implicitly-synchronising null stream),
# ON = per-thread default stream. The code is correct under both.
option(PER_THREAD_DEFAULT_STREAM "Build with the per-thread default stream" OFF)

find_package(Threads REQUIRED)

add_executable(graph graph.hip)
target_link_libraries(graph PRIVATE Threads::Threads)
target_compile_options(graph PRIVATE -O2 -g)

if(PER_THREAD_DEFAULT_STREAM)
  target_compile_definitions(graph PRIVATE HIP_API_PER_THREAD_DEFAULT_STREAM=1)
  target_compile_options(graph PRIVATE -fgpu-default-stream=per-thread)
endif()

# Managed memory shared by several GPUs/the host generally wants page-fault migration:
#   export HSA_XNACK=1   (on GPUs that support it) before running.
enable_testing()
add_test(NAME graph_1worker COMMAND graph 1)
add_test(NAME graph_4workers COMMAND graph 4)

scabbard_instrument_all()
