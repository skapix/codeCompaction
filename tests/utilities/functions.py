#!/usr/bin/python

import os.path
from utilities.constants import *
from utilities.compile import Compile, createProcess
from pathlib import Path

# common functions

def createOptCompile(filename, arch = ""):
    optInfo = g_optCompileInfo

    result = Compile(filename, "", optInfo)
    archAdd = "" if arch == "" else arch + "/"
    result.outputFile = g_commonDir + "/" + archAdd + getShortName(filename) + "_bbf.ll"
    
    return result

def createLlvmLink(filenames, filenameOut,  opt=""):

    g_llvmLinkCompileInfo = CompileInfo("llvm-link", opt, ".ll")
    result = Compile(filename, "", g_llvmLinkCompileInfo)
    result.outputFile = os.path.splitext(filename)[0] + "_bbf.ll"
    return result

def getShortName(str):
    noExt = os.path.splitext(str)[0]
    return os.path.split(noExt)[1]

def printError(value):
    print(g_cyellow + str(value) + g_cend)


def printSuccess(value):
    print(g_cgreen + str(value) + g_cend)


def printFailure(value):
    print(g_cred + str(value) + g_cend)


def getExt(filename):
    return os.path.splitext(filename)[1]

