Before running tests or any utility, make sure, that the path for bbfactoring library is set correctly in utilities/constants.py (variable g_loadOptimization)

####tests.py
Simple tests, that run lli to check output equality of  modules
Run: ./tests.py

####lit.cfg
Config suite for LIT. For running, LLVM utils should be installed (FileCheck, lit)
Run: lit testCases

####compare.py
Auxiliary utility for comparing sizes of factored and non-factored object files for arm and x64 architecture
Executable creates factored file, assembler files and object files for both: factored and non-factored programm for each architecture
Run help: ./compare.py -h

####merge.py
Tool uses llvm-link to merge source files into solo file. Accepts not only .bb and .ll files, but also higher level (like .c, .cpp)

####utilities
Directory contains shared python files
