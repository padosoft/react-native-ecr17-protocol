# The Node.js addon (ecr17.node): addon.cpp + the C++ core (@padosoft/ecr17) + the host's
# TCP transport. Built by cmake-js, which passes Node's headers (CMAKE_JS_INC) and, on
# Windows, its import library (CMAKE_JS_LIB):
#
#   cd packages/ecr17-node && bun run build:node     -> build/Release/ecr17.node
#
# Node-API is ABI-stable, so one binary per platform/arch runs on every Node.js version
# that supports NAPI_VERSION (8: Node.js 12.22+, 14.17+, 16+).
cmake_minimum_required(VERSION 3.21)
project(ecr17_node LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# The core is a static library linked into a shared module.
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# The core: ECR17_DIR when given, else @padosoft/ecr17 resolved with Node (an installed
# package, or the workspace link), else the monorepo sibling (CI installs no workspace).
set(ECR17_DIR "" CACHE PATH "Path to @padosoft/ecr17")
if(NOT ECR17_DIR)
    execute_process(
        COMMAND node -p "require('path').dirname(require.resolve('@padosoft/ecr17/package.json'))"
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        OUTPUT_VARIABLE ECR17_RESOLVED
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
        RESULT_VARIABLE ECR17_RESOLVE_RESULT
    )
    if(ECR17_RESOLVE_RESULT EQUAL 0 AND EXISTS "${ECR17_RESOLVED}/CMakeLists.txt")
        set(ECR17_DIR "${ECR17_RESOLVED}")
    elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../ecr17/CMakeLists.txt")
        set(ECR17_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../ecr17")
    else()
        message(FATAL_ERROR "@padosoft/ecr17 not found: install it, or pass -DECR17_DIR=<path>.")
    endif()
endif()
message(STATUS "ecr17 core: ${ECR17_DIR}")

set(ECR17_BUILD_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory(${ECR17_DIR} ecr17)

execute_process(
    COMMAND node -p "require('node-addon-api').include_dir"
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    OUTPUT_VARIABLE NODE_ADDON_API_DIR
    OUTPUT_STRIP_TRAILING_WHITESPACE
    COMMAND_ERROR_IS_FATAL ANY
)

add_library(ecr17_node SHARED addon.cpp ${CMAKE_JS_SRC})
set_target_properties(ecr17_node PROPERTIES PREFIX "" SUFFIX ".node" OUTPUT_NAME ecr17)
target_include_directories(ecr17_node PRIVATE ${CMAKE_JS_INC} ${NODE_ADDON_API_DIR})
target_compile_definitions(ecr17_node PRIVATE NAPI_VERSION=8 NAPI_CPP_EXCEPTIONS NODE_ADDON_API_DISABLE_DEPRECATED)
target_link_libraries(ecr17_node PRIVATE ecr17::ecr17 ${CMAKE_JS_LIB})

if(MSVC)
    target_compile_options(ecr17_node PRIVATE /W4 /utf-8 /EHsc)
    if(CMAKE_JS_NODELIB_DEF AND CMAKE_JS_NODELIB_TARGET)
        # Generates node.lib from cmake-js' .def (cmake-js >= 7).
        execute_process(COMMAND ${CMAKE_AR} /def:${CMAKE_JS_NODELIB_DEF} /out:${CMAKE_JS_NODELIB_TARGET} ${CMAKE_STATIC_LINKER_FLAGS})
    endif()
else()
    target_compile_options(ecr17_node PRIVATE -Wall -Wextra -fexceptions)
endif()
