cmake_minimum_required(VERSION 3.20)
project(scabbard VERSION 0.3.0)

if(NOT DEFINED CMAKE_INSTALL_PREFIX)
    include(GNUInstallDirs)
    message("[NOTE] CMAKE_INSTALL_PREFIX will be configured using GNIInstallDirs")
else()
    if(CMAKE_INSTALL_PREFIX MATCHES "/usr/local")
        set(CMAKE_INSTALL_PREFIX "/usr/local/scabbard" CACHE FILEPATH "don't drop all scabbard files into the top level of /urs/local" FORCE)
    endif()
    message("[NOTE] Scabbard will install to CMAKE_INSTALL_PREFIX:\"${CMAKE_INSTALL_PREFIX}\"")
endif()
configure_file(${CMAKE_SOURCE_DIR}/config/version.h.config
                ${CMAKE_SOURCE_DIR}/lib/include/scabbard/version.h)

# tmp build with debug llvm-project on external drive
# set(ENV{LLVM_HOME} /mnt/d/repos/llvm-project/build)

# we need rocm and hip paths to be defined
if (DEFINED ENV{ROCM_PATH})
    set(ENV{LLVM_HOME} $ENV{ROCM_PATH}/llvm)
    if (EXISTS "$ENV{ROCM_PATH}/hip")
        set(ENV{HIP_PATH} "$ENV{ROCM_PATH}/hip")
    endif ()
    message("[NOTE] $ROCM_PATH was found to be \"$ENV{ROCM_PATH}\"")
elseif (EXISTS "/opt/rocm") # if both are not defined define them if possible
    set(ENV{ROCM_PATH} "/opt/rocm")
    set(ENV{LLVM_HOME} "$ENV{ROCM_PATH}/llvm")
    if (NOT DEFINED ENV{HIP_PATH} AND EXISTS "$ENV{ROCM_PATH}/hip")
        set(ENV{HIP_PATH} "$ENV{ROCM_PATH}/hip")
    endif ()
else ()
    message(FATAL_ERROR "[ERROR] hip/rocm could not be found try defining $HIP_PATH and $ROCM_PATH\n")
endif()
list(APPEND CMAKE_PREFIX_PATH $ENV{ROCM_PATH} $ENV{LLVM_HOME})
if (DEFINED ENV{HIP_PATH})
    list(APPEND CMAKE_PREFIX_PATH $ENV{HIP_PATH})
    set(ENV{HIP_PATH} "$ENV{ROCM_PATH}/hip")
endif ()
if (NOT DEFINED ENV{ROCM_DIR})
    set(ENV{ROCM_DIR} $ENV{ROCM_PATH}/lib/cmake)
endif()
if (NOT DEFINED ENV{HIP_DIR})
    set(ENV{HIP_DIR} $ENV{ROCM_DIR})
endif()

# we need LLVM_HOME in order to automatically set LLVM_DIR
if(NOT DEFINED ENV{LLVM_HOME})
    message(WARNING "$LLVM_HOME was not defined (looking for valid values)")
    # look for possibel values for llvm in the default locations (for linux)
    if (EXISTS "$ENV{ROCM_PATH}/llvm") # try to use the default rocm llvm install
        set(ENV{LLVM_HOME} "$ENV{ROCM_PATH}/llvm")
        message("[NOTE] $LLVM_HOME was set to `$ENV{ROCM_PATH}/llvm` for a version of amd-llvm\n")
    elseif(EXISTS "../../llvm-project/build") # for using a local debug build of llvm 
        set(ENV{LLVM_HOME} "../../llvm-project/build")
        message("[NOTE] $LLVM_HOME was set to `../../llvm-project/build` for a local debug build of llvm\n")
    else() # try using a normal llvm install
        # list instead of range to select prefered values (15 or 12) before other/future values
        foreach(I IN ITEMS 16 17 18 19 20 21 22 23 24 25 15 12 14 13 11 10) 
            if(EXISTS "/usr/lib/llvm-${I}")
                set(ENV{LLVM_HOME} "/usr/lib/llvm-${I}")
                message("[NOTE] $LLVM_HOME was set to `/usr/lib/llvm-${I}` for llvm v${I}\n") 
                if(i GREATER 16)
                    message(AUTHOR_WARNING "[WARNING] LLVM versions >16 are not officialy supported!")
                endif()
                break()
            endif()
        endforeach()
    endif()
    if(NOT DEFINED ENV{LLVM_HOME})
        message(FATAL_ERROR "[ERROR] $LLVM_HOME could not be found or infered (is LLVM installed?)")
    endif()
endif()
set(ENV{LLVM_DIR} $ENV{LLVM_HOME}/lib/cmake/llvm)

find_package(LLVM REQUIRED CONFIG PATHS ${LLVM_DIR})
add_definitions(${LLVM_DEFINITIONS_LIST})
include_directories(${LLVM_INCLUDE_DIRS})
link_directories(${LLVM_LIBRARY_DIRS})
if (${LLVM_VERSION_MAJOR} VERSION_GREATER_EQUAL 10)
    set(CMAKE_CXX_STANDARD 17)
    message("[NOTE] $LLVM_HOME version was greater than 10 => compiling with C++17\n")
else()
    set(CMAKE_CXX_STANDARD 11)
    message("[NOTE] $LLVM_HOME version was less than 10 => compiling with C++11\n")
endif()

# set(CMAKE_C_COMPILER $ENV{LLVM_HOME}/bin/clang CACHE FILEPATH "set C compiler to default clang" FORCE)
# set(CMAKE_CXX_COMPILER $ENV{LLVM_HOME}/bin/clang++ CACHE FILEPATH "set C++ compiler to default clang" FORCE)
set(CMAKE_C_COMPILER $ENV{ROCM_PATH}/lib/llvm/bin/clang CACHE FILEPATH "set C compiler to amd-clang" FORCE)
set(CMAKE_CXX_COMPILER $ENV{ROCM_PATH}/lib/llvm/bin/clang++ CACHE FILEPATH "set C++ compiler to amd-clang" FORCE)

set(CMAKE_INSTALL_LIBDIR "")
set(CMAKE_INSTALL_BINDIR "")
set(CMAKE_INSTALL_INCLUDEDIR include)
set(SCABBARD_PATH ${CMAKE_BINARY_DIR}/scabbard)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${SCABBARD_PATH})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${SCABBARD_PATH})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${SCABBARD_PATH})

option(SCABBARD_USE_COMPRESSION 
        "set compression level of trace files using zlib file notation i.e. /[1-9][hf]?/ in SCABBARD_COMPRESSION_LEVEL var" 
        ON)

if(SCABBARD_USE_COMPRESSION)
find_package(ZLIB 1.2 REQUIRED)
set(SCABBARD_ZLIB_LIBRARIES "-L${ZLIB_LIBRARIES}")
set(SCABBARD_LINK_ZLIB "-lz")
if(NOT DEFINED SCABBARD_COMPRESSION_LEVEL)
set(SCABBARD_COMPRESSION_LEVEL "6" CACHE STRING "compression level of trace files using zlib file notation i.e. /[1-9][hf]?/")
endif()
message(NOTICE "[NOTE] building lib trace to use compression (level=${SCABBARD_COMPRESSION_LEVEL})")
else()
set(SCABBARD_ZLIB_LIBRARIES "")
set(SCABBARD_LINK_ZLIB "")
endif()

configure_file(${CMAKE_SOURCE_DIR}/scabbard.cmake ${SCABBARD_PATH}/scabbard.cmake @ONLY)
# configure_file(${CMAKE_SOURCE_DIR}/scabbard.cmake ${SCABBARD_PATH}/cmake/scabbard.cmake @ONLY)


add_subdirectory(lib)
add_subdirectory(libtrace)
add_subdirectory(instr)
add_subdirectory(verif)
add_subdirectory(interception)

install(TARGETS scabbard instr trace trace.device
        DESTINATION  ${CMAKE_INSTALL_PREFIX}/
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_WRITE GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
        )
install(DIRECTORY ${SCABBARD_PATH}/
        DESTINATION ${CMAKE_INSTALL_PREFIX}
        FILE_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_WRITE GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
        DIRECTORY_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_WRITE GROUP_EXECUTE WORLD_READ
        FILES_MATCHING PATTERN "*.py" PATTERN "*.cmake"
        )
