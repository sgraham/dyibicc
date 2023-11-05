#include "test.h"

int main() {
  $vec(int) x = {0};
  x..push_back(33);
  x..push_back(44);

  ASSERT(44, *x..back());

  $map(int, float) y = {0};
  y..insert(3, 6.7f);
  y..insert(4, 3.14f);
  y..insert(5, 1.2f);

  ASSERT(3.14f, *y..at(4));

  x..drop();
  y..drop();
}
