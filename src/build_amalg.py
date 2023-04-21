#!/usr/bin/env python3

import os
import shutil
import subprocess
import sys


HEADER = '''\
//
// Amalgamated (single file) build of https://github.com/sgraham/dyibicc.
// Revision: %s
//
// This file should not be edited or modified, patches should be to the
// non-amalgamated files in src/. The user-facing API is in libdyibicc.h
// which should be located in the same directory as this .c file.
//

'''


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
            if line.startswith('#include "../include/all/reflect.h"'):
                continue
            if line.startswith('#pragma once'):
                continue
            if line.startswith('IMPLEXTERN '):
                continue
            f.write(line.replace('IMPLSTATIC ', 'static ').replace('DASM_FDEF ', 'static '))
        f.write('//\n// END OF %s\n//\n' % src)
        if 'dynasm/' in src:
            pop_dynasm_warnings(f)


def main():
    out_dir = sys.argv[1]
    root_src = sys.argv[2]
    if not os.path.isdir(out_dir):
        os.makedirs(out_dir)
    with open(os.path.join(out_dir, 'libdyibicc.c'), 'w', newline='\n') as f:
        rev_parse = subprocess.run(['git', 'rev-parse', 'HEAD'], capture_output=True)
        cur_rev = rev_parse.stdout.decode('utf-8').strip()
        f.write(HEADER % cur_rev)
        for src in sys.argv[3:]:
            include_file(f, src)

        f.write('#if X64WIN\n')
        include_file(f, 'codegen.w.c')
        f.write('#else // ^^^ X64WIN / !X64WIN vvv\n')
        include_file(f, 'codegen.l.c')
        f.write('#endif // !X64WIN\n')
    shutil.copyfile(os.path.join(root_src, 'libdyibicc.h'), os.path.join(out_dir, 'libdyibicc.h'))
    shutil.copyfile(os.path.join(root_src, '..', 'LICENSE'), os.path.join(out_dir, 'LICENSE'))
    shutil.copytree(os.path.join(root_src, '..', 'include'), os.path.join(out_dir, 'include'),
                    dirs_exist_ok=True)

    # Smoke test that there's no warnings, etc. Currently only on host platform.
    os.chdir(out_dir)
    if sys.platform == 'win32':
        subprocess.check_call(['cl', '/nologo', '/W4', '/Wall', '/WX', '/c', 'libdyibicc.c'])
        os.remove('libdyibicc.obj')
    elif sys.platform == 'linux':
        subprocess.check_call(['gcc', '-Wall', '-Wextra', '-Werror', '-c', 'libdyibicc.c'])
        subprocess.check_call(['clang', '-Wall', '-Wextra', '-Werror', '-c', 'libdyibicc.c'])
        symsp = subprocess.run(['readelf', '-s', 'libdyibicc.o'], capture_output=True)
        os.remove('libdyibicc.o')
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
