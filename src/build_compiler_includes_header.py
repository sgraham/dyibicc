import os
import sys
import textwrap

'''
static const unsigned char compiler_include_blob[N] = {
    ...
};

CompilerInclude compiler_includes_all[] = {
    { "stddef.h", <start_offset_into_blob_above> },
    ...
};
'''

def escape(s, encoding='ascii'):
    result = ''
    for c in s:
        if not (32 <= ord(c) < 127) or c in ('\\', '"'):
            result += '\\%03o' % ord(c)
        else:
            result += c
    return '"' + result + '"'

def main():
    out, inputs = sys.argv[1], sys.argv[2:]
    with open(out, 'w', newline='\n', encoding='utf-8') as f:
        files = []
        for i in inputs:
            PREFIX = '../../include/'
            if not i.startswith(PREFIX):
                print('Expecting %s as prefix' % PREFIX)
                return 1
            path = i[len(PREFIX):]
            category, name = os.path.split(path)
            with open(i, 'r', encoding='utf-8') as g:
                files.append((category, name, g.read()))

        f.write('typedef struct CompilerInclude {\n'
                '    char* path;\n'
                '    int offset;\n'
                '} CompilerInclude;\n\n')

        blob = ''
        f.write('static CompilerInclude compiler_includes[%d] = {\n' % len(files))
        for cat, name, contents  in files:
            f.write('    { "__include__/%s/%s", %d },\n' % (cat, name, len(blob)))
            blob += contents + '\0'
        f.write('};\n')
        f.write('static unsigned char compiler_include_blob[%d] = {\n' % len(blob))
        f.write('\n'.join(textwrap.wrap(', '.join(str(ord(x)) for x in blob))))
        f.write('};\n')

if __name__ == '__main__':
    sys.exit(main())

