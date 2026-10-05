# ecr17: the ECR17 (Nexi) payment-terminal protocol as a React-free C++20
# library: framing, LRC, command builders, response parsers, the ACK/NAK session
# and the client (auto-connect, money-safe retry). On Windows it also builds the
# Winsock transport.
#
#   cmake -S packages/ecr17 -B build && cmake --build build && ctest --test-dir build --output-on-failure
#
# The React Native binding (packages/react-native-ecr17) compiles the same sources through its own
# build (Android CMake, the Ecr17 pod, the Windows Nitro host).
cmake_minimum_required(VERSION 3.21)
project(ecr17 VERSION 2.0.0 LANGUAGES CXX)

add_library(ecr17 STATIC
    cpp/src/Lcr.cpp
    cpp/src/PacketCodec.cpp
    cpp/src/Ecr17Protocol.cpp
    cpp/src/Ecr17Response.cpp
    cpp/src/Ecr17Session.cpp
    cpp/src/Ecr17Client.cpp
)
add_library(ecr17::ecr17 ALIAS ecr17)
target_compile_features(ecr17 PUBLIC cxx_std_20)
target_include_directories(ecr17 PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/cpp/include>
    $<INSTALL_INTERFACE:include>
)
find_package(Threads REQUIRED)
target_link_libraries(ecr17 PUBLIC Threads::Threads)

if(WIN32)
    target_sources(ecr17 PRIVATE windows/src/WinsockTransport.cpp)
    target_include_directories(ecr17 PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/windows/include>
    )
    target_compile_definitions(ecr17 PUBLIC NOMINMAX WIN32_LEAN_AND_MEAN)
    target_link_libraries(ecr17 PUBLIC ws2_32)
endif()

if(MSVC)
    target_compile_options(ecr17 PRIVATE /W4 /permissive- /utf-8 /EHsc)
else()
    target_compile_options(ecr17 PRIVATE -Wall -Wextra)
endif()

install(TARGETS ecr17 EXPORT ecr17Targets ARCHIVE DESTINATION lib)
install(DIRECTORY cpp/include/ DESTINATION include)
if(WIN32)
    install(DIRECTORY windows/include/ DESTINATION include)
endif()
install(EXPORT ecr17Targets NAMESPACE ecr17:: DESTINATION lib/cmake/ecr17
        FILE ecr17Config.cmake)

option(ECR17_BUILD_TESTS "Build the GoogleTest suites" ${PROJECT_IS_TOP_LEVEL})
if(ECR17_BUILD_TESTS)
    find_package(GTest CONFIG QUIET)
    if(NOT GTest_FOUND)
        include(FetchContent)
        FetchContent_Declare(googletest
            URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.tar.gz)
        set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
        FetchContent_MakeAvailable(googletest)
    endif()
    enable_testing()
    include(GoogleTest)
    add_subdirectory(cpp/tests)
    if(WIN32)
        add_subdirectory(windows/tests)
    endif()
endif()
