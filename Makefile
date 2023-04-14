include makefile.shared

CC=clang
CFLAGS=-std=c11 -O0 -fsanitize=address -g -fno-common -Wall -Werror -Wno-switch -pthread
# -O1 -fsanitize=address

SRCS += codegen.linux.c

OBJS=$(SRCS:.c=.o)

dyibicc: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ -ldl $(LDFLAGS)

$(OBJS): dyibicc.h libdyibicc.h

minilua: dynasm/minilua.c
	$(CC) $(CFLAGS) -o $@ $^ -lm

codegen.linux.c: codegen.in.c minilua
	./minilua dynasm/dynasm.lua -o $@ codegen.in.c

test: testrun.lua minilua dyibicc
	./minilua testrun.lua linux

# Misc.

clean:
	rm -rf dyibicc codegen.linux.c test/*.s test/*.exe minilua minilua.exe
	find * -type f '(' -name '*~' -o -name '*.o' ')' -exec rm {} ';'
	clang-format -i *.c *.h

.PHONY: test clean
