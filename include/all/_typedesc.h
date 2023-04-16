#pragma once

#include <stdint.h>

typedef struct _TypeDesc _TypeDesc;
typedef struct _TypeDescMember _TypeDescMember;
typedef struct _TypeDescEnumerant _TypeDescEnumerant;

#define _TDK_VOID 0
#define _TDK_BOOL 1
#define _TDK_CHAR 2
#define _TDK_SHORT 3
#define _TDK_INT 4
#define _TDK_LONG 5
#define _TDK_FLOAT 6
#define _TDK_DOUBLE 7
// TODO: LDOUBLE
#define _TDK_ENUM 9
#define _TDK_PTR 10
#define _TDK_FUNC 11
#define _TDK_ARRAY 12
#define _TDK_VLA 13
#define _TDK_STRUCT 14
#define _TDK_UNION 15

#define _TDF_UNSIGNED 0x0001  // Integer types only
#define _TDF_ATOMIC 0x0002    // Integer types only
#define _TDF_FLEXIBLE 0x0004  // Structs only
#define _TDF_PACKED 0x0008    // Structs and unions only
#define _TDF_VARIADIC 0x0010  // Functions only

struct _TypeDesc {
  char* name; // Either the built-in typename, or the user declared one.
  int32_t size;
  int32_t align;
  int32_t kind; // One of _TDK_*.
  int32_t flags; // Combination of _TDF_*.

  union {
    struct {
      _TypeDesc* base;
      int32_t len;
    } arr;
    struct {
      _TypeDesc* base;
    } ptr;
    struct {
      _TypeDescMember* members;
    } structunion;
    struct {
      _TypeDesc* return_ty;
      _TypeDesc* params;  // Linked by func.next.
      _TypeDesc* next;
    } func;
    struct {
      _TypeDescEnumerant* enums;
    } enumer;
    struct {
      _TypeDesc* vla_size;
      // len?
    } vla;
  };
};

struct _TypeDescMember {
  _TypeDescMember* next;
  _TypeDesc* td;
  char* name;
  int32_t idx;
  int32_t align;
  int32_t offset;
};

struct _TypeDescEnumerant {
  _TypeDescEnumerant* next;
  char* name;
  int32_t value;
};

extern _TypeDesc* _TypeDescFor(...);
