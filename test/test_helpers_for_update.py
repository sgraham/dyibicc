import sys


_MAIN_TEMPLATE = r'''
#include "libdyibicc.h"
#include <stdio.h>

int main(void) {
  char* include_paths[] = {
    %(include_paths)s
  };
  char* input_paths[] = {
    %(input_paths)s
  };

  DyibiccEnviromentData env_data = {
      .include_paths = (const char**)include_paths,
      .files = (const char**)input_paths,
      .dyibicc_include_dir = "./include",
      .get_function_address = NULL,
      .output_function = NULL,
      .use_ansi_codes = false,
  };

  DyibiccContext* ctx = dyibicc_set_environment(&env_data);

  int final_result = 0;

%(steps)s

  printf("OK\n");
fail:
  dyibicc_free(ctx);
  return final_result;
}
'''

_UPDATE_FILE_TEMPLATE = r'''
  static char contents_step%(step)d[] = %(contents)s;
  if (!dyibicc_update(ctx, "%(filename)s", contents_step%(step)d)) {
    final_result = 255;
    goto fail;
  }
'''

_CALL_ENTRY_TEMPLATE = r'''
  {
  void* entry_point = dyibicc_find_export(ctx, "main");
  if (entry_point) {
    int myargc = 1;
    char* myargv[] = {"prog", NULL};
    int result = ((int (*)(int, char**))entry_point)(myargc, myargv);
    if (result != %(desired_result)d) {
      printf("%(exp_file)s:%(exp_line)d: got %%d, but expected %%d\n", result, %(desired_result)d);
      final_result = 253;
      goto fail;
    } else {
      printf("%(exp_file)s:%(exp_line)d: OK (%%d)\n", result);
    }
  } else {
    printf("no entry point found\n");
    final_result = 254;
    goto fail;
  }
  }
'''


_steps = []
_current = {}
_is_dirty = {}
_include_paths = []


def _string_as_c_array(s):
    result = []
    for ch in s:
        result.append(hex(ord(ch)))
    return ','.join(result) + ",'\\0'"


def initial(file_to_contents):
    global _steps
    global _current
    for f, c in file_to_contents.items():
        _current[f] = c
        _is_dirty[f] = True
    update_ok()


def update_ok():
    global _steps
    global _current
    for f, dirty in _is_dirty.items():
        if dirty:
            _steps.append(_UPDATE_FILE_TEMPLATE % {
                'filename': f,
                'contents': '{' + _string_as_c_array(_current[f]) + '}',
                'step': len(_steps)})
            _is_dirty[f] = False


def expect(rv):
    import inspect
    previous_frame = inspect.currentframe().f_back
    (filename, line_number, _, _, _) = inspect.getframeinfo(previous_frame)
    global _steps
    _steps.append(_CALL_ENTRY_TEMPLATE % {
        'desired_result': rv,
        'exp_file': filename,
        'exp_line': line_number})


def sub(filename, line, find, replace_with):
    global _current
    cur = _current[filename]
    lines = cur.splitlines()
    lines[line - 1] = lines[line - 1].replace(find, replace_with)
    _current[filename] = '\n'.join(lines)
    _is_dirty[filename] = True


def done():
    global _steps
    global _current
    global _include_paths
    files = ['"%s"' % x for x in _current.keys()] + ['NULL']
    _include_paths.append('NULL')
    with open(sys.argv[1], 'w', newline='\n') as f:
        f.write(_MAIN_TEMPLATE % {
                'include_paths': ', '.join(_include_paths),
                'input_paths': ', '.join(files),
                'steps': '\n'.join(_steps)})
