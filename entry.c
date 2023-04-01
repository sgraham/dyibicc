#include "dyibicc.h"

int main(int argc, char** argv) {
  void* entry_point = compile_and_link(argc, argv);
  if (entry_point) {
    int (*p)() = (int (*)())entry_point;
    int result = p();
    printf("main returned: %d\n", result);
    return result;
  } else {
    fprintf(stderr, "link failed or no entry point found\n");
    return 1;
  }
}
