#include "test.h"
#include <reflect.h>

typedef struct X {
  int x;
} X;

typedef struct X {
  int x;
} X;

int printf();

int main(void) {
  int x = 42;
  //_TypeDesc* td0 = _TypeDescFor(int);
  //_ReflectTypeDesc* td1 = _ReflectTypeDescOf(x);
  //_ReflectTypeDesc* td2 = _ReflectTypeDescOf(42);
  //ASSERT(td0, td1);
  //ASSERT(td0, td2);
}
