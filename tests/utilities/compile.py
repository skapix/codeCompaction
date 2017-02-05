#!/usr/bin/python

import os.path
import subprocess
from utilities.constants import *


def createProcess(query):
    process = subprocess.Popen(
        query.split(), stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    ecode = process.wait()
    out, err = process.communicate()
    return (ecode, out.decode("utf-8")) if ecode == 0 else (ecode, err.decode("utf-8"))


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
        self.arch = compilesToArchInfo[prog]

    def __initCompileInfo(self, compileInfo, filename):
        if compileInfo is None:
            self.compileInfo = g_filenamesTo[os.path.splitext(filename)[1]]
        else:
            assert isinstance(compileInfo, CompileInfo)
            self.compileInfo = compileInfo

    def __initOutputFile(self, filename, arch):
        filepath = os.path.split(filename)[1]
        shortname = os.path.splitext(filepath)[0]
        self.outputFile = g_commonDir + "/" + arch + \
            "/" + shortname + self.compileInfo.ext

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

    def compile(self, extraArgs):
        Compile.__createDirs(self.outputFile)
        query = self.__createQuery(extraArgs)
        (self.errorCode, self.output) = createProcess(query)
        if self.errorCode != 0:
            raise Exception("Query: " + query + "\nError: " + self.output)
