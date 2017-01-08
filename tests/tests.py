#!/usr/bin/python

import os, io, subprocess, sys, os.path
import unittest
from pathlib import Path
from utilities.functions import *

g_testCasesDir = "testCases/"

#Tests perform query correctness to the opt and calculate amount of created functions
class TestCreatingFunctions(unittest.TestCase):

	def test_no_output(self):
		name = "no_output.ll"
		d, _ = checkFile(g_testCasesDir + name)
		self.assertGreaterEqual(d, 1)

	def test_solo_output(self):
		name = "solo_output.ll"
		d, _ = checkFile(g_testCasesDir + name)
		self.assertGreaterEqual(d, 1)

	def test_phi_nodes(self):
		name = "phi_nodes.ll"
		d, _ = checkFile(g_testCasesDir + name)
		self.assertGreaterEqual(d, 1)

	def test_merge_with_func(self):
		name = "merge_with_func.ll"
		d, _ = checkFile(g_testCasesDir + name)
		self.assertEqual(d, 0)

if __name__ == '__main__':
	unittest.main() 
