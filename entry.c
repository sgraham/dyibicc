#include "dyibicc.h"

int main(int argc, char** argv) {
  HashMap active_symbols = {NULL, 0, 0, /*global_alloc=*/true};
  dyostore_init_new_generation(&active_symbols);

  void* entry_point = compile_and_link(argc, argv, &active_symbols);
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
