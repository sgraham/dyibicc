#!/usr/bin/env python3

import os
import shutil
import subprocess
import sys


# Update gen.py too, except entry.c.
#
# Order is important because of static initialized data that can't be forward
# declared.
FILELIST = [
    'src/dyibicc.h',
    'src/khash.h',
    'src/dynasm/dasm_proto.h',
    'src/dynasm/dasm_x86.h',
    'src/type.c',
    'src/alloc.c',
    'src/util.c',
    'src/hashmap.c',
    'src/link.c',
    'src/main.c',
    'src/parse.c',
    'src/preprocess.c',
    'src/tokenize.c',
    'src/unicode.c',
]


def push_disable_dynasm_warnings(f):
    f.write('#ifdef _MSC_VER\n')
    f.write('#pragma warning(push)\n')
    f.write('#pragma warning(disable: 4127)\n')
    f.write('#pragma warning(disable: 4242)\n')
    f.write('#pragma warning(disable: 4244)\n')
    f.write('#endif\n')


def pop_dynasm_warnings(f):
    f.write('#ifdef _MSC_VER\n')
    f.write('#pragma warning(pop)\n')
    f.write('#endif\n')


def include_file(f, src):
    with open(src, 'r') as i:
        f.write('#undef C\n')
        f.write('#undef L\n')
        f.write('#undef VOID\n')
        if 'dynasm/' in src:
            push_disable_dynasm_warnings(f)
        f.write('//\n// START OF %s\n//\n' % src)
        for line in i.readlines():
            if line.startswith('#include "dyibicc.h"'):
                continue
            if line.startswith('#include "khash.h"'):
                continue
            if line.startswith('#include "dynasm/dasm_proto.h"'):
                continue
            if line.startswith('#include "dynasm/dasm_x86.h"'):
                continue
            if line.startswith('IMPLEXTERN '):
                continue
            f.write(line.replace('IMPLSTATIC ', 'static ').replace('DASM_FDEF ', 'static '))
        f.write('//\n// END OF %s\n//\n' % src)
        if 'dynasm/' in src:
            pop_dynasm_warnings(f)


def main():
    if not os.path.isfile('out/wr/codegen.w.c') or not os.path.isfile('out/lr/codegen.l.c'):
        print('Must first do both a `m.bat r test` and `./m r test`.')
        return 1

    if not os.path.isdir('embed'):
        os.makedirs('embed')
    with open('embed/libdyibicc.c', 'w', newline='\n') as f:
        for src in FILELIST:
            include_file(f, src)

        f.write('#if X64WIN\n')
        include_file(f, 'out/wr/codegen.w.c')
        f.write('#else // ^^^ X64WIN / !X64WIN vvv\n')
        include_file(f, 'out/lr/codegen.l.c')
        f.write('#endif // !X64WIN\n')
    shutil.copyfile('src/libdyibicc.h', 'embed/libdyibicc.h')
    shutil.copyfile('LICENSE', 'embed/LICENSE')

    # Smoke test that there's no warnings, etc. Currently only on host platform.
    os.chdir('embed')
    if sys.platform == 'win32':
        subprocess.check_call(['cl', '/nologo', '/W4', '/Wall', '/WX', '/c', 'libdyibicc.c'])
    elif sys.platform == 'linux':
        subprocess.check_call(['gcc', '-Wall', '-Wextra', '-Werror', '-c', 'libdyibicc.c'])
        subprocess.check_call(['clang', '-Wall', '-Wextra', '-Werror', '-c', 'libdyibicc.c'])
        symsp = subprocess.run(['readelf', '-s', 'libdyibicc.o'], capture_output=True)
        syms = str(symsp.stdout, encoding='utf-8').splitlines()
        syms = [l for l in syms if 'GLOBAL ' in l]
        syms = [l for l in syms if 'UND ' not in l]
        print('These are the global exported symbols from libdyibicc.o,')
        print('check that they match libdyibicc.h (only).')
        print('-'*80)
        for s in syms:
            print(s)
        print('-'*80)

    return 0


if __name__ == '__main__':
    sys.exit(main())
