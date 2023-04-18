#include <reflect.h>
#include <stdbool.h>
#include <stdint.h>

#include "test.h"

bool SomeFunc(void) {}
int SomeFunc2(char a, float b, short c, double d) {}

typedef struct Xyz {
  int a;
} Xyz;

struct NoTypedef {
};

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

  _ReflectType* t_expr_int = _ReflectTypeOf(4+5);
  ASSERT(1, t_expr_int == t_int);
  _ReflectType* t_expr_uint = _ReflectTypeOf(4u+5);
  ASSERT(1, t_expr_uint == t_uint);

  _ReflectType* t_bool0 = _ReflectTypeOf(_Bool);
  _ReflectType* t_bool1 = _ReflectTypeOf(bool);
  // "false" and "true" are just 0 and 1, so getting their type is int, not bool.
  ASSERT(0, strcmp(t_bool0->name, "bool"));
  ASSERT(_REFLECT_KIND_BOOL, t_bool0->kind);
  ASSERT(1, t_bool0->size);
  ASSERT(1, t_bool0->align);
  ASSERT(1, t_bool0 == t_bool1);

  _ReflectType* t_char = _ReflectTypeOf(char);
  ASSERT(0, strcmp(t_char->name, "char"));
  ASSERT(_REFLECT_KIND_CHAR, t_char->kind);
  ASSERT(1, t_char->size);
  ASSERT(1, t_char->align);

  _ReflectType* t_short = _ReflectTypeOf(short);
  ASSERT(0, strcmp(t_short->name, "short"));
  ASSERT(_REFLECT_KIND_SHORT, t_short->kind);
  ASSERT(2, t_short->size);
  ASSERT(2, t_short->align);

  _ReflectType* t_float = _ReflectTypeOf(float);
  ASSERT(0, strcmp(t_float->name, "float"));
  ASSERT(_REFLECT_KIND_FLOAT, t_float->kind);
  ASSERT(4, t_float->size);
  ASSERT(4, t_float->align);

  _ReflectType* t_double = _ReflectTypeOf(double);
  ASSERT(0, strcmp(t_double->name, "double"));
  ASSERT(_REFLECT_KIND_DOUBLE, t_double->kind);
  ASSERT(8, t_double->size);
  ASSERT(8, t_double->align);

  _ReflectType* t_int64 = _ReflectTypeOf(int64_t);
  ASSERT(_REFLECT_KIND_LONG, t_int64->kind);
#if __SIZEOF_LONG__ == 4
  ASSERT(0, strcmp(t_int64->name, "long long"));
#else
  ASSERT(0, strcmp(t_int64->name, "long"));
#endif
  ASSERT(8, t_int64->size);
  ASSERT(8, t_int64->align);

  _ReflectType* t_charp = _ReflectTypeOf(char*);
  ASSERT(0, strcmp(t_charp->name, "char*"));
  ASSERT(_REFLECT_KIND_PTR, t_charp->kind);
  ASSERT(8, t_charp->size);
  ASSERT(8, t_charp->align);
  ASSERT(1, t_charp->ptr.base == t_char);

  _ReflectType* t_charpp = _ReflectTypeOf(char**);
  ASSERT(0, strcmp(t_charpp->name, "char**"));
  ASSERT(_REFLECT_KIND_PTR, t_charpp->kind);
  ASSERT(8, t_charpp->size);
  ASSERT(8, t_charpp->align);
  ASSERT(1, t_charpp->ptr.base == t_charp);

  char* x = "zippy";
  ASSERT(1, _ReflectTypeOf(x) == t_charp);

  _ReflectType* t_intp = _ReflectTypeOf(int*);
  ASSERT(0, strcmp(t_intp->name, "int*"));
  ASSERT(_REFLECT_KIND_PTR, t_intp->kind);
  ASSERT(8, t_intp->size);
  ASSERT(8, t_intp->align);
  ASSERT(1, t_intp->ptr.base == t_int);

  int y[5];
  _ReflectType* t_intarr5 = _ReflectTypeOf(y);
  ASSERT(0, strcmp(t_intarr5->name, "int [5]"));
  ASSERT(_REFLECT_KIND_ARRAY, t_intarr5->kind);
  ASSERT(5*sizeof(int), t_intarr5->size);
  ASSERT(4, t_intarr5->align);
  ASSERT(1, t_intarr5->arr.base == t_int);
  ASSERT(5, t_intarr5->arr.len);

  float(**ppz)[5];
  _ReflectType* t_pparr5 = _ReflectTypeOf(ppz);
  ASSERT(0, strcmp(t_pparr5->name, "float (**) [5]"));
  ASSERT(_REFLECT_KIND_PTR, t_pparr5->kind);
  ASSERT(8, t_pparr5->size);
  ASSERT(8, t_pparr5->align);

  _ReflectType* t_func = _ReflectTypeOf(SomeFunc);
  ASSERT(0, strcmp(t_func->name, "bool (void)"));

  _ReflectType* t_func2 = _ReflectTypeOf(SomeFunc2);
  ASSERT(0, strcmp(t_func2->name, "int (char, float, short, double)"));

  int (*Funcy)(void);
  _ReflectType* t_funcptrvoid = _ReflectTypeOf(Funcy);
  ASSERT(0, strcmp(t_funcptrvoid->name, "int (*)(void)"));

  int (*(*Funcy2)(void))[3];
  _ReflectType* t_fpretparr = _ReflectTypeOf(Funcy2);
  ASSERT(0, strcmp(t_fpretparr->name, "int (* (*)(void)) [3]"));

  float(**(*lsd)(int (*)(double)))[5][6];
  _ReflectType* t_ptr_to_func_taking_fptr_returning_pp_5_6_float = _ReflectTypeOf(lsd);
  ASSERT(0, strcmp(t_ptr_to_func_taking_fptr_returning_pp_5_6_float->name,
                   "float (** (*)(int (*)(double))) [5][6]"));

  _ReflectType* t_xyz = _ReflectTypeOf(Xyz);
  printf("%s\n", t_xyz->name);

  _ReflectType* t_pxyz = _ReflectTypeOf(Xyz*);
  printf("%s\n", t_pxyz->name);

  _ReflectType* t_notypedef = _ReflectTypeOf(NoTypedef);
  printf("%s\n", t_notypedef->name);

  _ReflectType* t_pnotypedef = _ReflectTypeOf(NoTypedef*);
  printf("%s\n", t_pnotypedef->name);

  return 0;
}
