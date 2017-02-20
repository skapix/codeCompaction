#!/usr/bin/python

import glob
import sys
import argparse
import os.path
import shutil
import tempfile
import atexit
from utilities.constants import *
from utilities.functions import getExt, printError
from utilities.compile import *

def checkIfIR(str):
    ext = os.path.splitext(str)[1]
    if ext != '.bc' and ext != '.ll':
        msg = "Supported extensions: '.ll', '.bc'. Your extension: '%s'" % ext
        raise argparse.ArgumentTypeError(msg)
    return str

def getFilesFromFolder(folder, extensions, isRec):
    assert os.path.isdir(folder), filename + "is not a folder"
    if folder[-1] != '/':
            folder += "/"
    result = []
    pattern = "**/*" if isRec else "*"
    for ext in extensions:
        result += glob.glob(folder + pattern + ext, recursive=isRec)
    return result

def getFiles(fileFolderNames, extensions, isRec):
    result = []
    for filename in fileFolderNames:
        if not os.path.exists(filename):
            print("File ", filename, " does not exist")
            continue
        if os.path.isdir(filename):
            result += getFilesFromFolder(filename, extensions, isRec)
        else:
            result += [filename]
    return result


def createTmp():
    result = tempfile.mkdtemp()
    atexit.register(shutil.rmtree, result)
    return result

def transformToBc(filenames, arch, tmpDir):
    result = []
    for filename in filenames:
        if getExt(filename) == ".bc" or getExt(filename) == ".ll":
            result += [filename]
            continue
        comp = Compile(filename, arch)
        comp.outputFile = tmpDir + "/" + os.path.splitext(filename)[0] + ".ll"
        try:
            comp.compile()
        except Exception as inst:
            printError(str(inst))
            continue
        result += [comp.outputFile]

    return result

def llvmLink(filenames, outputFile):
    filenameQuery = " ".join(x for x in filenames)
    addReadability = " -S" if getExt(outputFile) == ".ll" else ""
    addArgs = getArg(g_link)
    query = g_link + " " + filenameQuery + addReadability + " " + addArgs + "-o " + outputFile
    (errCode, out, error) = createProcess(query)
    if errCode != 0:
        printError("Query: " + query + "\nError: " + error)
    return errCode


if __name__ == "__main__":
    #prepare parser
    parser = argparse.ArgumentParser(
    description='Auxiliary utility for compiling and  linking all matched extensions into single IR with llvm-link')

    parser.add_argument('filenames', nargs='+', help='Filenames to be merged')
    parser.add_argument('--ext', nargs='*', choices=g_filenamesTo.keys(),
                        default=g_filenamesTo.keys(), help='Select file extensions')
    parser.add_argument('--arch', nargs='?', choices=g_arches.keys(), default="",
                        help='Choose architecture. Native by default')
    parser.add_argument('--args', nargs='*', default="", type=parseAdditionalArguments,  help="Additional args in view like utility:'extra flags'")
    parser.add_argument('-r', action='store_true', help='Search files in folders recursively')
    parser.add_argument('-o', metavar='filename', type=checkIfIR,
                        default='a.ll', help='Output file, \'a.ll\' by default')

    args = parser.parse_args()
    #start processing filenames

    filenames = getFiles(args.filenames, args.ext, args.r)
    # filter duplicate filenames
    filenames = list(set(filenames))

    if len(filenames) <= 1:
        print("Insufficient amount of filenames")
        sys.exit()

    tmpDir = createTmp()
    try:
        filenames = transformToBc(filenames, args.arch, tmpDir)
        if len(filenames) > 1:
            errCode = llvmLink(filenames, args.o)
        else:
            printError("Not enough filenames to merge")
    except Exception as e:
        printError(str(e))
