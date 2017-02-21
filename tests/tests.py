#!/usr/bin/python

import os
import io
import unittest
import tempfile
import atexit
from pathlib import Path
from utilities.functions import getExt, createOptCompile
from utilities.constants import g_identLen, g_startStrIdent, g_lli
from utilities.compile import createProcess

os.chdir(os.path.dirname(os.path.abspath(__file__)))

g_testCasesDir = "testCases/"

# compare amount of functions utils


def isFuncLine(s):
    return len(s) > g_identLen and s[:g_identLen] == g_startStrIdent


def getFuncAmount(f):
    result = 0
    for line in f:
        if isFuncLine(line):
            result = result + 1
    return result


def checkFile(filename):
    assert getExt(filename) == ".ll", "Bad extension: " + getExt(filename)
    assert os.path.exists(filename), "Path not exists: " + os.path.abspath(filename)

    content0 = Path(filename).read_text()
    if not content0:
        raise "Empty file " + filename

    comp = createOptCompile(filename)
    comp.outputFile = ""
    _, content1, _ = comp.compile("-bbfactor-force-merging")

    num0 = getFuncAmount(io.StringIO(content0))
    num1 = getFuncAmount(io.StringIO(content1))

    return num1 - num0

# end compare amount of functions utils
# Tests perform query correctness to the opt and calculate amount of created functions


class TestCreatingFunctions(unittest.TestCase):

    def test_noOutput(self):
        name = "noOutput.ll"
        self.assertGreaterEqual(checkFile(g_testCasesDir + name), 1)

    def test_soloOutput(self):
        name = "soloOutput.ll"
        self.assertGreaterEqual(checkFile(g_testCasesDir + name), 1)

    def test_phiNodes(self):
        name = "phiNodes.ll"
        self.assertGreaterEqual(checkFile(g_testCasesDir + name), 1)

    def test_mergeWithFunc(self):
        name = "mergeWithFunc.ll"
        self.assertEqual(checkFile(g_testCasesDir + name), 0)

    def test_structReturn(self):
        name = "structReturn.ll"
        self.assertGreaterEqual(checkFile(g_testCasesDir + name), 1)

    def test_differentOutput(self):
        name = "differentOutput.ll"
        self.assertGreaterEqual(checkFile(g_testCasesDir + name), 1)

    def test_withExceptions(self):
        name = "withExceptions.ll"
        self.assertGreaterEqual(checkFile(g_testCasesDir + name), 1)

    def test_withGlobalMerge(self):
        name = "withGlobalMerge.ll"
        self.assertEqual(checkFile(g_testCasesDir + name), 0)

    def test_lifetime(self):
        name = "lifetime.ll"
        self.assertEqual(checkFile(g_testCasesDir + name), 1)

    def test_mergeNoOutput(self):
        name = "mergeNoOutput.ll"
        self.assertEqual(checkFile(g_testCasesDir + name), 0)

    def test_mergeUnusedOutput(self):
            name = "mergeUnusedOutput.ll"
            self.assertEqual(checkFile(g_testCasesDir + name), 0)

# Tests perform ouput equality


def postCheck(filename):
    if os.path.exists(filename):
        os.remove(filename)


def getLLiOutput(filename):
    return createProcess(g_lli + " " + filename)


class TestIdenticalOutput(unittest.TestCase):
    def checkFileCorrectness(self, filename):
        assert getExt(filename) == ".ll" or getExt(filename) == ".ll", "Bad extension: " + getExt(filename)
        assert os.path.exists(filename), "Path not exists: " + os.path.abspath(filename)

        tmpFile = tempfile.mkstemp()[1]
        atexit.register(postCheck, tmpFile)

        comp = createOptCompile(filename)
        comp.outputFile = tmpFile
        comp.compile("-bbfactor-force-merging")
        self.assertEqual(getLLiOutput(filename), getLLiOutput(tmpFile))

    def test_noOutput(self):
        self.checkFileCorrectness(g_testCasesDir + "/noOutput.ll")

    def test_soloOutput(self):
        self.checkFileCorrectness(g_testCasesDir + "/soloOutput.ll")

    def test_phiNodes(self):
        self.checkFileCorrectness(g_testCasesDir + "/phiNodes.ll")

    def test_mergeWithFunc(self):
        self.checkFileCorrectness(g_testCasesDir + "/mergeWithFunc.ll")

    def test_structReturn(self):
        self.checkFileCorrectness(g_testCasesDir + "/structReturn.ll")

    def test_differentOutput(self):
        self.checkFileCorrectness(g_testCasesDir + "/differentOutput.ll")

    def test_withExceptions(self):
        self.checkFileCorrectness(g_testCasesDir + "/withExceptions.ll")

    def test_withGlobalMerge(self):
        self.checkFileCorrectness(g_testCasesDir + "/withGlobalMerge.ll")

    def test_lifetime(self):
        self.checkFileCorrectness(g_testCasesDir + "/lifetime.ll")

    def test_mergeNoOutput(self):
        self.checkFileCorrectness(g_testCasesDir + "/mergeNoOutput.ll")

    def test_mergeUnusedOutput(self):
        self.checkFileCorrectness(g_testCasesDir + "/mergeUnusedOutput.ll")

        

if __name__ == '__main__':
    unittest.main()
