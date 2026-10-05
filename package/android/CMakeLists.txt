project(Ecr17)
cmake_minimum_required(VERSION 3.9.0)

set (PACKAGE_NAME Ecr17)
set (CMAKE_VERBOSE_MAKEFILE ON)
set (CMAKE_CXX_STANDARD 20)

# Enable Raw Props parsing in react-native (for Nitro Views)
add_compile_options(-DRN_SERIALIZABLE_STATE=1)

# The protocol core lives in @padosoft/ecr17-kit (React-free). Resolved with Node,
# like @padosoft/native-modules' Gradle scripts: from node_modules in an app, from
# the workspace link in this repo.
find_program(NODE_EXECUTABLE node REQUIRED)
execute_process(
  COMMAND ${NODE_EXECUTABLE} --print "require('path').dirname(require.resolve('@padosoft/ecr17-kit/package.json'))"
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  OUTPUT_VARIABLE ECR17_KIT_DIR
  OUTPUT_STRIP_TRAILING_WHITESPACE
  RESULT_VARIABLE ECR17_KIT_RESULT
)
if(NOT ECR17_KIT_RESULT EQUAL 0 OR NOT EXISTS "${ECR17_KIT_DIR}/cpp/include/Ecr17Kit/Ecr17Client.hpp")
  message(FATAL_ERROR "@padosoft/ecr17-kit not found (got '${ECR17_KIT_DIR}'). Install it next to @padosoft/react-native-ecr17.")
endif()
file(GLOB ECR17_KIT_SOURCES CONFIGURE_DEPENDS "${ECR17_KIT_DIR}/cpp/src/*.cpp")

# Define C++ library and add all sources
add_library(${PACKAGE_NAME} SHARED
	src/main/cpp/cpp-adapter.cpp
	../cpp/Ecr17.cpp
	../cpp/Ecr17Client/HybridEcr17Client.cpp
	../cpp/Transport/NativeTransportAdapter.cpp
	${ECR17_KIT_SOURCES}
)

# Add Nitrogen specs :)
include(${CMAKE_SOURCE_DIR}/../nitrogen/generated/android/Ecr17+autolinking.cmake)

# Set up local includes
include_directories(
        "src/main/cpp"
        "../cpp"
        "../cpp/Ecr17Client"  # generated Ecr17OnLoad.cpp does a flat #include "HybridEcr17Client.hpp"
        "${ECR17_KIT_DIR}/cpp/include"
)

find_library(LOG_LIB log)

# Link all libraries together
target_link_libraries(
        ${PACKAGE_NAME}
        ${LOG_LIB}
        android                                   # <-- Android core
)
