#include "dyibicc.h"

static int dump_output_fn(int level, const char* fmt, va_list ap) {
  (void)level;
  return vfprintf(stdout, fmt, ap);
}

int main(int argc, char* argv[]) {
  output_fn = dump_output_fn;

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
