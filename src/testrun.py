import base64
import json
import os
import subprocess
import sys

def main():
    root = os.path.abspath(sys.argv[1])
    os.chdir(root)

    ccbin = os.path.abspath(sys.argv[2].replace('/', os.path.sep))
    js = base64.b64decode(bytes(sys.argv[3], encoding='utf-8'))
    cmds = json.loads(js)

    # The default is 1, but we would prefer something a little more distinct. Windows will
    # return the exception code (big number) so out of 0..255, but Linux is always in that
    # range, so select something arbitrary as an ASAN signal that's more notable than the
    # default of 1.
    env = os.environ.copy()
    env['ASAN_OPTIONS'] = 'exitcode=117'

    if cmds['txt']:
        res = subprocess.run([ccbin] + cmds['run'].split(' '), cwd=root, capture_output=True,
                             universal_newlines=True, env=env)
        out = res.stdout
        if out != cmds['txt']:
            print('got output:\n')
            print(out)
            print('but expected:\n')
            print(cmds['txt'])
            return 1
    else:
        res = subprocess.run([ccbin] + cmds['run'].split(' '), cwd=root, env=env)

    if cmds['ret'] == 'NOCRASH':
        if res.returncode >= 0 and res.returncode <= 255 and res.returncode != 117:
            return 0
        # Something out of range indicates crash, e.g Windows returns -1073741819 on GPF.
        # Linux is always in the range 0..255, so pick 117 arbitrarily for an ASAN signal.
        return 2
    else:
        if res.returncode != cmds['ret']:
            print('got return code %d, but expected %d' % (res.returncode, cmds['ret']))
            return 2


if __name__ == '__main__':
    sys.exit(main())
