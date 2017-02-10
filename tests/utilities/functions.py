#!/usr/bin/python

import os
import io
from utilities.constants import *
from utilities.compile import Compile, createProcess
from pathlib import Path

# common functions


def constructOptWithLoadQuery(filename, additionalParams="", outputFile=""):
    out = "" if outputFile == "" else " -o " + outputFile
    return g_optWithLoad + " " + additionalParams + " " + filename + out


def createOptCompile(filename):
    optInfo = g_optCompileInfo
    result = Compile(filename, "", optInfo)
    result.outputFile = os.path.splitext(filename)[0] + "_bbf.ll"
    return result

def createLlvmLink(filenames, filenameOut,  opt=""):

    g_llvmLinkCompileInfo = CompileInfo("llvm-link", opt, ".ll")
    result = Compile(filename, "", g_llvmLinkCompileInfo)
    result.outputFile = os.path.splitext(filename)[0] + "_bbf.ll"
    return result

def printError(value):
    print(g_cyellow + str(value) + g_cend)


def printSuccess(value):
    print(g_cgreen + str(value) + g_cend)


def printFailure(value):
    print(g_cred + str(value) + g_cend)


def getExt(filename):
    return os.path.splitext(filename)[1]

# compare amount of functions


def isFuncLine(s):
    return len(s) > g_identLen and s[:g_identLen] == g_startStrIdent


def getFuncAmount(f):
    result = 0
    for line in f:
        if isFuncLine(line):
            result = result + 1
    return result


def equalModules(m0, m1):
    m0.readline()
    m0.readline()
    m1.readline()
    m1.readline()
    return m0.read() == m1.read()


def checkFile(filename):
    if getExt(filename) != ".ll":
        raise Exception("Bad extension " + os.path.splitext(filename))
    if not os.path.exists(filename):
        raise Exception("Path not exists: " + os.path.abspath(filename))

    content0 = Path(filename).read_text()
    if not content0:
        raise "Empty file " + filename

    query = constructOptWithLoadQuery(filename, "-S -bbfactor-force-merging")
    rc, content1 = createProcess(query)
    if rc != 0:
        raise Exception("File " + filename + "\nQuery: " +
                        query + " returned " + str(rc) + "\nError:" + content1)

    num0 = getFuncAmount(io.StringIO(content0))
    num1 = getFuncAmount(io.StringIO(content1))
    if num1 == num0:
        return 0, equalModules(io.StringIO(content0), io.StringIO(content1))
    else:
        return (num1 - num0, False)

# compare sizes of binaries


def compileWithFrontend(filename, arch):
    frontend = Compile(filename, arch)
    frontend.compile("")

    assert getExt(frontend.outputFile) == ".ll" or getExt(
        frontend.outputFile) == ".bc"
    irFile = frontend.outputFile
    opt = createOptCompile(irFile)
    irOptFile = opt.outputFile
    opt.compile("")

    compileIr = Compile(irFile, arch)
    compileIr.compile("")
    compileIrOpt = Compile(irOptFile, arch)
    compileIrOpt.compile("")
    return [compileIr.outputFile, compileIrOpt.outputFile]


def getBinaryCodeSize(filename):
    err, out = createProcess("size " + filename)
    if err != 0:
        raise "Can't get binary code size. Error: " + out
    return int(out.split()[6])


def printSizes(f, fOpt):
    if f < fOpt:
        printFailure(str(f) + " < " + str(fOpt))
    elif f == fOpt:
        print(f, "==", fOpt)
    else:
        printSuccess(str(f) + " > " + str(fOpt))


def compareBinaryFileSizes(filename, additionalParams=""):
    if (not os.path.exists(filename)):
        raise Exception("File " + filename + " does not exist")
    archList = [key for key in g_arches]
    for arch in archList:
        print(arch + ": ", end="")
        try:
            files = compileWithFrontend(filename, arch)
            assert len(files) == 2, "Wrong final length size"
            sz = getBinaryCodeSize(files[0])
            szOpt = getBinaryCodeSize(files[1])
            printSizes(sz, szOpt)
        except Exception as e:
            printError("Error: " + str(e))
