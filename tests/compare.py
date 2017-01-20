#!/usr/bin/python

import os, subprocess, sys, os.path, shutil, glob
from utilities.functions import compareBinaryFileSizes
from utilities.constants import g_optWithLoad, g_arch, g_armDir, g_x64Dir, g_factoredDir, g_cgreen, g_cred, g_cyellow, g_cend

def help():
	print('Program builds 2 binary files from llvm bitcode (.ll, .bc) for native (or specified architectures):  without optimizaton, with optimization. \nFurther it compares its code sizes\nIf program has unresolved links, use -c flag')

def applyCompare(filenames):
	print("Original Size | sign | Optimized size")
	for filename in filenames:
		print("File:", filename)
		try:
			result = compareBinaryFileSizes(filename)
			i = 0
			for sizes in result:
				if sizes[0] < sizes[1]:
					print(g_arch[i][0] + ":" + g_cred, sizes[0], " < ", sizes[1], g_cend)
				elif sizes[0] == sizes[1]:
					print(g_arch[i][0] + ":", sizes[0], "==", sizes[1])
				else:
					print(g_arch[i][0] + ":", g_cgreen + str(sizes[0]), " > ", sizes[1], g_cend)
				i+=1
		except Exception as inst:
			print(g_cyellow + str(inst) + g_cend)

#start program

if len(sys.argv) < 2:
	print('File argument required\n use --help for info')
	sys.exit()

if (sys.argv[1] == "--help"):
	help()
	sys.exit()

if (sys.argv[1] == "--clean"):
	for dirName in [g_armDir, g_x64Dir, g_factoredDir]:
		if os.path.exists(dirName):
			shutil.rmtree(dirName)			
	sys.exit()

firstFilename = 1
#helps if we have some unresolved links
if sys.argv[1] == "-c":
	g_arch[0][2] += " -c"
	g_arch[1][2] += " -c"
	firstFilename+=1

filenames = []
for i in range(firstFilename, len(sys.argv)):
	filename = sys.argv[i]
	if not os.path.exists(filename):
		print("File ", filename, " does not exist")
		continue
	if os.path.isdir(filename):
		for f in glob.glob(filename + "/*.ll") + glob.glob(filename + "*.bc"):
			filenames += [f]
	else:
		filenames += [filename]

applyCompare(filenames)

