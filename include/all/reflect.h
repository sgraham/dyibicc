#pragma once

#include <stddef.h>
#include <stdint.h>

typedef struct _ReflectType _ReflectType;
typedef struct _ReflectTypeMember _ReflectTypeMember;
typedef struct _ReflectTypeEnumerant _ReflectTypeEnumerant;

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

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4200)  // Zero-sized array.
#pragma warning(disable: 4201)  // Unnamed union.
#endif

struct _ReflectTypeMember {
  _ReflectType* type;
  char* name;
  int32_t align;
  int32_t offset;
  int32_t bit_width;   // -1 if not bitfield
  int32_t bit_offset;  // -1 if not bitfield
};

struct _ReflectTypeEnumerant {
  _ReflectTypeEnumerant* next;
  char* name;
  int32_t value;
};

struct _ReflectType {
  char* name; // Either the built-in typename, or the user declared one.
  int32_t size;
  int32_t align;
  int32_t kind; // One of _REFLECT_KIND_*.
  int32_t flags; // Combination of _REFLECT_TYPEFLAG_*.

  union {
    struct {
      _ReflectType* base;
      int32_t len;
    } arr;
    struct {
      _ReflectType* base;
    } ptr;
    struct {
      size_t num_members;
      _ReflectTypeMember members[];
    } su;
    struct {
      _ReflectType* return_ty;
      size_t num_params;
      _ReflectType* params[];
    } func;
    struct {
      _ReflectTypeEnumerant* enums;
    } enumer;
    struct {
      _ReflectType* vla_size;
      // len?
    } vla;
  };
};
#ifdef _MSC_VER
#pragma warning(pop)
#endif

#if __dyibicc__
extern _ReflectType* _ReflectTypeOf(...);
#endif
