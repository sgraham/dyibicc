#pragma once

#include <stdint.h>

typedef struct _ReflectTypeDesc _ReflectTypeDesc;
typedef struct _ReflectTypeDescMember _ReflectTypeDescMember;
typedef struct _ReflectTypeDescEnumerant _ReflectTypeDescEnumerant;

#define _REFLECT_KIND_VOID 0
#define _REFLECT_KIND_BOOL 1
#define _REFLECT_KIND_CHAR 2
#define _REFLECT_KIND_SHORT 3
#define _REFLECT_KIND_INT 4
#define _REFLECT_KIND_LONG 5
#define _REFLECT_KIND_FLOAT 6
#define _REFLECT_KIND_DOUBLE 7
// TODO: LDOUBLE
#define _REFLECT_KIND_ENUM 9
#define _REFLECT_KIND_PTR 10
#define _REFLECT_KIND_FUNC 11
#define _REFLECT_KIND_ARRAY 12
#define _REFLECT_KIND_VLA 13
#define _REFLECT_KIND_STRUCT 14
#define _REFLECT_KIND_UNION 15

#define _REFLECT_TYPEFLAG_UNSIGNED 0x0001  // Integer types only
#define _REFLECT_TYPEFLAG_ATOMIC 0x0002    // Integer types only
#define _REFLECT_TYPEFLAG_FLEXIBLE 0x0004  // Arrays at end of structs only
#define _REFLECT_TYPEFLAG_PACKED 0x0008    // Structs and unions only
#define _REFLECT_TYPEFLAG_VARIADIC 0x0010  // Functions only

struct _ReflectTypeDesc {
  char* name; // Either the built-in typename, or the user declared one.
  int32_t size;
  int32_t align;
  int32_t kind; // One of _REFLECT_KIND_*.
  int32_t flags; // Combination of _REFLECT_TYPEFLAG_*.

  union {
    struct {
      _ReflectTypeDesc* base;
      int32_t len;
    } arr;
    struct {
      _ReflectTypeDesc* base;
    } ptr;
    struct {
      _ReflectTypeDescMember* members;
    } structunion;
    struct {
      _ReflectTypeDesc* return_ty;
      _ReflectTypeDesc* params;  // Linked by func.next.
      _ReflectTypeDesc* next;
    } func;
    struct {
      _ReflectTypeDescEnumerant* enums;
    } enumer;
    struct {
      _ReflectTypeDesc* vla_size;
      // len?
    } vla;
  };
};

struct _ReflectTypeDescMember {
  _ReflectTypeDescMember* next;
  _ReflectTypeDesc* td;
  char* name;
  int32_t idx;
  int32_t align;
  int32_t offset;
};

struct _ReflectTypeDescEnumerant {
  _ReflectTypeDescEnumerant* next;
  char* name;
  int32_t value;
};

//extern void _ReflectGetAllTypeDesc(_ReflectTypeDesc** typedescs, size_t* count);
//extern _ReflectTypeDesc* _ReflectTypeDescOf(...);
