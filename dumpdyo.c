#include "dyibicc.h"

static int default_output_fn(const char* fmt, va_list ap) {
  return vfprintf(stdout, fmt, ap);
}

int main(int argc, char* argv[]) {
  user_context = &(UserContext){0};
  user_context->output_function = default_output_fn;

  if (argc != 2) {
    fprintf(stderr, "dumpdyo <file.dyo>\n");
    return 1;
  }
  FILE* f = fopen(argv[1], "rb");
  if (!f) {
    fprintf(stderr, "couldn't open '%s'\n", argv[1]);
    return 2;
  }
  dump_dyo_file(f);
  fclose(f);
  return 0;
}
