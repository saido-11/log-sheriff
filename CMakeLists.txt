cmake_minimum_required(VERSION 3.20)

project(log-sheriff VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(LOG_SHERIFF_BUILD_TESTS "Build unit tests" ON)

include(FetchContent)

FetchContent_Declare(
  CLI11
  GIT_REPOSITORY https://github.com/CLIUtils/CLI11.git
  GIT_TAG v2.4.2
)
FetchContent_MakeAvailable(CLI11)

add_library(log_sheriff_lib
  src/summarizer.cpp
)

target_include_directories(log_sheriff_lib
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_compile_features(log_sheriff_lib PUBLIC cxx_std_20)

if(MSVC)
  target_compile_options(log_sheriff_lib PRIVATE /W4 /permissive-)
else()
  target_compile_options(log_sheriff_lib PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(log-sheriff
  src/main.cpp
)

target_link_libraries(log-sheriff
  PRIVATE
    log_sheriff_lib
    CLI11::CLI11
)

if(LOG_SHERIFF_BUILD_TESTS)
  include(CTest)
  enable_testing()

  FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.6.0
  )
  FetchContent_MakeAvailable(Catch2)

  add_executable(log_sheriff_tests
    tests/summarizer_tests.cpp
  )

  target_link_libraries(log_sheriff_tests
    PRIVATE
      log_sheriff_lib
      Catch2::Catch2WithMain
  )

  target_compile_features(log_sheriff_tests PRIVATE cxx_std_20)

  list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
  include(Catch)
  catch_discover_tests(log_sheriff_tests)
endif()
