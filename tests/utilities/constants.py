#!/usr/bin/python

import os

g_optimization = "-mergebb"
g_optimization_force = "-mergebb-force"
g_loadOptimization = os.path.dirname(os.path.abspath(__file__)) + "/../../build/libIRMergeBB.so"

g_opt = "opt"
g_lli = "lli"
g_link = "llvm-link"

g_optWithLoad = g_opt + " -load " + g_loadOptimization + " " + g_optimization
g_testPath = os.path.dirname(os.path.abspath(__file__)) + "/testCases"

g_armRoot = "/usr/arm-linux-gnueabihf/"

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
g_clangArmArch = "-target arm-linux-gnueabihf --sysroot="

g_arches = {"x64": {"clang++": ArchInfo(g_clangX64Arch), "clang": ArchInfo(g_clangX64Arch),
                    "llc": ArchInfo("-march=x86-64"), "size": ArchInfo("size")},
            "arm": {"clang++": ArchInfo(g_clangArmArch + g_armRoot),
                    "clang": ArchInfo(g_clangArmArch + g_armRoot),
                    "llc": ArchInfo("-march=arm"), "size": ArchInfo("arm-linux-gnueabihf-size") }}

g_commonDir = "tmpFactored"

# print colors
g_cgreen = '\33[32m'
g_cred = '\33[31m'
g_cend = '\33[0m'
g_cyellow = '\33[33m'
