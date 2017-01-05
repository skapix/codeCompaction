#!/usr/bin/python

g_optimization = "-bbfactor"
g_loadOptimization = "../build/libIRFactoringTransform.so"

g_opt = "opt"

g_optWithLoad = g_opt + " -load " + g_loadOptimization + " " + g_optimization

g_startStrIdent = "define "

g_identLen = len(g_startStrIdent) 


