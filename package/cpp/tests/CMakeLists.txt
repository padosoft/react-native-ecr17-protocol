cmake_minimum_required(VERSION 3.16)
project(ecr17_cpp_tests CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# GoogleTest via FetchContent (CI has network access).
include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.tar.gz
)
# Keep CRT linkage consistent on Windows runners.
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()

# Production C++ sources under test (../ is package/cpp).
set(ECR17_CPP_DIR ${CMAKE_CURRENT_SOURCE_DIR}/..)

add_executable(ecr17_cpp_tests
  test_lrc.cpp
  test_packet_codec.cpp
  test_protocol.cpp
  test_protocol_commands.cpp
  test_flows.cpp
  ${ECR17_CPP_DIR}/Lcr/Lcr.cpp
  ${ECR17_CPP_DIR}/PacketCodec/PacketCodec.cpp
  ${ECR17_CPP_DIR}/Ecr17Protocol/Ecr17Protocol.cpp
)

target_include_directories(ecr17_cpp_tests PRIVATE
  ${ECR17_CPP_DIR}                  # resolves "Lcr/Lcr.hpp", "PacketCodec/..."
  ${CMAKE_CURRENT_SOURCE_DIR}/stubs # test-only LrcMode.hpp (Nitrogen-generated in real builds)
)

if(MSVC)
  target_compile_options(ecr17_cpp_tests PRIVATE /W4)
else()
  target_compile_options(ecr17_cpp_tests PRIVATE -Wall -Wextra)
endif()

target_link_libraries(ecr17_cpp_tests PRIVATE GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(ecr17_cpp_tests)
