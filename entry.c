#include "dyibicc.h"

int main(int argc, char** argv) {
  LinkInfo link_info = {0};
  if (compile_and_link(argc, argv, &link_info)) {
    if (link_info.entry_point) {
      int (*p)() = (int (*)())link_info.entry_point;
      int result = p();
      printf("main returned: %d\n", result);
      return result;
    } else {
      fprintf(stderr, "no entry point found\n");
      return 1;
    }
  } else {
    fprintf(stderr, "link failed\n");
  }
}
