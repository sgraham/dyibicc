// DISABLED
// asm blocks are passed through to the system assembler in the original design,
// and there isn't one of those now that we're jitting, so this functionality is
// disabled for the time being.
#include "test.h"

char *asm_fn1(void) {
  asm("mov rax, 50\n\t"
      "mov rsp, rbp\n\t"
      "pop rbp\n\t"
      "ret");
}

char *asm_fn2(void) {
  asm inline volatile("mov rax, 55\n\t"
                      "mov rsp, rbp\n\t"
                      "pop rbp\n\t"
                      "ret");
}

int main() {
  ASSERT(50, asm_fn1());
  ASSERT(55, asm_fn2());

  printf("OK\n");
  return 0;
}
