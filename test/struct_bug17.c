#include "test.h"

struct StructA {
  int x1, x2, x3;
};

void func(int dummy1,
          int dummy2,
          int dummy3,
          int dummy4,
          struct StructA trigger,
          struct StructA bug_occurred) {
  // Crash was occurring due to incorrect calculation of parameter offsets when
  // passed by reference beyond register slots.
  ASSERT(0, bug_occurred.x1);
  ASSERT(1, bug_occurred.x2);
  ASSERT(2, bug_occurred.x3);
}

extern void XXXXX(int dummy1,
          int dummy2,
          int dummy3,
          int dummy4,
          struct StructA trigger,
          struct StructA bug_occurred);
int main(void) {
  struct StructA var = {0, 1, 2};
  func(1, 2, 3, 4, var, var);
  return 0;
}
