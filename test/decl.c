// RUN: -Itest test/common.c {self}
// RET: 0
#include "test.h"

int main() {
  ASSERT(1, ({ char x; sizeof(x); }));
  ASSERT(2, ({ short int x; sizeof(x); }));
  ASSERT(2, ({ int short x; sizeof(x); }));
  ASSERT(4, ({ int x; sizeof(x); }));
  ASSERT(__SIZEOF_LONG__, ({ long int x; sizeof(x); }));
  ASSERT(__SIZEOF_LONG__, ({ int long x; sizeof(x); }));

  ASSERT(8, ({ long long x; sizeof(x); }));

  ASSERT(0, ({ _Bool x=0; x; }));
  ASSERT(1, ({ _Bool x=1; x; }));
  ASSERT(1, ({ _Bool x=2; x; }));
  ASSERT(1, (_Bool)1);
  ASSERT(1, (_Bool)2);
  ASSERT(0, (_Bool)(char)256);

  printf("OK\n");
  return 0;
}
