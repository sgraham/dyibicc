import base64
import glob
import json
import os
import pathlib
import sys

# Assumes we're in $root/scripts.
ROOT_DIR = os.path.normpath(
    os.path.join(os.path.abspath(os.path.dirname(__file__)), '..'))

# This order is important for the amalgamated build because of static
# initialized data that can't be forward declared.
FILELIST = [
    'type.c',
    'alloc.c',
    'entry.c',
    'fuzz_entry.c',
    'hashmap.c',
    'link.c',
    'main.c',
    'parse.c',
    'preprocess.c',
    'tokenize.c',
    'unicode.c',
    'util.c',
]

CONFIGS = {
    'w': {
        'd': {
            'COMPILE': 'cl /showIncludes /nologo /FS /Od /Zi /D_DEBUG /DIMPLSTATIC= /DIMPLEXTERN=extern /D_CRT_SECURE_NO_DEPRECATE /W4 /WX /I$root /I. /c $in /Fo:$out /Fd:$out.pdb',
            'LINK': 'link /nologo gdi32.lib user32.lib onecore.lib /DEBUG $in /out:$out /pdb:$out.pdb',
            'ML': 'cl /nologo /wd4132 /wd4324 $in /link /out:$out',
            'TESTCEXE': 'cl /nologo /Zi /FS /D_CRT_SECURE_NO_WARNINGS /Iembed /W4 /Wall /WX $in /link /debug onecore.lib user32.lib /out:$out',
        },
        'r': {
            'COMPILE': 'cl /showIncludes /nologo /FS /Ox /GL /Zi /DNDEBUG /DIMPLSTATIC= /DIMPLEXTERN=extern /D_CRT_SECURE_NO_DEPRECATE /W4 /WX /I$root /I. /c $in /Fo$out /Fd:$out.pdb',
            'LINK': 'link /nologo gdi32.lib user32.lib onecore.lib /LTCG /DEBUG /OPT:REF /OPT:ICF $in /out:$out /pdb:$out.pdb',
            'ML': 'cl /nologo /wd4132 /wd4324 $in /link /out:$out',
            'TESTCEXE': 'cl /nologo /FS /D_CRT_SECURE_NO_WARNINGS /Iembed /W4 /Zi /Ox /GL /Wall /WX /wd4710 /wd4711 $in /link onecore.lib user32.lib /LTCG /DEBUG /out:$out',
        },
        'a': {
            'COMPILE': 'cl /showIncludes /nologo /FS /Od /fsanitize=address /Zi /D_DEBUG /DIMPLSTATIC= /DIMPLEXTERN=extern /D_CRT_SECURE_NO_DEPRECATE /W4 /WX /I$root /I. /c $in /Fo:$out /Fd:$out.pdb',
            'LINK': 'link /nologo gdi32.lib user32.lib onecore.lib /DEBUG $in /out:$out /pdb:$out.pdb',
            'ML': 'cl /nologo /wd4132 /wd4324 $in /link /out:$out',
            'TESTCEXE': 'cl /nologo /FS /D_CRT_SECURE_NO_WARNINGS /Iembed /W4 /Wall /WX $in /link /DEBUG onecore.lib user32.lib /out:$out',
        },
        '__': {
            'exe_ext': '.exe',
            'obj_ext': '.obj',
        },
    },
    'l': {
        'f': {
            'COMPILE': 'clang -fsanitize=fuzzer -std=c11 -MMD -MT $out -MF $out.d -g -O0 -fcolor-diagnostics -fno-common -Wall -Werror -Wno-switch -DNDEBUG -DIMPLSTATIC= -DIMPLEXTERN=extern -pthread -I$root -I. -c $in -o $out',
            'LINK': 'clang -fsanitize=fuzzer -o $out $in -pthread -lm -ldl -g',
            'ML': 'clang -o $out $in -lm',
            'TESTCEXE': 'clang -fsanitize=fuzzer -Iembed -Wall -Wextra -Werror -ldl -lm -o $out $in',
        },
        'd': {
            'COMPILE': 'clang -std=c11 -MMD -MT $out -MF $out.d -g -O0 -fcolor-diagnostics -fno-common -Wall -Werror -Wno-switch -DNDEBUG -DIMPLSTATIC= -DIMPLEXTERN=extern -pthread -I$root -I. -c $in -o $out',
            'LINK': 'clang -o $out $in -pthread -lm -ldl -g',
            'ML': 'clang -o $out $in -lm',
            'TESTCEXE': 'clang -Iembed -Wall -Wextra -Werror -ldl -lm -o $out $in',
        },
        'r': {
            'COMPILE': 'clang -std=c11 -MMD -MT $out -MF $out.d -g -Oz -fcolor-diagnostics -fno-common -Wall -Werror -Wno-switch -D_DEBUG -DIMPLSTATIC= -DIMPLEXTERN=extern -pthread -c -I$root -I. $in -o $out',
            'LINK': 'clang -o $out $in -pthread -lm -ldl -g',
            'ML': 'clang -o $out $in -lm',
            'TESTCEXE': 'clang -Iembed -Wall -Wextra -Werror -ldl -lm -Oz -o $out $in',
        },
        'a': {
            'COMPILE': 'clang -std=c11 -MMD -MT $out -MF $out.d -g -O0 -fsanitize=address -fcolor-diagnostics -fno-common -Wall -Werror -Wno-switch -D_DEBUG -DIMPLSTATIC= -DIMPLEXTERN=extern -pthread -c -I$root -I. $in -o $out',
            'LINK': 'clang -fsanitize=address -o $out $in -pthread -lm -ldl -g',
            'ML': 'clang -o $out $in -lm',
            'TESTCEXE': 'clang -Iembed -Wall -Wextra -Werror -ldl -lm -fsanitize=address -o $out $in',
        },
        '__': {
            'exe_ext': '',
            'obj_ext': '.o',
        },
    },
}


def get_tests():
    tests = {}
    for test in glob.glob(os.path.join('test', '*.c')):
        test = test.replace('\\', '/')
        run = '-Itest test/common.c {self}'
        ret = '0'
        txt = ''
        disabled = False
        run_prefix = '// RUN: '
        ret_prefix = '// RET: '
        txt_prefix = '// TXT: '
        disabled_prefix = '// DISABLED'
        with open(test, 'r', encoding='utf-8') as f:
            for l in f.readlines():
                if l.startswith(run_prefix): run = l[len(run_prefix):].rstrip()
                if l.startswith(ret_prefix): ret = l[len(ret_prefix):].rstrip()
                if l.startswith(txt_prefix): txt += l[len(txt_prefix):].rstrip() + '\n'
                if l.startswith(disabled_prefix): disabled = True
            def sub(t):
                return t.replace('{self}', test)
            if not disabled:
                tests[test] = {'run': sub(run), 'ret': int(ret), 'txt': sub(txt)}
    return tests


def get_upd_tests():
    tests = []
    for test in glob.glob(os.path.join('test', 'update_*.py')):
        test = test.replace('\\', '/')
        tests.append(test)
    return tests


def get_fuzz_tests():
    tests = []
    for test in glob.glob(os.path.join('test', 'fuzzcases', '*')):
        test = test.replace('\\', '/')
        tests.append(test)
    return tests


def generate(platform, config, settings, cmdlines, tests, upd_tests, fuzz_tests):
    root_dir = os.path.join('out', platform + config)
    if not os.path.isdir(root_dir):
        os.makedirs(root_dir)

    exe_ext = settings['exe_ext']
    obj_ext = settings['obj_ext']

    with open(os.path.join(root_dir, 'build.ninja'), 'w', newline='\n') as f:
        f.write('root = ../../src\n')
        f.write('\n')
        f.write('rule cc\n')
        f.write('  command = ' + cmdlines['COMPILE'] + '\n')
        f.write('  description = CC $out\n')
        f.write('  deps = ' + ('msvc' if platform == 'w' else 'gcc') + '\n')
        if platform != 'w':
            f.write('  depfile = $out.d')
        f.write('\n')
        f.write('rule link\n')
        f.write('  command = ' + cmdlines['LINK'] + '\n')
        f.write('  description = LINK $out\n')
        f.write('\n')
        f.write('rule mlbuild\n')
        f.write('  command = ' + cmdlines['ML'] + '\n')
        f.write('  description = CC $out\n')
        f.write('\n')
        f.write('rule dynasm_w\n')
        f.write('  command = ./minilua' + exe_ext + ' $root/dynasm/dynasm.lua -D WIN -o $out $in\n')
        f.write('  description = DYNASM $out\n')
        f.write('\n')
        f.write('rule dynasm_l\n')
        f.write('  command = ./minilua' + exe_ext + ' $root/dynasm/dynasm.lua -D SYSV -o $out $in\n')
        f.write('  description = DYNASM $out\n')
        f.write('\n')
        f.write('rule amalg\n')
        f.write('  command = %s $root/build_amalg.py embed $root $in\n' % sys.executable)
        f.write('\n')
        f.write('rule compincl\n')
        f.write('  command = %s $root/build_compiler_includes_header.py $out $in\n' % sys.executable)
        f.write('\n')

        objs = []

        # codegen required for both platforms (not just |platform|) to bundle
        # into amalgamated build.
        dynasmed_l = 'codegen.l.c'
        dynasmed_w = 'codegen.w.c'
        f.write('build %s: dynasm_l $root/codegen.in.c | minilua%s\n' % (dynasmed_l, exe_ext))
        f.write('build %s: dynasm_w $root/codegen.in.c | minilua%s\n' % (dynasmed_w, exe_ext))
        # But only the current one linked into the compiler binary.
        dynasmed = dynasmed_w if platform == 'w' else dynasmed_l
        obj = os.path.splitext(dynasmed)[0] + obj_ext
        f.write('build %s: cc %s\n' % (obj, dynasmed))
        objs.append(obj)

        for src in FILELIST:
            if config == 'f' and src == 'entry.c': continue
            if config != 'f' and src == 'fuzz_entry.c': continue
            obj = os.path.splitext(src)[0] + obj_ext
            objs.append(obj)
            extra_deps = ' | compincl.h' if src == 'preprocess.c' else ''
            f.write('build %s: cc $root/%s%s\n' % (obj, src, extra_deps))

        compiler_include_headers = pathlib.Path('include').rglob('*.h')
        f.write('build compincl.h: compincl %s | $root/build_compiler_includes_header.py\n' %
                (' '.join([os.path.join('$root', '..', x).replace('\\', '/')
                           for x in compiler_include_headers])))

        EXTRAS_FOR_AMALG = [
                '$root/dyibicc.h',
                '$root/../include/all/reflect.h',
                'compincl.h',
                '$root/dynasm/dasm_proto.h',
                '$root/dynasm/dasm_x86.h',
        ]
        EXTRAS_FOR_AMALG_LATE = [
                '$root/dyn_basic_pdb.h',
        ]
        f.write('build embed/libdyibicc.c embed/libdyibicc.h embed/LICENSE: amalg %s %s %s | %s %s $root/build_amalg.py\n' %
                (' '.join(EXTRAS_FOR_AMALG),
                 ' '.join('$root/' + x for x in FILELIST if 'entry.c' not in x),
                 ' '.join(EXTRAS_FOR_AMALG_LATE),
                 dynasmed_w,
                 dynasmed_l))

        f.write('build libdyibicc%s: cc embed/libdyibicc.c\n' % obj_ext)

        dyibiccexe = 'dyibicc' + exe_ext
        f.write('build %s: link %s\n' % (dyibiccexe, ' '.join(objs)))

        miniluaexe = 'minilua' + exe_ext
        f.write('build %s: mlbuild $root/dynasm/minilua.c\n' % miniluaexe)

        f.write('\n')
        f.write('rule testrun\n')
        f.write('  command = %s $root/testrun.py $root/.. %s/%s $data\n' % (
            sys.executable, root_dir, dyibiccexe))
        f.write('  description = TEST $in\n\n')

        f.write('rule genupdaterunner\n')
        f.write('  command = %s $in $out\n' % sys.executable)
        f.write('  description = GEN_UPDATE_TEST_RUNNER $in\n\n')

        f.write('rule testcexe\n')
        f.write('  command = ' + cmdlines['TESTCEXE'] + '\n')
        f.write('  description = UPDATE_RUNNER_CC $out\n')
        f.write('\n')

        f.write('rule runbin\n')
        f.write('  command = ./$in\n')
        f.write('  description = RUN_UPDATE_TEST_BINARY $in\n\n')

        alltests = []
        for testf, cmds in tests.items():
            f.write('build %s: testrun $root/../%s | %s $root/../test/common.c\n' % (
                testf, testf, dyibiccexe))
            # b64 <- json <- dict to smuggle through to test script w/o dealing
            # with shell quoting garbage.
            cmds_to_pass = base64.b64encode(bytes(json.dumps(cmds), encoding='utf-8'))
            f.write('  data = %s\n' % str(cmds_to_pass, encoding='utf-8'))
            alltests.append(testf)

        for testf in fuzz_tests:
            f.write('build %s: testrun $root/../%s | %s\n' % (
                testf, testf, dyibiccexe))
            cmds = {'run': testf, 'ret': 'NOCRASH', 'txt': ''}
            cmds_to_pass = base64.b64encode(bytes(json.dumps(cmds), encoding='utf-8'))
            f.write('  data = %s\n' % str(cmds_to_pass, encoding='utf-8'))
            alltests.append(testf)

        for testpy in upd_tests:
            tmpc = os.path.basename(testpy) + '.runner.c'
            tmpexe = os.path.basename(testpy) + '.runner' + exe_ext
            f.write('build %s: genupdaterunner $root/../%s | $root/../test/test_helpers_for_update.py\n' % (
                tmpc, testpy))
            f.write('build %s: testcexe %s libdyibicc%s | embed/libdyibicc.h\n' % (
                tmpexe, tmpc, obj_ext))
            f.write('build %s: runbin %s\n' % (testpy, tmpexe))
            alltests.append(testpy)

        f.write('\nbuild test: phony ' + ' '.join(alltests) + '\n')

        f.write('\ndefault dyibicc%s\n' % exe_ext)

        f.write('\nrule gen\n')
        f.write('  command = %s $root/gen.py $in\n' % sys.executable)
        f.write('  description = GEN build.ninja\n')
        f.write('  generator = 1\n')
        f.write('build build.ninja: gen | $root/gen.py\n')


def main():
    if len(sys.argv) != 1:
        print('usage: gen.py')
        return 1

    os.chdir(ROOT_DIR)  # Necessary when regenerating manifest from ninja
    tests = get_tests()
    upd_tests = get_upd_tests()
    fuzz_tests = get_fuzz_tests()
    for platform, pdata in CONFIGS.items():
        if (sys.platform == 'win32' and platform == 'w') or \
                (sys.platform == 'linux' and platform == 'l'):
            for config, cmdlines in pdata.items():
                if config == '__':
                    continue
                generate(platform, config, pdata['__'], cmdlines, tests, upd_tests, fuzz_tests)


if __name__ == '__main__':
    main()
