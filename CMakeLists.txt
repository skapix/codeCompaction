cmake_minimum_required(VERSION 3.4.3)
find_package(LLVM REQUIRED CONFIG)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")
project("LLVMTransform")
# http://llvm.org/docs/CMake.html#developing-llvm-passes-out-of-source

message(STATUS LLVM_ENABLE_RTTI=${LLVM_ENABLE_RTTI})
add_compile_options(-Wall)
if (NOT LLVM_ENABLE_RTTI)
  add_compile_options(-fno-rtti)
endif(NOT LLVM_ENABLE_RTTI)

set(pass_name IRFactoringTransform)

add_definitions(${LLVM_DEFINITIONS})
include_directories(${LLVM_INCLUDE_DIRS})
llvm_map_components_to_libnames(llvm_libs support core irreader)
add_subdirectory(${pass_name})
