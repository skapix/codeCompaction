#!/usr/bin/python

import os

g_optimization = "-bbfactor"
g_loadOptimization = os.path.dirname(os.path.abspath(__file__)) + "/../../build/libIRFactoringTransform.so"

g_opt = "opt"
g_link = "llvm-link"

g_optWithLoad = g_opt + " -load " + g_loadOptimization + " " + g_optimization

# function identificator
g_startStrIdent = "define "
g_identLen = len(g_startStrIdent)

g_armInclude = "/usr/arm-none-eabi/include/"
g_armIncludeCpp1 = g_armInclude + "c++/6.3.0/"
g_armIncludeCpp2 = g_armIncludeCpp1 + "arm-none-eabi/"

class CompileInfo:
    def __init__(self, program, defaultArgs, outputExt):
        self.program = program
        self.args = defaultArgs
        self.ext = outputExt

g_defaultClangOpts = "-emit-llvm -S -Oz"
g_optCompileInfo = CompileInfo("opt", " -load " + g_loadOptimization + " " + g_optimization, ".ll")

g_filenamesTo = {".cpp": CompileInfo("clang++", g_defaultClangOpts, ".ll"),
                 ".c": CompileInfo("clang", g_defaultClangOpts + " -fno-unwind-tables", ".ll"),
                 ".ll": CompileInfo("llc", "-filetype=obj", ".o"),
                 ".bc": CompileInfo("llc", "-filetype=obj", ".o")}


class ArchInfo:
    def __init__(self, additionalArgs):
        self.args = additionalArgs
# arches to architecture-dependent arguments

g_clangX64Arch = "-target x86_64-linux-gnu"
g_clangArmArchInclude = "-target arm-none-eabi -I"
g_arches = {"x64": {"clang++": ArchInfo(g_clangX64Arch), "clang": ArchInfo(g_clangX64Arch),
                    "llc": ArchInfo("-march=x86-64")},
            "arm": {"clang++": ArchInfo(g_clangArmArchInclude + g_armIncludeCpp1 + " -I" + g_armIncludeCpp2),
                    "clang": ArchInfo(g_clangArmArchInclude + g_armInclude),
                    "llc": ArchInfo("-march=arm")}}

g_commonDir = "tmpFactored"

# print colors
g_cgreen = '\33[32m'
g_cred = '\33[31m'
g_cend = '\33[0m'
g_cyellow = '\33[33m'