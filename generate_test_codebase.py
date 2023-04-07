import os.path
import random
import sys

final_list = []


def SetDir(dir):
    if (not os.path.exists(dir)):
        os.mkdir(dir)
    os.chdir(dir)


def lib_name(i):
    return "lib_" + str(i)


def CreateHeader(name):
    filename = name + ".h"
    handle = open(filename, "w", newline='\n')

    guard = name + '_h_'
    handle.write('#ifndef ' + guard + '\n')
    handle.write('#define ' + guard + '\n\n')

    handle.write('void ' + name + '_init(void);\n')
    handle.write('void ' + name + '_shutdown(void);\n')
    handle.write('\n')

    handle.write('#endif\n')


def CreateC(name, lib_number, classes_per_lib, internal_includes,
            external_includes):
    filename = name + ".c"
    handle = open(filename, "w", newline='\n')

    header = name + ".h"
    handle.write('#include "' + header + '"\n')

    includes = random.sample(range(classes_per_lib), internal_includes)
    for i in includes:
        handle.write('#include "func_' + str(i) + '.h"\n')

    if (lib_number > 0):
        includes = random.sample(range(classes_per_lib), external_includes)
        lib_list = range(lib_number)
        for i in includes:
            libname = 'lib_' + str(random.choice(lib_list))
            handle.write('#include <' + libname + '/' + 'func_' + str(i) +
                         '.h>\n')

    handle.write('\n')
    handle.write('void ' + name + '_init(void) {}\n')
    handle.write('void ' + name + '_shutdown(void) {}\n')


def CreateLibrary(lib_number, classes, internal_includes, external_includes):
    SetDir(lib_name(lib_number))
    for i in range(classes):
        classname = "func_" + str(i)
        CreateHeader(classname)
        CreateC(classname, lib_number, classes, internal_includes,
                external_includes)
    os.chdir("..")


def CreateSetOfLibraries(libs, classes, internal_includes, external_includes):
    global final_list
    random.seed(12345)
    for i in range(libs):
        CreateLibrary(i, classes, internal_includes, external_includes)
        for j in range(classes):
            final_list.append('%s/func_%s.c' % (lib_name(i), str(j)))

    with open('build_all.rsp', 'w', newline='\n') as f:
        f.write('\n'.join(final_list))
        f.write('\n')


HELP_USAGE = """Usage: generate_test_codebase.py libs classes internal external.
    libs     - Number of libraries (libraries only depend on those with smaller numbers)
    functions- Number of functions per library
    internal - Number of includes per file referring to that same library
    external - Number of includes per file pointing to other libraries
"""


def main(argv):
    if len(argv) != 5:
        print(HELP_USAGE)
        return

    root_dir = 'test_codebase'
    libs = int(argv[1])
    classes = int(argv[2])
    internal_includes = int(argv[3])
    external_includes = int(argv[4])

    SetDir(root_dir)

    CreateSetOfLibraries(libs, classes, internal_includes, external_includes)


if __name__ == "__main__":
    main(sys.argv)
