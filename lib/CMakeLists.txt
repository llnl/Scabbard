cmake_minimum_required(VERSION 3.16)
project(libscabbard VERSION 0.1)

add_library(scabbard SHARED
    src/Enums.cpp
    # src/TraceData.cpp
    # src/MetadataIO.cpp
  )

target_include_directories(scabbard
    PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE
    ./include
    src
  )
install(TARGETS scabbard EXPORT libscabbardConfig
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  )
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

export(TARGETS scabbard FILE libscabbardConfig.cmake)