#include "test.h"
#include <reflect.h>

typedef struct X {
  int x;
} X;

int printf();

extern _ReflectType* _$Ti = {

int main(void) {
  _ReflectType* td = _ReflectTypeOf(int);
  _ReflectType* td2 = &_$Ti;
  ASSERT(1, td == td2);
  ASSERT(4, td->size);
  ASSERT(4, td->align);
  ASSERT(0, strcmp(td->name, "int"));
}
