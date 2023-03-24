CFLAGS=-std=c11 -g -fno-common -Wall -Werror -Wno-switch -pthread -Wunreachable-code -Wmisleading-indentation

SRCS=codegen.c hashmap.c main.c parse.c preprocess.c strings.c tokenize.c type.c unicode.c alloc.c dyo.c link.c
OBJS=$(SRCS:.c=.o)

# XXX:
# test/asm.c
#   dyibicc passes asm() blocks directly through to the system assembler, so now
#   that we're not using that, there's no assembler available.
#
# test/tls.c
#   TLS not implemented
#
# test/commonsym.c
#   currently passes, but I thought I disabled support, so need to investigate.
#
TEST_SRCS=\
test/alignof.c \
test/alloca.c \
test/arith.c \
test/atomic.c \
test/attribute.c \
test/bitfield.c \
test/builtin.c \
test/cast.c \
test/commonsym.c \
test/compat.c \
test/complit.c \
test/const.c \
test/constexpr.c \
test/control.c \
test/decl.c \
test/enum.c \
test/extern.c \
test/float.c \
test/function.c \
test/generic.c \
test/initializer.c \
test/line.c \
test/literal.c \
test/macro.c \
test/offsetof.c \
test/pointer.c \
test/pragma-once.c \
test/sizeof.c \
test/stdhdr.c \
test/string.c \
test/struct.c \
test/typedef.c \
test/typeof.c \
test/usualconv.c \
test/unicode.c \
test/union.c \
test/varargs.c \
test/variable.c \
test/vla.c

dyibicc: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ -ldl $(LDFLAGS)

dumpdyo: dumpdyo.o dyo.o hashmap.o alloc.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(OBJS): dyibicc.h

minilua: dynasm/minilua.c
	$(CC) $(CFLAGS) -o $@ $^ -lm

codegen.c: codegen.in.c minilua
	./minilua dynasm/dynasm.lua -o $@ codegen.in.c

test: $(TEST_SRCS) dyibicc
	for i in $(filter-out dyibicc,$^); do echo $$i; ./dyibicc -Itest test/common.c $$i || exit 1; echo; done

# Misc.

clean:
	rm -rf dyibicc dumpdyo codegen.c tmp* test/*.s test/*.exe minilua minilua.exe *.dyo *.exe *.pdb *.ilk
	find * -type f '(' -name '*~' -o -name '*.o' ')' -exec rm {} ';'
	clang-format -i *.c *.h

.PHONY: test clean test-stage2 dumpdyo
