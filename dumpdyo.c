#include "dyibicc.h"

int main(int argc, char* argv[]) {
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
