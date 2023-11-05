#include "test.h"

struct __attribute__((methodcall(ZIPPY_))) X {
  int a;
  int b;
};
typedef struct X X;

void ZIPPY_func(X* x, int c) {
  ASSERT(5, x->a);
  ASSERT(6, x->b);
  ASSERT(32, c);
}

int main(void) {
  X x = {5, 6};
  x..func(32);

  X* px = &x;
  px..func(32);

  X** ppx = &px;
  ppx..func(32);
}
