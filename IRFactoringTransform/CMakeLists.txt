add_library(${pass_name} MODULE BBFactoring.cpp ../external/merging.h ../external/merging.cpp TargetDependent/CommonPAC.cpp TargetDependent/CommonPAC.h TargetDependent/PAC_x86_64.h TargetDependent/PAC_arm.h IProceduralAbstractionCost.cpp IProceduralAbstractionCost.h ForceMergePAC.h TargetDependent/PAC_x86_64.cpp TargetDependent/PAC_arm.cpp)
#target_link_libraries(${pass_name} ${llvm_libs})
