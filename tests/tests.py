#!/usr/bin/python

import os
import unittest
from utilities.functions import checkFile

os.chdir(os.path.dirname(os.path.abspath(__file__)))
g_testCasesDir = "testCases/"

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
