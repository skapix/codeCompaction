#!/usr/bin/python

import os
import subprocess
import sys
import os.path
import shutil
import glob
import argparse
from utilities.functions import *
from utilities.compile import *
from utilities.constants import *

## comparing utils

def compileWithFrontend(filename, arch, isVerbose):
    fileExt = getExt(filename)
    irFile = filename
    if not fileExt == ".ll" and not fileExt == ".bc":
        frontend = Compile(filename, arch)
        frontend.compile() 
        irFile = frontend.outputFile
    

    opt = createOptCompile(irFile, arch)
    irOptFile = opt.outputFile
    _, output, error = opt.compile()
    # dbgs info goes into errs, because output goes to the outputFile
    if isVerbose and error != "":
        print("Opt:\n")
        print(error)


    compileIr = Compile(irFile, arch)
    compileIr.compile()
    compileIrOpt = Compile(irOptFile, arch)
    compileIrOpt.compile()
    return [compileIr.outputFile, compileIrOpt.outputFile]


def getBinaryCodeSize(filename):
    err, out, _ = createProcess("size " + filename)
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


def compareBinaryFileSizes(filename, archList, isVerbose):
    if (not os.path.exists(filename)):
        raise Exception("File " + filename + " does not exist")
    for arch in archList:
        print(arch + ": ", end="")
        try:
            files = compileWithFrontend(filename, arch, isVerbose)
            assert len(files) == 2, "Wrong final length size"
            sz = getBinaryCodeSize(files[0])
            szOpt = getBinaryCodeSize(files[1])
            if isVerbose:
                print(arch + ": ", end="")
            printSizes(sz, szOpt)
        except Exception as e:
            printError("Error: " + str(e))

## end comparing utils



def applyCompare(filenames, arches, isVerbose):
    print("Original Size | sign | Optimized size")
    for filename in filenames:
        print("File:", filename)
        try:
            compareBinaryFileSizes(filename, arches, isVerbose)
        except Exception as inst:
            printError(str(inst))

# start program
if __name__ == "__main__":
    # prepare parser
    parser = argparse.ArgumentParser(
    description='Program builds 2 binary files from some languages\
    (currently .ll, .bc .c, .cpp) for x86-64 and arm architectures:  with and without bbfactor optimizaton.\
    Further it compares code sizes of the created object files')

    parser.add_argument('filenames', nargs='+', help='Filenames to be compared')
    parser.add_argument('--arch', nargs='*', choices=g_arches.keys(), default=g_arches.keys(),
                        help='Choose architecture. All by default')
    parser.add_argument('--args', nargs='*', default="", type=parseAdditionalArguments,  help="Additional args in view like 'utility:extra flags'")
    parser.add_argument('-v', action='store_true', help="Print output of transforms")
    parser.add_argument('--clean', action='store_true', help="Remove temporary directory")

    args = parser.parse_args()

    if (args.clean):
        if os.path.exists(g_commonDir):
            shutil.rmtree(g_commonDir)
        sys.exit()


    filenames = []
    for filename in args.filenames:
        if not os.path.exists(filename):
            print("File ", filename, " does not exist")
            continue
        if os.path.isdir(filename):
            print(filename, "is directory. Directories are not supported with this tool")
            continue
        filenames += [filename]
            

    applyCompare(filenames, args.arch, args.v)
