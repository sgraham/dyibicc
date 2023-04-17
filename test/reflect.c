#include "test.h"
#include <reflect.h>

typedef struct X {
  int x;
} X;

int main(void) {
  _ReflectType* t_int = _ReflectTypeOf(int);
  ASSERT(0, strcmp(t_int->name, "int"));
  ASSERT(_REFLECT_KIND_INT, t_int->kind);
  ASSERT(4, t_int->size);
  ASSERT(4, t_int->align);

  _ReflectType* t_uint = _ReflectTypeOf(unsigned int);
  ASSERT(0, strcmp(t_uint->name, "unsigned int"));
  ASSERT(_REFLECT_KIND_INT, t_uint->kind);
  ASSERT(4, t_uint->size);
  ASSERT(4, t_uint->align);
  ASSERT(_REFLECT_TYPEFLAG_UNSIGNED, t_uint->flags);
}
