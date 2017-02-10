#!/usr/bin/python

import os.path
import io
import unittest
from pathlib import Path
from utilities.functions import getExt, createOptCompile
from utilities.constants import g_identLen, g_startStrIdent

os.chdir(os.path.dirname(os.path.abspath(__file__)))

g_testCasesDir = "testCases/"

## compare amount of functions utils
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

    comp = createOptCompile(filename)
    comp.outputFile = ""
    rc, content1 = comp.compile("-S -bbfactor-force-merging")

    num0 = getFuncAmount(io.StringIO(content0))
    num1 = getFuncAmount(io.StringIO(content1))
    if num1 == num0:
        return 0, equalModules(io.StringIO(content0), io.StringIO(content1))
    else:
        return (num1 - num0, False)

## end compare amount of functions utils
# Tests perform query correctness to the opt and calculate amount of created functions


class TestCreatingFunctions(unittest.TestCase):

    def test_noOutput(self):
        name = "noOutput.ll"
        d, _ = checkFile(g_testCasesDir + name)
        self.assertGreaterEqual(d, 1)

    def test_soloOutput(self):
        name = "soloOutput.ll"
        d, _ = checkFile(g_testCasesDir + name)
        self.assertGreaterEqual(d, 1)

    def test_phiNodes(self):
        name = "phiNodes.ll"
        d, _ = checkFile(g_testCasesDir + name)
        self.assertGreaterEqual(d, 1)

    def test_mergeWithFunc(self):
        name = "mergeWithFunc.ll"
        d, e = checkFile(g_testCasesDir + name)
        self.assertEqual(d, 0)
        self.assertFalse(e)

    def test_structReturn(self):
        name = "structReturn.ll"
        d, _ = checkFile(g_testCasesDir + name)
        self.assertGreaterEqual(d, 1)

    def test_differentOutput(self):
        name = "differentOutput.ll"
        d, _ = checkFile(g_testCasesDir + name)
        self.assertGreaterEqual(d, 1)

    def test_withExceptions(self):
        name = "withExceptions.ll"
        d, _ = checkFile(g_testCasesDir + name)
        self.assertGreaterEqual(d, 1)

    def test_withGlobalMerge(self):
        name = "withGlobalMerge.ll"
        d, e = checkFile(g_testCasesDir + name)
        self.assertEqual(d, 0)
        self.assertFalse(e)

if __name__ == '__main__':
    unittest.main()
