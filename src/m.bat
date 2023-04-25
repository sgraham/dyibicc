@echo off
cl /W4 /Wall /WX /Zi /nologo /fsanitize=address dyn_basic_pdb_test.c /link /out:dyn_basic_pdb_test.exe
dyn_basic_pdb_test.exe
"C:\Program Files\LLVM\bin\llvm-pdbutil.exe" dump ..\scratch\pdb\dbp.pdb >..\scratch\pdb\dbp.txt

