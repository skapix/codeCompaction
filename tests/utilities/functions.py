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

def constructOptWithLoadQuery(filename, additionalParams = "", outputFile = ""):
	out = "" if outputFile == "" else " -o " + outputFile
	return g_optWithLoad + " " + additionalParams + " " + filename + out;

##### compare amount of functions

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

##### compare sizes of binaries

def getFactoredName(filename, archId=-1):
	basename = os.path.basename(filename)
	return g_factoredDir + "/" + os.path.splitext(basename)[0] + "_bbf.ll"

def getAsmName(filename, archId=-1):
	d = g_arch[archId][0] + "/" if archId!=-1 else ""
	basename = os.path.basename(filename)
	return d + os.path.splitext(basename)[0] + ".s"
def getObjName(filename, archId=-1):
	d = g_arch[archId][0] + "/" if archId!=-1 else ""
	basename = os.path.basename(filename)
	return d + os.path.splitext(basename)[0] + ".out"

def constructLlcQuery(filename, additionalParams, archId):
	newName = getAsmName(filename, archId)
	llc = g_arch[archId][1]
	return llc + " " + additionalParams + " " + filename + " -o " + newName
def constructGccQuery(filename, additionalParams, archId):
	newName = getObjName(filename, archId)
	gcc = g_arch[archId][2]
	return gcc + " " + additionalParams + " " + filename + " -o " + newName


def getBinaryCodeSize(filename):
	err, out = createProcess("size " + filename)
	return int(out.split()[6])

def compareBinaryFileSizes(filename, additionalParams = ""):
	if (not os.path.exists(filename)):
		raise Exception("File " + filename + " does not exist")

	extension = os.path.splitext(filename)[1]
	if extension != ".ll" and extension != ".bc":
		raise Exception("Fomat must be .ll or .bc. Your format: " + extension)

	if not os.path.exists(g_factoredDir):
		os.makedirs( g_factoredDir)

	query = constructOptWithLoadQuery(filename,"-S " + additionalParams, getFactoredName(filename))

	retcode, out = createProcess(query)
	if (retcode != 0):
		raise Exception("Query: " + query + "\nError: " + out)

	if not os.path.exists(g_arch[0][0]):
		os.makedirs(g_arch[0][0])
	if not os.path.exists(g_arch[1][0]):
		os.makedirs(g_arch[1][0])
	result = []
	for i in range(0,2):
		sizes = []
		for name in [filename, getFactoredName(filename)]:
#create asm files
			query = constructLlcQuery(name, "", i)
			retcode, out = createProcess(query)
			if (retcode != 0):
				raise Exception("Query: " + query + "\nError: " + out)
#create binary files
			query = constructGccQuery(getAsmName(name, i), "", i)
			retcode, out = createProcess(query)
			if (retcode != 0):
				raise Exception("Query: " + query + "\nError: " + out)
#calculate executable size
			sizes += [ getBinaryCodeSize(getObjName(name, i)) ]
		result += [sizes]
	return result


