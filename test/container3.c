#include "test.h"

$vec(int) container_test29($map(int, int)* py);

int main(void) {
  $map(int, int) myy = {0};

  $vec(int) myx = container_test29(&myy);

  ASSERT(7, myx..size());
  ASSERT(9, *myx..back());
  ASSERT(3, *myx..front());

  ASSERT(4, myy..size());

  ASSERT(2, *myy..at(1));
  ASSERT(4, *myy..at(3));
  ASSERT(6, *myy..at(5));
  ASSERT(8, *myy..at(7));

  myx..drop();
  myy..drop();
}
