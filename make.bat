clang -D_CRT_SECURE_NO_WARNINGS -O0 dynasm/minilua.c -o minilua.exe
minilua.exe dynasm/dynasm.lua -o codegen.c codegen.in.c
clang -D_CRT_SECURE_NO_WARNINGS -std=c11 -g -fno-common -Wall -Wno-switch codegen.c hashmap.c main.c parse.c preprocess.c strings.c tokenize.c type.c unicode.c alloc.c dyo.c link.c -o dyibicc.exe
