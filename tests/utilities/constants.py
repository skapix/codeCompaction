#!/usr/bin/python

g_optimization = "-bbfactor"
g_loadOptimization = "../build/libIRFactoringTransform.so"

g_opt = "opt"

g_optWithLoad = g_opt + " -load " + g_loadOptimization + " " + g_optimization

g_startStrIdent = "define "

g_identLen = len(g_startStrIdent) 


# compare constants
g_factoredDir = "tmpFactored"

g_x64Dir = "tmpFactored/x64"
g_armDir = "tmpFactored/arm"

g_llcX64 = "llc -march=x86-64 -x86-asm-syntax=intel"
g_llcArm = "llc -march=arm"

g_gccX64 = "g++"
g_gccArm = "arm-none-eabi-g++ --specs=rdimon.specs -Wl,--start-group -lgcc -lc -lm -lrdimon -Wl,--end-group"

g_arch = [[g_x64Dir, g_llcX64, g_gccX64], [g_armDir, g_llcArm, g_gccArm]]


g_cgreen = '\33[32m'
g_cred = '\33[31m'
g_cend = '\33[0m'
g_cyellow = '\33[33m'
