cmake_minimum_required(VERSION 3.4.3)

if ($ENV{CLION_IDE})
  set(BUILD_DIR ${CMAKE_SOURCE_DIR}/build)
  if(NOT EXISTS BUILD_DIR)
    file(MAKE_DIRECTORY ${BUILD_DIR})
  endif(NOT EXISTS BUILD_DIR)
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${BUILD_DIR})
  set(CMAKE_BINARY_OUTPUT_DIRECTORY ${BUILD_DIR})
endif()

find_package(LLVM REQUIRED CONFIG)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")
project("LLVMTransform")

if (NOT MSVC)
  add_compile_options(-Wall)
  # http://llvm.org/docs/CMake.html#developing-llvm-passes-out-of-source
  message(STATUS LLVM_ENABLE_RTTI=${LLVM_ENABLE_RTTI})
  if (NOT LLVM_ENABLE_RTTI)
    add_compile_options(-fno-rtti)
  endif(NOT LLVM_ENABLE_RTTI)
endif(NOT MSVC)

set(pass_name IRFactoringTransform)

add_definitions(${LLVM_DEFINITIONS})
include_directories(${LLVM_INCLUDE_DIRS})
llvm_map_components_to_libnames(llvm_libs support core irreader)
add_subdirectory(${pass_name})
