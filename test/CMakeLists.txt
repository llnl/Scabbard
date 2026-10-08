

set(GTEST_RELEASE_HASH 063de7e9578f82b369302001269680b4b1553359)

include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/${GTEST_RELEASE_HASH}.zip)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()

add_subdirectory("state-machine")

