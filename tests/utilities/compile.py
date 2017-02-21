#!/usr/bin/python

import os.path
import subprocess
from copy import copy
from utilities.constants import *

#auxiliary var and funcs for additional arguments
g_addArgs = {}

def parseAdditionalArguments(str):
    if str == "":
        return ""
    if str == "internalize":
        addArg(g_link, "--" + str)
        return ""
    splitted= str.split(":")
    if len(splitted) != 2:
        print("Can't parse additional argument:", str)
        return ""
    addArg(splitted[0], splitted[1])
    return ""

def getArg(prog):
    return "" if not prog in g_addArgs else g_addArgs[prog]

def addArg(key, val):
    if not key in g_addArgs:
        g_addArgs[key] = ""
    g_addArgs[key] += val + ' '

#end compile args

def createProcess(query):
    process = subprocess.Popen(
        query.split(), stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    ecode = process.wait()
    out, err = process.communicate()
    return (ecode, out.decode("utf-8"), err.decode("utf-8"))


class Compile:
    def __initArchInfo(self, archInfo):
        if archInfo == "":
            self.arch = ArchInfo("")
            return
        if not archInfo in g_arches:
            raise Exception("Unknown arch: " + archInfo)
        compilesToArchInfo = g_arches[archInfo]
        prog = self.compileInfo.program
        if not prog in compilesToArchInfo:
            raise Exception("Utility " + prog + " not found in arch settings " + archInfo)
        self.arch = copy(compilesToArchInfo[prog])

    def __initCompileInfo(self, compileInfo, filename):
        if compileInfo is None:
            self.compileInfo = copy(g_filenamesTo[os.path.splitext(filename)[1]])
        else:
            assert isinstance(compileInfo, CompileInfo)
            self.compileInfo = copy(compileInfo)

    def __initOutputFile(self, filename, arch):
        filepath = os.path.split(filename)[1]
        shortname = os.path.splitext(filepath)[0]
        archAdd = "" if arch == "" else arch + "/"
        self.outputFile = g_commonDir + "/" + archAdd + shortname + self.compileInfo.ext

    def __init__(self, filename, archInfo, compileInfo=None):
        self.inputFile = filename
        self.__initCompileInfo(compileInfo, filename)
        assert isinstance(self.compileInfo, CompileInfo), "Bad CompileInfo"
        self.__initArchInfo(archInfo)
        assert isinstance(self.arch, ArchInfo), "Bad ArchInfo"
        self.__initOutputFile(filename, archInfo)

    def __createDirs(outputFile):
        if outputFile == "":
            return
        path = os.path.split(outputFile)[0]
        if not os.path.exists(path):
            os.makedirs(path)

    def __createQuery(self, extraArgs):
        output = "" if self.outputFile == "" else " -o " + self.outputFile
        return self.compileInfo.program + " " + self.inputFile + " " + self.compileInfo.args + " " + self.arch.args + " " + extraArgs + output

    def compile(self, args = ""):
        extraArgs = getArg(self.compileInfo.program) if args == "" else args
        Compile.__createDirs(self.outputFile)
        query = self.__createQuery(extraArgs)
        (self.errorCode, self.output, self.error) = createProcess(query)
        if self.errorCode != 0:
            raise Exception("Query: " + query + "\nError: " + self.error)
        return (self.errorCode, self.output, self.error)
