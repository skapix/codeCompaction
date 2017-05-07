add_library(${pass_name} MODULE MergeBB.cpp CompareBB.cpp CompareBB.h FunctionCost.cpp FunctionCost.h
        Utilities.cpp Utilities.h)
#llvm_map_components_to_libnames(llvm_local_libs object)
#message(STATUS "Local libraries: ${llvm_local_libs}")
target_link_libraries(${pass_name} libLLVMObject.a)#${llvm_local_libs})
