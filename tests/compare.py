#!/usr/bin/python

import os
import subprocess
import sys
import os.path
import shutil
import glob
from utilities.functions import compareBinaryFileSizes, printFailure, printError, printSuccess


def help():
    print('Program builds 2 binary files from some languages (currently .c, .cpp) for x86-64 and arm architectures:  with and without bbfactor optimizaton. \nFurther it compares code sizes of the created object files')


def applyCompare(filenames, additionalParams):
    print("Original Size | sign | Optimized size")
    for filename in filenames:
        print("File:", filename)
        try:
            compareBinaryFileSizes(filename, additionalParams)
        except Exception as inst:
            printError(str(inst))

# start program

if len(sys.argv) < 2:
    print('File argument required\n use --help for info')
    sys.exit()

if (sys.argv[1] == "--help"):
    help()
    sys.exit()

if (sys.argv[1] == "--clean"):
    if os.path.exists(g_commonDir):
        shutil.rmtree(g_commonDir)
    sys.exit()


filenames = []
additionalParams = ""
for i in range(1, len(sys.argv)):
    filename = sys.argv[i]
    if not os.path.exists(filename):
        print("File ", filename, " does not exist")
        continue
    if os.path.isdir(filename):
        for f in glob.glob(filename + "/*.ll") + glob.glob(filename + "*.bc"):
            filenames += [f]
    else:
        filenames += [filename]

applyCompare(filenames, additionalParams)
