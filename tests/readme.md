Make sure, thar you set the right pass for bbfactoring library in utilities/constants.py (variable g_loadOptimization)

tests.py ~ simple tests, that check whether blocks were factored
Run: ./tests.py

compare.py ~ auxiliary utility for comparing sizes of factored and non-factored object files for arm and x64 architecture
Executable creates factored file, assembler files and object files for both: factored and non-factored programm for each architecture
Run: ./compare.py [-c] filenameOrDir1 [filenameOrDir2 ...]

utilites directory contains non-runnable shared python files
