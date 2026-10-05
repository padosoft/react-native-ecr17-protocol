# Ecr17Kit: the ECR17 (Nexi) payment-terminal protocol as a React-free C++20
# library: framing, LRC, command builders, response parsers, the ACK/NAK session
# and the client (auto-connect, money-safe retry). On Windows it also builds the
# Winsock transport.
#
#   cmake -S packages/ecr17-kit -B build && cmake --build build && ctest --test-dir build --output-on-failure
#
# The React Native binding (packages/react-native-ecr17) compiles the same sources through its own
# build (Android CMake, the Ecr17Kit pod, the Windows Nitro host).
cmake_minimum_required(VERSION 3.21)
project(Ecr17Kit VERSION 1.0.0 LANGUAGES CXX)

add_library(Ecr17Kit STATIC
    cpp/src/Lcr.cpp
    cpp/src/PacketCodec.cpp
    cpp/src/Ecr17Protocol.cpp
    cpp/src/Ecr17Response.cpp
    cpp/src/Ecr17Session.cpp
    cpp/src/Ecr17Client.cpp
)
add_library(Ecr17Kit::Ecr17Kit ALIAS Ecr17Kit)
target_compile_features(Ecr17Kit PUBLIC cxx_std_20)
target_include_directories(Ecr17Kit PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/cpp/include>
    $<INSTALL_INTERFACE:include>
)
find_package(Threads REQUIRED)
target_link_libraries(Ecr17Kit PUBLIC Threads::Threads)

if(WIN32)
    target_sources(Ecr17Kit PRIVATE windows/src/WinsockTransport.cpp)
    target_include_directories(Ecr17Kit PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/windows/include>
    )
    target_compile_definitions(Ecr17Kit PUBLIC NOMINMAX WIN32_LEAN_AND_MEAN)
    target_link_libraries(Ecr17Kit PUBLIC ws2_32)
endif()

if(MSVC)
    target_compile_options(Ecr17Kit PRIVATE /W4 /permissive- /utf-8 /EHsc)
else()
    target_compile_options(Ecr17Kit PRIVATE -Wall -Wextra)
endif()

install(TARGETS Ecr17Kit EXPORT Ecr17KitTargets ARCHIVE DESTINATION lib)
install(DIRECTORY cpp/include/ DESTINATION include)
if(WIN32)
    install(DIRECTORY windows/include/ DESTINATION include)
endif()
install(EXPORT Ecr17KitTargets NAMESPACE Ecr17Kit:: DESTINATION lib/cmake/Ecr17Kit
        FILE Ecr17KitConfig.cmake)

option(ECR17KIT_BUILD_TESTS "Build the GoogleTest suites" ${PROJECT_IS_TOP_LEVEL})
if(ECR17KIT_BUILD_TESTS)
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
