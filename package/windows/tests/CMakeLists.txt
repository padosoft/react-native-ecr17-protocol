cmake_minimum_required(VERSION 3.20)
project(ecr17_windows_transport_tests CXX)

# Standalone test of the Windows (Winsock) Ecr17Transport against a loopback
# TCP server. It builds the real transport source plus Nitro's portable C++
# runtime and JSI, without React Native Windows, so it runs in a couple of
# minutes on any Windows machine with MSVC + the Windows SDK.
#
#   cmake -S package/windows/tests -B build-win
#   cmake --build build-win --config Release
#   ctest --test-dir build-win -C Release --output-on-failure
#
# Needs `node_modules` installed at the repo root (react-native-nitro-modules,
# react-native) and the nitrogen output (`cd package && npx nitrogen`).

if(NOT WIN32)
  message(FATAL_ERROR "The Windows transport tests only build on Windows.")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.tar.gz
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()

set(ECR17_PACKAGE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../..)
set(ECR17_WINDOWS_DIR ${ECR17_PACKAGE_DIR}/windows/Ecr17)
set(ECR17_GENERATED_DIR ${ECR17_PACKAGE_DIR}/nitrogen/generated/shared/c++)
set(NODE_MODULES_DIR ${ECR17_PACKAGE_DIR}/../node_modules CACHE PATH "node_modules with react-native-nitro-modules and react-native")
set(NITRO_DIR ${NODE_MODULES_DIR}/react-native-nitro-modules)
set(JSI_DIR ${NODE_MODULES_DIR}/react-native/ReactCommon/jsi)

foreach(required ${NITRO_DIR}/cpp ${JSI_DIR}/jsi/jsi.cpp ${ECR17_GENERATED_DIR}/HybridEcr17TransportSpec.cpp)
  if(NOT EXISTS ${required})
    message(FATAL_ERROR "Missing ${required}. Install node_modules at the repo root and run nitrogen in package/.")
  endif()
endforeach()

# <NitroModules/Foo.hpp> header map, same generator the vcxproj uses.
find_program(NODE_EXECUTABLE node REQUIRED)
set(NITRO_SHIMS_DIR ${CMAKE_BINARY_DIR}/shims)
execute_process(
  COMMAND ${NODE_EXECUTABLE} ${ECR17_PACKAGE_DIR}/scripts/windows-nitro-shims.mjs
          --nitro ${NITRO_DIR} --out ${NITRO_SHIMS_DIR}/NitroModules
  RESULT_VARIABLE shims_result
)
if(NOT shims_result EQUAL 0)
  message(FATAL_ERROR "windows-nitro-shims.mjs failed (${shims_result})")
endif()

# Nitro's C++ runtime, minus views/ (needs Fabric renderer headers; no views here).
file(GLOB NITRO_SOURCES ${NITRO_DIR}/cpp/*/*.cpp)
list(FILTER NITRO_SOURCES EXCLUDE REGEX "/views/")
file(GLOB NITRO_INCLUDE_DIRS LIST_DIRECTORIES true ${NITRO_DIR}/cpp/*)
list(FILTER NITRO_INCLUDE_DIRS EXCLUDE REGEX "\\.(cpp|hpp|h)$")

add_executable(ecr17_windows_transport_tests
  test_windows_transport.cpp
  NitroTestPlatform.cpp
  ${ECR17_WINDOWS_DIR}/HybridEcr17TransportWindows.cpp
  ${ECR17_GENERATED_DIR}/HybridEcr17TransportSpec.cpp
  ${NITRO_SOURCES}
  ${JSI_DIR}/jsi/jsi.cpp
)

target_include_directories(ecr17_windows_transport_tests PRIVATE
  ${ECR17_WINDOWS_DIR}
  ${ECR17_GENERATED_DIR}
  ${NITRO_SHIMS_DIR}
  ${NITRO_INCLUDE_DIRS}
  ${JSI_DIR}
)

target_compile_definitions(ecr17_windows_transport_tests PRIVATE _WINSOCK_DEPRECATED_NO_WARNINGS NOMINMAX WIN32_LEAN_AND_MEAN)
target_compile_options(ecr17_windows_transport_tests PRIVATE
  /W3 /bigobj /Zc:__cplusplus /utf-8
  /FI${ECR17_WINDOWS_DIR}/NitroMsvcStdCompat.h
)
target_link_libraries(ecr17_windows_transport_tests PRIVATE GTest::gtest_main ws2_32)

include(GoogleTest)
# A hung probe (e.g. a blocking peek after the reader drained the socket) must fail, not hang.
gtest_discover_tests(ecr17_windows_transport_tests DISCOVERY_TIMEOUT 60 PROPERTIES TIMEOUT 30)
