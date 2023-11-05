# A bunch of hacks to get the magic containers we support built from the
# standard STC distribution.
#
# Use STC's singleheader.py to get a standalone header for each of our containers
# Remove all system <> includes (stdint, inttypes, etc.)
# Rewrite c_assert (it's messy to include and our test code uses its own assert).
# undef __attribute__ since some sys/cdefs.h defines __attribute__ away (o_O)
# Add __attribute__((methodcall(...))) to appropriate places
# Replace bool with _Bool (as stdbool is a define not a typedef)
# Prefix in whatever prototypes/typedefs are necessary/were lost from dropping includes above
# TODO: Strip the __cplusplus branches

import os
import re
import subprocess
import sys

REPOROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), os.pardir))
SCRIPT_NAME = os.path.basename(__file__)

COMMON_PREFIX = [
            "// BEGIN " + SCRIPT_NAME + "\n",
            "#ifndef __dyibicc_internal_include__\n",
            "#error Can only be included by the compiler, or confusing errors will result!\n",
            "#endif\n",
            "#undef __attribute__\n",
            "typedef unsigned char uint8_t;\n",
            "typedef unsigned long long uint64_t;\n",
            "typedef unsigned long long size_t;\n",
            "typedef long long int64_t;\n",
            "typedef long long intptr_t;\n",
            "typedef unsigned int uint32_t;\n",
            "void* memset(void* dest, int ch, size_t count);\n",
            "void* memcpy(void* dest, const void* src, size_t count);\n",
            "int memcmp(const void* lhs, const void* rhs, size_t count);\n",
            "void* memmove(void* dest, const void* src, size_t count);\n",
            "size_t strlen(const char* str);\n",
            "void free(void* ptr);\n",
            "void *malloc(size_t size);\n",
            "void *calloc(size_t num, size_t size);\n",
            "void* realloc(void* ptr, size_t new_size);\n",
            "#define NULL ((void*)0) /* todo! */\n",
            "// ### END " + SCRIPT_NAME + "\n",
        ]

CONTAINERS = [
    {
        "in": os.path.join(REPOROOT, "scratch/STC/include/stc/cvec.h"),
        "out": os.path.join(REPOROOT, "include/all/_vec.h"),
        "prefix": COMMON_PREFIX,
    },
    {
        "in": os.path.join(REPOROOT, "scratch/STC/include/stc/cmap.h"),
        "out": os.path.join(REPOROOT, "include/all/_map.h"),
        "prefix": COMMON_PREFIX,
    },
]

def main():
    os.chdir(REPOROOT)
    if not os.path.isdir("scratch"):
        os.makedirs("scratch")
    os.chdir("scratch")
    subprocess.run(["git", "clone", "https://github.com/stclib/STC.git", "STC"])
    os.chdir("STC")

    for c in CONTAINERS:
        subprocess.run([sys.executable, "src/singleheader.py",
                        c["in"], c["out"]])
        with open(c["out"], "r", encoding="utf-8") as f:
            contents = f.readlines()
        munged = []
        for line in contents:

            m_inc = re.search('^\\s*# *include\\s*[<"](.+)[>"]', line)
            if m_inc:
                line = '// EXCLUDED BY %s' % SCRIPT_NAME + ' ' + line

            m_typedef = re.search('^\\s*typedef struct SELF {(.*)', line)
            if m_typedef:
                line = ('typedef struct __attribute__((methodcall(SELF##_))) SELF {' +
                        m_typedef.group(1) + '\n')

            line = re.sub(r'\bbool\b', '_Bool', line)
            line = re.sub(r'\bfalse\b', '0', line)
            line = re.sub(r'\btrue\b', '1', line)

            m_defassert = re.search('^\\s*#define\\s+c_assert.*assert.*', line)
            if m_defassert:
                line = ('#ifndef STC_ASSERT\n'
                        '#define STC_ASSERT(expr)\n'
                        '#endif\n'
                        '    #define c_assert(expr)      STC_ASSERT(expr)\n')

            munged.append(line)
        contents = c["prefix"] + munged
        with open(c["out"], "w", encoding="utf-8", newline="\n") as f:
            f.writelines(contents)

    return 0

if __name__ == "__main__":
    sys.exit(main())
