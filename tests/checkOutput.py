#!/usr/bin/python

import os
import argparse
import tempfile
import atexit
from utilities.functions import getExt, createOptCompile
from utilities.constants import g_cyellow, g_cgreen, g_cend, g_lli
from utilities.compile import createProcess

os.chdir(os.path.dirname(os.path.abspath(__file__)))

g_testCasesDir = "testCases/"

def postCheck(filename):
    if os.path.exists(filename):
        os.remove(filename)


def getLLiOutput(filename):
    return createProcess(g_lli + " " + filename)

def printInfo(prefix, color, value):
    endColor = g_cend
    print(prefix)
    print(color + str(value) + endColor)
    print(prefix)

def checkFileCorrectness(filename, isVerbose):
    assert getExt(filename) == ".ll" or getExt(filename) == ".bc", "Bad extension: " + getExt(filename)
    assert os.path.exists(filename), "Path not exists: " + \
        os.path.abspath(filename)

    tmpFile = tempfile.mkstemp()[1]
    atexit.register(postCheck, tmpFile)

    comp = createOptCompile(filename)
    comp.outputFile = tmpFile
    comp.compile("-bbfactor-force-merging")
    original = getLLiOutput(filename)
    modified = getLLiOutput(tmpFile)
    if isVerbose and original != modified:
        print("<<<: ", g_cyellow, "Original", g_cend)
        print(">>>: ", g_cgreen, "Modified", g_cend)
        if original[0] != modified[0]:
            print("Error Code: ", g_cyellow, original[0], g_cyellow, g_cend, ", ", g_cgreen, modified[0], g_cend)
        if original[1] != modified[1]:
            print("Output:")
            printInfo("<<<", g_cyellow, original[1])
            printInfo(">>>", g_cgreen, modified[1])
        if original[2] != modified[2]:
            print("Error:")
            printInfo("<<<", g_cyellow, original[2])
            printInfo(">>>", g_cgreen, modified[2])

    return original == modified

if __name__ == '__main__':
    parser = argparse.ArgumentParser(
    description='Auxiliary utility for testing output of original and optimized with bbfactor files')
    parser.add_argument('filename', help='Filename to be optimized and compared')
    parser.add_argument('-v', action='store_true', help="Verbose")
    args = parser.parse_args()
    exit(0 if checkFileCorrectness(args.filename, args.v) else 1)