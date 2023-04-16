#include "test.h"
#include <_typedesc.h>

int printf();

int main(void) {
  int x = 42;
  //_TypeDesc* td0 = _TypeDescFor(int);
  _TypeDesc* td1 = _TypeDescFor(x);
  _TypeDesc* td2 = _TypeDescFor(42);
  //ASSERT(td0, td1);
  //ASSERT(td0, td2);
}
