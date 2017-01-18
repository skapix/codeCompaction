#!/usr/bin/python

from utilities.constants import *
import os, subprocess, sys, os.path,  io
from pathlib import Path

##### common functions

def createProcess(query):
	process = subprocess.Popen(query.split(), stdout=subprocess.PIPE, stderr=subprocess.PIPE)
	ecode = process.wait()
	out, err = process.communicate()
	return (ecode, out.decode("utf-8") ) if ecode == 0 else (ecode, err.decode("utf-8") )

def constructOptWithLoadQuery(filename, additionalParams = ""):
	return g_optWithLoad + " " + additionalParams + " " + filename

##### compare creating additional functions

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
	if os.path.splitext(filename)[1] != ".ll":
		raise Exception("Bad extension " + os.path.splitext(filename))
	if not os.path.exists(filename):
		raise Exception("Path not exists: " + os.path.abspath(filename))
	
	content0 = Path(filename).read_text()
	if not content0:
		raise "Empty file " + filename
	
	query = constructOptWithLoadQuery(filename, "-S -bbfactor-force-merging")
	rc, content1 = createProcess(query)
	if rc != 0:
		raise Exception("File " + filename + "\nQuery: " + query + " returned " + str(rc) + "\nError:" + content1)
	
	num0 = getFuncAmount(io.StringIO(content0))
	num1 = getFuncAmount(io.StringIO(content1))
	if num1 == num0:
		return 0, equalModules(io.StringIO(content0), io.StringIO(content1))
	else:
		return (num1 - num0, False)
