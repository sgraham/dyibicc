import base64
import glob
import json
import os
import sys

# Assumes we're in $root/scripts.
ROOT_DIR = os.path.normpath(
    os.path.join(os.path.abspath(os.path.dirname(__file__)), '..'))

# Update build_amalg.py too.
FILELIST = [
    'alloc.c',
    'entry.c',
    'hashmap.c',
    'link.c',
    'main.c',
    'parse.c',
    'preprocess.c',
    'tokenize.c',
    'type.c',
    'unicode.c',
    'util.c',
]

CONFIGS = {
    'w': {
        'r': {
            'COMPILE': 'cl /showIncludes /nologo /FS /Ox /GL /Zi /DIMPLSTATIC= /DIMPLEXTERN=extern /D_CRT_SECURE_NO_DEPRECATE /W4 /WX /I$root /c $in /Fo$out /Fddyibicc.pdb',
            'LINK': 'link /nologo gdi32.lib user32.lib onecore.lib /LTCG /DEBUG /OPT:REF /OPT:ICF $in /out:$out',
            'ML': 'cl /nologo /wd4132 /wd4324 $in /link /out:$out',
        },
        'd': {
            'COMPILE': 'cl /showIncludes /nologo /FS /Od /Zi /DIMPLSTATIC= /DIMPLEXTERN=extern /D_CRT_SECURE_NO_DEPRECATE /W4 /WX /I$root /c $in /Fo:$out /Fddyibicc.pdb',
            'LINK': 'link /nologo gdi32.lib user32.lib onecore.lib /DEBUG $in /out:$out',
            'ML': 'cl /nologo /wd4132 /wd4324 $in /link /out:$out',
        },
        'a': {
            'COMPILE': 'cl /showIncludes /nologo /FS /Od /fsanitize=address /Zi /DIMPLSTATIC= /DIMPLEXTERN=extern /D_CRT_SECURE_NO_DEPRECATE /W4 /WX /I$root /c $in /Fo:$out /Fddyibicc.pdb',
            'LINK': 'link /nologo gdi32.lib user32.lib onecore.lib /DEBUG $in /out:$out',
            'ML': 'cl /nologo /wd4132 /wd4324 $in /link /out:$out',
        },
        '__': {
            'exe_ext': '.exe',
            'obj_ext': '.obj',
            'dynasm_def': '-D WIN',
        },
    },
    'l': {
        'd': {
            'COMPILE': 'clang -std=c11 -MMD -MT $out -MF $out.d -O0 -fcolor-diagnostics -fno-common -Wall -Werror -Wno-switch -DIMPLSTATIC= -DIMPLEXTERN=extern -pthread -I$root -c $in -o $out',
            'LINK': 'clang -o $out $in -pthread -lm -ldl',
            'ML': 'clang -o $out $in -lm',
        },
        'r': {
            'COMPILE': 'clang -std=c11 -MMD -MT $out -MF $out.d -Oz -fcolor-diagnostics -fno-common -Wall -Werror -Wno-switch -DIMPLSTATIC= -DIMPLEXTERN=extern -pthread -c -I$root $in -o $out',
            'LINK': 'clang -o $out $in -pthread -lm -ldl',
            'ML': 'clang -o $out $in -lm',
        },
        'a': {
            'COMPILE': 'clang -std=c11 -MMD -MT $out -MF $out.d -O0 -fsanitize=address -fcolor-diagnostics -fno-common -Wall -Werror -Wno-switch -DIMPLSTATIC= -DIMPLEXTERN=extern -pthread -c -I$root $in -o $out',
            'LINK': 'clang -fsanitize=address -o $out $in -pthread -lm -ldl',
            'ML': 'clang -o $out $in -lm',
        },
        '__': {
            'exe_ext': '',
            'obj_ext': '.o',
            'dynasm_def': '-D SYSV',
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


def generate(platform, config, settings, cmdlines, tests):
    root_dir = os.path.join('out', platform + config)
    if not os.path.isdir(root_dir):
        os.makedirs(root_dir)

    exe_ext = settings['exe_ext']
    obj_ext = settings['obj_ext']
    dynasm_def = settings['dynasm_def']

    with open(os.path.join(root_dir, 'build.ninja'), 'w', newline='\n') as f:
        f.write('builddir = .\n')
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
        f.write('rule dynasm\n')
        f.write('  command = ./minilua' + exe_ext +
                ' $root/dynasm/dynasm.lua %s -o $out $in\n' % dynasm_def)
        f.write('  description = DYNASM $out\n')
        f.write('\n')
        f.write('\n')

        objs = []

        dynasmed = 'codegen.%s.c' % platform
        f.write('build %s: dynasm $root/codegen.in.c | minilua%s\n' %
                (dynasmed, exe_ext))
        obj = os.path.splitext(dynasmed)[0] + obj_ext
        f.write('build %s: cc %s\n' % (obj, dynasmed))
        objs.append(obj)

        for src in FILELIST:
            obj = os.path.splitext(src)[0] + obj_ext
            objs.append(obj)
            f.write('build %s: cc $root/%s\n' % (obj, src))

        dyibiccexe = 'dyibicc' + exe_ext
        f.write('build %s: link %s\n' % (dyibiccexe, ' '.join(objs)))

        miniluaexe = 'minilua' + exe_ext
        f.write('build %s: mlbuild $root/dynasm/minilua.c\n' % miniluaexe)

        f.write('\n')
        f.write('rule testrun\n')
        f.write('  command = %s $root/testrun.py $root/.. %s/%s $data\n' % (
            sys.executable, root_dir, dyibiccexe))
        f.write('  description = TEST $in\n')

        alltests = []
        for testf, cmds in tests.items():
            f.write('build %s: testrun $root/../%s | %s $root/../test/common.c\n' % (
                testf, testf, dyibiccexe))
            # b64 <- json <- dict to smuggle through to test script w/o dealing
            # with shell quoting garbage.
            cmds_to_pass = base64.b64encode(bytes(json.dumps(cmds), encoding='utf-8'))
            f.write('  data = %s\n' % str(cmds_to_pass, encoding='utf-8'))
            alltests.append(testf)
        f.write('build test: phony ' + ' '.join(alltests))

        f.write('\n')
        f.write('default dyibicc%s\n' % exe_ext)

        f.write('\n')
        f.write('rule gen\n')
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
    for platform, pdata in CONFIGS.items():
        if (sys.platform == 'win32' and platform == 'w') or \
                (sys.platform == 'linux' and platform == 'l'):
            for config, cmdlines in pdata.items():
                if config == '__':
                    continue
                generate(platform, config, pdata['__'], cmdlines, tests)


if __name__ == '__main__':
    main()
