#include <stdio.h>

#include "libdyibicc.h"

int main(int argc, char** argv) {
  int result = 0;
  DyibiccLinkInfo link_info = {0};

  if (dyibicc_compile_and_link(argc, argv, &link_info)) {
    if (link_info.entry_point) {
      int (*p)() = (int (*)())link_info.entry_point;
      result = p();
      printf("main returned: %d\n", result);
    } else {
      fprintf(stderr, "no entry point found\n");
      result = 1;
    }
  } else {
    fprintf(stderr, "link failed\n");
    result = 2;
  }

  dyibicc_free_link_info_resources(&link_info);

  return result;
}
