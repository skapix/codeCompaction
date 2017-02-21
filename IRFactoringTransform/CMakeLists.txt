add_library(${pass_name} MODULE BBFactoring.cpp ../external/merging.h ../external/merging.cpp TargetDependent/CommonDecisionMaker.cpp TargetDependent/CommonDecisionMaker.h TargetDependent/DecisionMaker_x86_64.h TargetDependent/DecisionMaker_arm.h IDecisionMaker.cpp IDecisionMaker.h ForceMergeDecisionMaker.h)
#target_link_libraries(${pass_name} ${llvm_libs})
