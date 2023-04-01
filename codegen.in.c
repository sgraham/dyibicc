#include "dyibicc.h"

#define DASM_CHECKS 1

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4127)
#pragma warning(disable : 4244)
#endif
#include "dynasm/dasm_proto.h"
#include "dynasm/dasm_x86.h"
#ifdef _MSC_VER
#pragma warning(pop)
#endif

///| .arch x64
///| .section main
///| .actionlist dynasm_actions
///| .globals dynasm_globals
///| .if WIN
///| .define X64WIN, 1
///| .endif

#define Dst &dynasm

static FILE* dyo_file;
static int depth;

#define REG_DI 7
#define REG_SI 6
#define REG_DX 2
#define REG_CX 1
#define REG_R8 8
#define REG_R9 9

// Used with Rq(), Rd(), Rw(), Rb()
#if X64WIN
static int dasmargreg[] = {REG_CX, REG_DX, REG_R8, REG_R9};
#define REG_UTIL REG_CX
#define X64WIN_REG_MAX 4
#define PARAMETER_SAVE_SIZE (4 * 8)
#else
static int dasmargreg[] = {REG_DI, REG_SI, REG_DX, REG_CX, REG_R8, REG_R9};
#define REG_UTIL REG_DI
#define SYSV_GP_MAX 6
#define SYSV_FP_MAX 8
#endif
///| .if X64WIN
///| .define CARG1, rcx
///| .define CARG1d, ecx
///| .define CARG2, rdx
///| .define CARG3, r8
///| .define CARG4, r9
///| .define RUTIL, rcx
///| .define RUTILd, ecx
///| .define RUTILenc, 0x11
///| .else
///| .define CARG1, rdi
///| .define CARG1d, edi
///| .define CARG2, rsi
///| .define CARG3, rdx
///| .define CARG4, rcx
///| .define CARG5, r8
///| .define CARG6, r9
///| .define RUTIL, rdi
///| .define RUTILd, edi
///| .define RUTILenc, 0x17
///| .endif

static Obj* current_fn;

static dasm_State* dynasm;
static int numlabels = 1;
static int dasm_label_main_entry = -1;
static StringIntArray import_fixups;
static StringIntArray data_fixups;
static IntIntArray pending_code_pclabels;

static void gen_expr(Node* node);
static void gen_stmt(Node* node);

int codegen_pclabel(void) {
  int ret = numlabels;
  dasm_growpc(&dynasm, ++numlabels);
  return ret;
}

static void push(void) {
  ///| push rax
  depth++;
}

static void pop(int dasmreg) {
  ///| pop Rq(dasmreg)
  depth--;
}

static void pushf(void) {
  ///| sub rsp, 8
  ///| movsd qword [rsp], xmm0
  depth++;
}

static void popf(int reg) {
  ///| movsd xmm(reg), qword [rsp]
  ///| add rsp, 8
  depth--;
}

// Load a value from where %rax is pointing to.
static void load(Type* ty) {
  switch (ty->kind) {
    case TY_STRUCT:
    case TY_UNION:
    case TY_ARRAY:
    case TY_FUNC:
    case TY_VLA:
      // If it is an array, do not attempt to load a value to the
      // register because in general we can't load an entire array to a
      // register. As a result, the result of an evaluation of an array
      // becomes not the array itself but the address of the array.
      // This is where "array is automatically converted to a pointer to
      // the first element of the array in C" occurs.
      return;
    case TY_FLOAT:
      ///| movss xmm0, dword [rax]
      return;
    case TY_DOUBLE:
      ///| movsd xmm0, qword [rax]
      return;
#if !X64WIN
    case TY_LDOUBLE:
      ///| fld tword [rax]
      return;
#endif
  }

  // When we load a char or a short value to a register, we always
  // extend them to the size of int, so we can assume the lower half of
  // a register always contains a valid value. The upper half of a
  // register for char, short and int may contain garbage. When we load
  // a long value to a register, it simply occupies the entire register.
  if (ty->size == 1) {
    if (ty->is_unsigned) {
      ///| movzx eax, byte [rax]
    } else {
      ///| movsx eax, byte [rax]
    }
  } else if (ty->size == 2) {
    if (ty->is_unsigned) {
      ///| movzx eax, word [rax]
    } else {
      ///| movsx eax, word [rax]
    }
  } else if (ty->size == 4) {
    ///| movsxd rax, dword [rax]
  } else {
    ///| mov rax, qword [rax]
  }
}

// Store %rax to an address that the stack top is pointing to.
static void store(Type* ty) {
  pop(REG_UTIL);

  switch (ty->kind) {
    case TY_STRUCT:
    case TY_UNION:
      for (int i = 0; i < ty->size; i++) {
        ///| mov r8b, [rax+i]
        ///| mov [RUTIL+i], r8b
      }
      return;
    case TY_FLOAT:
      ///| movss dword [RUTIL], xmm0
      return;
    case TY_DOUBLE:
      ///| movsd qword [RUTIL], xmm0
      return;
#if !X64WIN
    case TY_LDOUBLE:
      ///| fstp tword [RUTIL]
      return;
#endif
  }

  if (ty->size == 1) {
    ///| mov [RUTIL], al
  } else if (ty->size == 2) {
    ///| mov [RUTIL], ax
  } else if (ty->size == 4) {
    ///| mov [RUTIL], eax
  } else {
    ///| mov [RUTIL], rax
  }
}

// Compute the absolute address of a given node.
// It's an error if a given node does not reside in memory.
static void gen_addr(Node* node) {
  switch (node->kind) {
    case ND_VAR:
      // Variable-length array, which is always local.
      if (node->var->ty->kind == TY_VLA) {
        ///| mov rax, [rbp+node->var->offset]
        return;
      }

      // Local variable
      if (node->var->is_local) {
        ///| lea rax, [rbp+node->var->offset]
        return;
      }

      // Thread-local variable
      if (node->var->is_tls) {
        // println("  mov rax, fs:0");
        // println("  add rax, [rel %s wrt ..gottpoff]", node->var->name);
        error_tok(node->tok, "TLS not implemented");
        return;
      }

      // Function
      if (node->ty->kind == TY_FUNC) {
        if (node->var->is_definition) {
          ///| lea rax, [=>node->var->dasm_entry_label]
        } else {
          int fixup_location = codegen_pclabel();
          strintarray_push(&import_fixups, (StringInt){node->var->name, fixup_location});
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4310)  // dynasm casts the top and bottom of the 64bit arg
#endif
          ///|=>fixup_location:
          ///| mov64 rax, 0xc0dec0dec0dec0de
#ifdef _MSC_VER
#pragma warning(pop)
#endif
        }
        return;
      }

      // Global variable
      int fixup_location = codegen_pclabel();
      strintarray_push(&data_fixups, (StringInt){node->var->name, fixup_location});
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4310)  // dynasm casts the top and bottom of the 64bit arg
#endif
      ///|=>fixup_location:
      ///| mov64 rax, 0xda7ada7ada7ada7a
#ifdef _MSC_VER
#pragma warning(pop)
#endif
      return;
    case ND_DEREF:
      gen_expr(node->lhs);
      return;
    case ND_COMMA:
      gen_expr(node->lhs);
      gen_addr(node->rhs);
      return;
    case ND_MEMBER:
      gen_addr(node->lhs);
#if X64WIN
      if (node->lhs->kind == ND_VAR && node->lhs->var->is_param_passed_by_reference) {
        ///| mov rax, [rax]
      }
#endif
      ///| add rax, node->member->offset
      return;
    case ND_FUNCALL:
      if (node->ret_buffer) {
        gen_expr(node);
        return;
      }
      break;
    case ND_ASSIGN:
    case ND_COND:
      if (node->ty->kind == TY_STRUCT || node->ty->kind == TY_UNION) {
        gen_expr(node);
        return;
      }
      break;
    case ND_VLA_PTR:
      ///| lea rax, [rbp+node->var->offset]
      return;
  }

  error_tok(node->tok, "not an lvalue");
}

static void cmp_zero(Type* ty) {
  switch (ty->kind) {
    case TY_FLOAT:
      ///| xorps xmm1, xmm1
      ///| ucomiss xmm0, xmm1
      return;
    case TY_DOUBLE:
      ///| xorpd xmm1, xmm1
      ///| ucomisd xmm0, xmm1
      return;
#if !X64WIN
    case TY_LDOUBLE:
      ///| fldz
      ///| fucomip st0
      ///| fstp st0
      return;
#endif
  }

  if (is_integer(ty) && ty->size <= 4) {
    ///| cmp eax, 0
  } else {
    ///| cmp rax, 0
  }
}

enum { I8, I16, I32, I64, U8, U16, U32, U64, F32, F64, F80 };

static int get_type_id(Type* ty) {
  switch (ty->kind) {
    case TY_CHAR:
      return ty->is_unsigned ? U8 : I8;
    case TY_SHORT:
      return ty->is_unsigned ? U16 : I16;
    case TY_INT:
      return ty->is_unsigned ? U32 : I32;
    case TY_LONG:
      return ty->is_unsigned ? U64 : I64;
    case TY_FLOAT:
      return F32;
    case TY_DOUBLE:
      return F64;
#if !X64WIN
    case TY_LDOUBLE:
      return F80;
#endif
  }
  return U64;
}

static void i32i8(void) {
  ///| movsx eax, al
}
static void i32u8(void) {
  ///| movzx eax, al
}
static void i32i16(void) {
  ///| movsx eax, ax
}
static void i32u16(void) {
  ///| movzx eax, ax
}
static void i32f32(void) {
  ///| cvtsi2ss xmm0, eax
}
static void i32i64(void) {
  ///| movsxd rax, eax
}
static void i32f64(void) {
  ///| cvtsi2sd xmm0, eax
}
static void i32f80(void) {
  ///| mov [rsp-4], eax
  ///| fild dword [rsp-4]
}

static void u32f32(void) {
  ///| mov eax, eax
  ///| cvtsi2ss xmm0, rax
}
static void u32i64(void) {
  ///| mov eax, eax
}
static void u32f64(void) {
  ///| mov eax, eax
  ///| cvtsi2sd xmm0, rax
}
static void u32f80(void) {
  ///| mov eax, eax
  ///| mov [rsp-8], rax
  ///| fild qword [rsp-8]
}

static void i64f32(void) {
  ///| cvtsi2ss xmm0, rax
}
static void i64f64(void) {
  ///| cvtsi2sd xmm0, rax
}
static void i64f80(void) {
  ///| mov [rsp-8], rax
  ///| fild qword [rsp-8]
}

static void u64f32(void) {
  ///| cvtsi2ss xmm0, rax
}
static void u64f64(void) {
  ///| test rax,rax
  ///| js >1
  ///| pxor xmm0,xmm0
  ///| cvtsi2sd xmm0,rax
  ///| jmp >2
  ///|1:
  ///| mov RUTIL,rax
  ///| and eax,1
  ///| pxor xmm0,xmm0
  ///| shr RUTIL, 1
  ///| or RUTIL,rax
  ///| cvtsi2sd xmm0,RUTIL
  ///| addsd xmm0,xmm0
  ///|2:
}
static void u64f80(void) {
  ///| mov [rsp-8], rax
  ///| fild qword [rsp-8]
  ///| test rax, rax
  ///| jns >1
  ///| mov eax, 1602224128
  ///| mov [rsp-4], eax
  ///| fadd dword [rsp-4]
  ///|1:
}

static void f32i8(void) {
  ///| cvttss2si eax, xmm0
  ///| movsx eax, al
}
static void f32u8(void) {
  ///| cvttss2si eax, xmm0
  ///| movzx eax, al
}
static void f32i16(void) {
  ///| cvttss2si eax, xmm0
  ///| movsx eax, ax
}
static void f32u16(void) {
  ///| cvttss2si eax, xmm0
  ///| movzx eax, ax
}
static void f32i32(void) {
  ///| cvttss2si eax, xmm0
}
static void f32u32(void) {
  ///| cvttss2si rax, xmm0
}
static void f32i64(void) {
  ///| cvttss2si rax, xmm0
}
static void f32u64(void) {
  ///| cvttss2si rax, xmm0
}
static void f32f64(void) {
  ///| cvtss2sd xmm0, xmm0
}
static void f32f80(void) {
  ///| movss dword [rsp-4], xmm0
  ///| fld dword [rsp-4]
}

static void f64i8(void) {
  ///| cvttsd2si eax, xmm0
  ///| movsx eax, al
}
static void f64u8(void) {
  ///| cvttsd2si eax, xmm0
  ///| movzx eax, al
}
static void f64i16(void) {
  ///| cvttsd2si eax, xmm0
  ///| movsx eax, ax
}
static void f64u16(void) {
  ///| cvttsd2si eax, xmm0
  ///| movzx eax, ax
}
static void f64i32(void) {
  ///| cvttsd2si eax, xmm0
}
static void f64u32(void) {
  ///| cvttsd2si rax, xmm0
}
static void f64i64(void) {
  ///| cvttsd2si rax, xmm0
}
static void f64u64(void) {
  ///| cvttsd2si rax, xmm0
}
static void f64f32(void) {
  ///| cvtsd2ss xmm0, xmm0
}
static void f64f80(void) {
  ///| movsd qword [rsp-8], xmm0
  ///| fld qword [rsp-8]
}

static void from_f80_1(void) {
  ///| fnstcw word [rsp-10]
  ///| movzx eax, word [rsp-10]
  ///| or ah, 12
  ///| mov [rsp-12], ax
  ///| fldcw word [rsp-12]
}

#define FROM_F80_2 " [rsp-24]\n fldcw [rsp-10]\n "

static void f80i8(void) {
  from_f80_1();
  ///| fistp dword [rsp-24]
  ///| fldcw word [rsp-10]
  ///| movsx eax, word [rsp-24]
}
static void f80u8(void) {
  from_f80_1();
  ///| fistp dword [rsp-24]
  ///| fldcw word [rsp-10]
  ///| movzx eax, word [rsp-24]
  ///| and eax, 0xff
}
static void f80i16(void) {
  from_f80_1();
  ///| fistp dword [rsp-24]
  ///| fldcw word [rsp-10]
  ///| movsx eax, word [rsp-24]
}
static void f80u16(void) {
  from_f80_1();
  ///| fistp dword [rsp-24]
  ///| fldcw word [rsp-10]
  ///| movzx eax, word [rsp-24]
}
static void f80i32(void) {
  from_f80_1();
  ///| fistp dword [rsp-24]
  ///| fldcw word [rsp-10]
  ///| mov eax, [rsp-24]
}
static void f80u32(void) {
  from_f80_1();
  ///| fistp dword [rsp-24]
  ///| fldcw word [rsp-10]
  ///| mov eax, [rsp-24]
}
static void f80i64(void) {
  from_f80_1();
  ///| fistp qword [rsp-24]
  ///| fldcw word [rsp-10]
  ///| mov rax, [rsp-24]
}
static void f80u64(void) {
  from_f80_1();
  ///| fistp qword [rsp-24]
  ///| fldcw word [rsp-10]
  ///| mov rax, [rsp-24]
}
static void f80f32(void) {
  ///| fstp dword [rsp-8]
  ///| movss xmm0, dword [rsp-8]
}
static void f80f64(void) {
  ///| fstp qword [rsp-8]
  ///| movsd xmm0, qword [rsp-8]
}

typedef void (*DynasmCastFunc)(void);

// clang-format off

// The table for type casts
static DynasmCastFunc dynasm_cast_table[][11] = {
  // "to" is the rows, "from" the columns
  // i8   i16     i32     i64     u8     u16     u32     u64     f32     f64     f80
  {NULL,  NULL,   NULL,   i32i64, i32u8, i32u16, NULL,   i32i64, i32f32, i32f64, i32f80}, // i8
  {i32i8, NULL,   NULL,   i32i64, i32u8, i32u16, NULL,   i32i64, i32f32, i32f64, i32f80}, // i16
  {i32i8, i32i16, NULL,   i32i64, i32u8, i32u16, NULL,   i32i64, i32f32, i32f64, i32f80}, // i32
  {i32i8, i32i16, NULL,   NULL,   i32u8, i32u16, NULL,   NULL,   i64f32, i64f64, i64f80}, // i64

  {i32i8, NULL,   NULL,   i32i64, NULL,  NULL,   NULL,   i32i64, i32f32, i32f64, i32f80}, // u8
  {i32i8, i32i16, NULL,   i32i64, i32u8, NULL,   NULL,   i32i64, i32f32, i32f64, i32f80}, // u16
  {i32i8, i32i16, NULL,   u32i64, i32u8, i32u16, NULL,   u32i64, u32f32, u32f64, u32f80}, // u32
  {i32i8, i32i16, NULL,   NULL,   i32u8, i32u16, NULL,   NULL,   u64f32, u64f64, u64f80}, // u64

  {f32i8, f32i16, f32i32, f32i64, f32u8, f32u16, f32u32, f32u64, NULL,   f32f64, f32f80}, // f32
  {f64i8, f64i16, f64i32, f64i64, f64u8, f64u16, f64u32, f64u64, f64f32, NULL,   f64f80}, // f64
  {f80i8, f80i16, f80i32, f80i64, f80u8, f80u16, f80u32, f80u64, f80f32, f80f64, NULL},   // f80
};

// clang-format on

static void cast(Type* from, Type* to) {
  if (to->kind == TY_VOID)
    return;

  if (to->kind == TY_BOOL) {
    cmp_zero(from);
    ///| setne al
    ///| movzx eax, al
    return;
  }

  int t1 = get_type_id(from);
  int t2 = get_type_id(to);
  if (dynasm_cast_table[t1][t2]) {
    dynasm_cast_table[t1][t2]();
  }
}

#if !X64WIN

// Structs or unions equal or smaller than 16 bytes are passed
// using up to two registers.
//
// If the first 8 bytes contains only floating-point type members,
// they are passed in an XMM register. Otherwise, they are passed
// in a general-purpose register.
//
// If a struct/union is larger than 8 bytes, the same rule is
// applied to the the next 8 byte chunk.
//
// This function returns true if `ty` has only floating-point
// members in its byte range [lo, hi).
static bool has_flonum(Type* ty, int lo, int hi, int offset) {
  if (ty->kind == TY_STRUCT || ty->kind == TY_UNION) {
    for (Member* mem = ty->members; mem; mem = mem->next)
      if (!has_flonum(mem->ty, lo, hi, offset + mem->offset))
        return false;
    return true;
  }

  if (ty->kind == TY_ARRAY) {
    for (int i = 0; i < ty->array_len; i++)
      if (!has_flonum(ty->base, lo, hi, offset + ty->base->size * i))
        return false;
    return true;
  }

  return offset < lo || hi <= offset || ty->kind == TY_FLOAT || ty->kind == TY_DOUBLE;
}

static bool has_flonum1(Type* ty) {
  return has_flonum(ty, 0, 8, 0);
}

static bool has_flonum2(Type* ty) {
  return has_flonum(ty, 8, 16, 0);
}

#endif

static int push_struct(Type* ty) {
  int sz = (int)align_to_s(ty->size, 8);
  ///| sub rsp, sz
  depth += sz / 8;

  for (int i = 0; i < ty->size; i++) {
    ///| mov r10b, [rax+i]
    ///| mov [rsp+i], r10b
  }

  return sz;
}

#if X64WIN

bool type_passed_in_register(Type* ty) {
  // https://learn.microsoft.com/en-us/cpp/build/x64-calling-convention:
  //   "__m128 types, arrays, and strings are never passed by immediate value.
  //   Instead, a pointer is passed to memory allocated by the caller. Structs
  //   and unions of size 8, 16, 32, or 64 bits, and __m64 types, are passed as
  //   if they were integers of the same size."
  //
  // Note that e.g. a pragma pack 5 byte structure will be passed by reference,
  // so this is not just size <= 8 as it is for 16 on SysV.
  //
  // Arrays and strings as mentioned won't be TY_STRUCT/TY_UNION so they should
  // not use this function.
  return ty->size == 1 || ty->size == 2 || ty->size == 4 || ty->size == 8;
}

static void push_args2_win(Node* args, bool first_pass) {
  if (!args)
    return;
  push_args2_win(args->next, first_pass);

  // Push all the by-stack first, then on the second pass, push all the things
  // that will be popped back into registers by the actual call.
  if ((first_pass && !args->pass_by_stack) || (!first_pass && args->pass_by_stack))
    return;

  if ((args->ty->kind != TY_STRUCT && args->ty->kind != TY_UNION) ||
      type_passed_in_register(args->ty)) {
    gen_expr(args);
  }

  switch (args->ty->kind) {
    case TY_STRUCT:
    case TY_UNION:
      if (!type_passed_in_register(args->ty)) {
        assert(args->pass_by_reference);
        ///| lea rax, [r11-args->pass_by_reference]
      } else {
        ///| mov rax, [rax]
      }
      push();
      break;
    case TY_FLOAT:
    case TY_DOUBLE:
      pushf();
      break;
    default:
      push();
      break;
  }
}

// --- Windows ---
// Load function call arguments. Arguments are already evaluated and
// stored to the stack as local variables. What we need to do in this
// function is to load them to registers or push them to the stack as
// required by the Windows ABI.
//
// - Integer arguments in the leftmost four positions are passed in RCX, RDX,
//   R8, and R9.
//
// - Floating point arguments in the leftmost four position are passed in
//   XMM0-XM3.
//
// - The 5th and subsequent arguments are push on the stack in right-to-left
//   order.
//
// - Arguments larger than 8 bytes are always passed by reference.
//
// - When mixing integer and floating point arguments, the opposite type's
//   register is left unused, e.g.
//
//     void func(int a, double b, int c, float d, int e, float f);
//
//   would have a in RCX, b in XMM1, c in R8, d in XMM3, f then e pushed on stack.
//
// - Varargs follow the same conventions, but floating point values must also
//   have their value stored in the corresponding integer register for the first
//   four arguments.
//
// - For larger than 8 byte structs, they're passed by reference. So we first
//   need to make a local copy on the stack (since they're still passed by value
//   not reference as far as the language is concerned), but then pass a pointer
//   to the copy rather than to the actual data. This difference definitely
//   casues the most changes vs. SysV in the rest of the compiler.
//
// - Integer return values of 8 bytes or less are in RAX (including user-defined
//   types like small structures). Floating point are returned in XMM0. For
//   user-defined types that are larger than 8 bytes, the caller allocates a
//   buffer and passes the address of the buffer in RCX, taking up the first
//   integer register slot. The function returns the same address passed in RCX
//   in RAX.
//
// - RAX, RCX, RDX, R8, R9, R10, R11, and XMM0-XMM5 are volatile.
// - RBX, RBP, RDI, RSI, RSP, R12, R13, R14, R15, and XMM6-XMM15 are
//   non-volatile.
//
// --- Windows ---
static int push_args_win(Node* node, int* by_ref_copies_size) {
  int stack = 0, reg = 0;

  bool has_by_ref_args = false;
  for (Node* arg = node->args; arg; arg = arg->next) {
    if ((arg->ty->kind == TY_STRUCT || arg->ty->kind == TY_UNION) &&
        !type_passed_in_register(arg->ty)) {
      has_by_ref_args = true;
      break;
    }
  }

  if (has_by_ref_args) {
    // Use r11 as a base pointer for by-reference copies of structs.
    ///| push r11
    ///| mov r11, rsp
  }

  // If the return type is a large struct/union, the caller passes
  // a pointer to a buffer as if it were the first argument.
  if (node->ret_buffer && !type_passed_in_register(node->ty))
    reg++;

  *by_ref_copies_size = 0;

  // Load as many arguments to the registers as possible.
  for (Node* arg = node->args; arg; arg = arg->next) {
    Type* ty = arg->ty;

    switch (ty->kind) {
      case TY_STRUCT:
      case TY_UNION:
        if (type_passed_in_register(ty)) {
          if (reg++ >= X64WIN_REG_MAX) {
            arg->pass_by_stack = true;
            ++stack;
          }
        } else {
          // Make a copy, and note the offset for passing by reference.
          gen_expr(arg);
          *by_ref_copies_size += push_struct(ty);
          arg->pass_by_reference = *by_ref_copies_size;
        }
        break;
      case TY_FLOAT:
      case TY_DOUBLE:
        if (reg++ >= X64WIN_REG_MAX) {
          arg->pass_by_stack = true;
          stack++;
        }
        break;
      default:
        if (reg++ >= X64WIN_REG_MAX) {
          arg->pass_by_stack = true;
          stack++;
        }
    }
  }

  assert((*by_ref_copies_size == 0 && !has_by_ref_args) ||
         (*by_ref_copies_size && has_by_ref_args));

  if ((depth + stack) % 2 == 1) {
    ///| sub rsp, 8
    depth++;
    stack++;
  }

  push_args2_win(node->args, true);
  push_args2_win(node->args, false);

  // If the return type is a large struct/union, the caller passes
  // a pointer to a buffer as if it were the first argument.
  if (node->ret_buffer && !type_passed_in_register(node->ty)) {
    ///| lea rax, [rbp+node->ret_buffer->offset]
    push();
  }

  return stack;
}

#else

static void push_args2_sysv(Node* args, bool first_pass) {
  if (!args)
    return;
  push_args2_sysv(args->next, first_pass);

  // Push all the by-stack first, then on the second pass, push all the things
  // that will be popped back into registers by the actual call.
  if ((first_pass && !args->pass_by_stack) || (!first_pass && args->pass_by_stack))
    return;

  gen_expr(args);

  switch (args->ty->kind) {
    case TY_STRUCT:
    case TY_UNION:
      push_struct(args->ty);
      break;
    case TY_FLOAT:
    case TY_DOUBLE:
      pushf();
      break;
    case TY_LDOUBLE:
      ///| sub rsp, 16
      ///| fstp tword [rsp]
      depth += 2;
      break;
    default:
      push();
      break;
  }
}

// --- SysV ---
//
// Load function call arguments. Arguments are already evaluated and
// stored to the stack as local variables. What we need to do in this
// function is to load them to registers or push them to the stack as
// specified by the x86-64 psABI. Here is what the spec says:
//
// - Up to 6 arguments of integral type are passed using RDI, RSI,
//   RDX, RCX, R8 and R9.
//
// - Up to 8 arguments of floating-point type are passed using XMM0 to
//   XMM7.
//
// - If all registers of an appropriate type are already used, push an
//   argument to the stack in the right-to-left order.
//
// - Each argument passed on the stack takes 8 bytes, and the end of
//   the argument area must be aligned to a 16 byte boundary.
//
// - If a function is variadic, set the number of floating-point type
//   arguments to RAX.
//
// --- SysV ---
static int push_args_sysv(Node* node) {
  int stack = 0, gp = 0, fp = 0;

  // If the return type is a large struct/union, the caller passes
  // a pointer to a buffer as if it were the first argument.
  if (node->ret_buffer && node->ty->size > 16)
    gp++;

  // Load as many arguments to the registers as possible.
  for (Node* arg = node->args; arg; arg = arg->next) {
    Type* ty = arg->ty;

    switch (ty->kind) {
      case TY_STRUCT:
      case TY_UNION:
        if (ty->size > 16) {
          arg->pass_by_stack = true;
          stack += align_to_s(ty->size, 8) / 8;
        } else {
          bool fp1 = has_flonum1(ty);
          bool fp2 = has_flonum2(ty);

          if (fp + fp1 + fp2 < SYSV_FP_MAX && gp + !fp1 + !fp2 < SYSV_GP_MAX) {
            fp = fp + fp1 + fp2;
            gp = gp + !fp1 + !fp2;
          } else {
            arg->pass_by_stack = true;
            stack += align_to_s(ty->size, 8) / 8;
          }
        }
        break;
      case TY_FLOAT:
      case TY_DOUBLE:
        if (fp++ >= SYSV_FP_MAX) {
          arg->pass_by_stack = true;
          stack++;
        }
        break;
      case TY_LDOUBLE:
        arg->pass_by_stack = true;
        stack += 2;
        break;
      default:
        if (gp++ >= SYSV_GP_MAX) {
          arg->pass_by_stack = true;
          stack++;
        }
    }
  }

  if ((depth + stack) % 2 == 1) {
    ///| sub rsp, 8
    depth++;
    stack++;
  }

  push_args2_sysv(node->args, true);
  push_args2_sysv(node->args, false);

  // If the return type is a large struct/union, the caller passes
  // a pointer to a buffer as if it were the first argument.
  if (node->ret_buffer && node->ty->size > 16) {
    ///| lea rax, [rbp+node->ret_buffer->offset]
    push();
  }

  return stack;
}

static void copy_ret_buffer(Obj* var) {
  Type* ty = var->ty;
  int gp = 0, fp = 0;

  if (has_flonum1(ty)) {
    assert(ty->size == 4 || 8 <= ty->size);
    if (ty->size == 4) {
      ///| movss dword [rbp+var->offset], xmm0
    } else {
      ///| movsd qword [rbp+var->offset], xmm0
    }
    fp++;
  } else {
    for (int i = 0; i < MIN(8, ty->size); i++) {
      ///| mov [rbp+var->offset+i], al
      ///| shr rax, 8
    }
    gp++;
  }

  if (ty->size > 8) {
    if (has_flonum2(ty)) {
      assert(ty->size == 12 || ty->size == 16);
      if (ty->size == 12) {
        ///| movss dword [rbp+var->offset+8], xmm(fp)
      } else {
        ///| movsd qword [rbp+var->offset+8], xmm(fp)
      }
    } else {
      for (int i = 8; i < MIN(16, ty->size); i++) {
        ///| mov [rbp+var->offset+i], Rb(gp)
        ///| shr Rq(gp), 8
      }
    }
  }
}

#endif

static void copy_struct_reg(void) {
#if X64WIN
  // TODO: I'm not sure if this is right/sufficient.
  ///| mov rax, [rax]
#else
  Type* ty = current_fn->ty->return_ty;

  int gp = 0, fp = 0;

  ///| mov RUTIL, rax

  if (has_flonum(ty, 0, 8, 0)) {
    assert(ty->size == 4 || 8 <= ty->size);
    if (ty->size == 4) {
      ///| movss xmm0, dword [RUTIL]
    } else {
      ///| movsd xmm0, qword [RUTIL]
    }
    fp++;
  } else {
    ///| mov rax, 0
    for (int i = MIN(8, ty->size) - 1; i >= 0; i--) {
      ///| shl rax, 8
      ///| mov ax, [RUTIL+i]
    }
    gp++;
  }

  if (ty->size > 8) {
    if (has_flonum(ty, 8, 16, 0)) {
      assert(ty->size == 12 || ty->size == 16);
      if (ty->size == 4) {
        ///| movss xmm(fp), dword [RUTIL+8]
      } else {
        ///| movsd xmm(fp), qword [RUTIL+8]
      }
    } else {
      ///| mov Rq(gp), 0
      for (int i = MIN(16, ty->size) - 1; i >= 8; i--) {
        ///| shl Rq(gp), 8
        ///| mov Rb(gp), [RUTIL+i]
      }
    }
  }
#endif
}

static void copy_struct_mem(void) {
  Type* ty = current_fn->ty->return_ty;
  Obj* var = current_fn->params;

  ///| mov RUTIL, [rbp+var->offset]

  for (int i = 0; i < ty->size; i++) {
    ///| mov dl, [rax+i]
    ///| mov [RUTIL+i], dl
  }
}

static void builtin_alloca(void) {
  // Align size to 16 bytes.
  ///| add CARG1, 15
  ///| and CARG1d, 0xfffffff0

  // Shift the temporary area by CARG1.
  ///| mov CARG4, [rbp+current_fn->alloca_bottom->offset]
  ///| sub CARG4, rsp
  ///| mov rax, rsp
  ///| sub rsp, CARG1
  ///| mov rdx, rsp
  ///|1:
  ///| cmp CARG4, 0
  ///| je >2
  ///| mov r8b, [rax]
  ///| mov [rdx], r8b
  ///| inc rdx
  ///| inc rax
  ///| dec CARG4
  ///| jmp <1
  ///|2:

  // Move alloca_bottom pointer.
  ///| mov rax, [rbp+current_fn->alloca_bottom->offset]
  ///| sub rax, CARG1
  ///| mov [rbp+current_fn->alloca_bottom->offset], rax
}

// Generate code for a given node.
static void gen_expr(Node* node) {
  switch (node->kind) {
    case ND_NULL_EXPR:
      return;
    case ND_NUM: {
      switch (node->ty->kind) {
        case TY_FLOAT: {
          union {
            float f32;
            uint32_t u32;
          } u = {(float)node->fval};
          ///| mov eax, u.u32
          ///| movd xmm0, rax
          return;
        }
        case TY_DOUBLE: {
          union {
            double f64;
            uint64_t u64;
          } u = {(double)node->fval};
          ///| mov64 rax, u.u64
          ///| movd xmm0, rax
          return;
        }
#if !X64WIN
        case TY_LDOUBLE: {
          union {
            long double f80;
            uint64_t u64[2];
          } u;
          memset(&u, 0, sizeof(u));
          u.f80 = node->fval;
          ///| mov64 rax, u.u64[0]
          ///| mov [rsp-16], rax
          ///| mov64 rax, u.u64[1]
          ///| mov [rsp-8], rax
          ///| fld tword [rsp-16]
          return;
        }
#endif
      }

      if (node->val < INT_MIN || node->val > INT_MAX) {
        ///| mov64 rax, node->val
      } else {
        ///| mov rax, node->val
      }
      return;
    }
    case ND_NEG:
      gen_expr(node->lhs);

      switch (node->ty->kind) {
        case TY_FLOAT:
          ///| mov rax, 1
          ///| shl rax, 31
          ///| movd xmm1, rax
          ///| xorps xmm0, xmm1
          return;
        case TY_DOUBLE:
          ///| mov rax, 1
          ///| shl rax, 63
          ///| movd xmm1, rax
          ///| xorpd xmm0, xmm1
          return;
#if !X64WIN
        case TY_LDOUBLE:
          ///| fchs
          return;
#endif
      }

      ///| neg rax
      return;
    case ND_VAR:
      gen_addr(node);
      load(node->ty);
      return;
    case ND_MEMBER: {
      gen_addr(node);
      load(node->ty);

      Member* mem = node->member;
      if (mem->is_bitfield) {
        ///| shl rax, 64 - mem->bit_width - mem->bit_offset
        if (mem->ty->is_unsigned) {
          ///| shr rax, 64 - mem->bit_width
        } else {
          ///| sar rax, 64 - mem->bit_width
        }
      }
      return;
    }
    case ND_DEREF:
      gen_expr(node->lhs);
      load(node->ty);
      return;
    case ND_ADDR:
      gen_addr(node->lhs);
      return;
    case ND_ASSIGN:
      gen_addr(node->lhs);
      push();
      gen_expr(node->rhs);

      if (node->lhs->kind == ND_MEMBER && node->lhs->member->is_bitfield) {
        ///| mov r8, rax

        // If the lhs is a bitfield, we need to read the current value
        // from memory and merge it with a new value.
        Member* mem = node->lhs->member;
        ///| mov RUTIL, rax
        ///| and RUTIL, (1L << mem->bit_width) - 1
        ///| shl RUTIL, mem->bit_offset

        ///| mov rax, [rsp]
        load(mem->ty);

        long mask = ((1L << mem->bit_width) - 1) << mem->bit_offset;
        ///| mov r9, ~mask
        ///| and rax, r9
        ///| or rax, RUTIL
        store(node->ty);
        ///| mov rax, r8
        return;
      }

      store(node->ty);
      return;
    case ND_STMT_EXPR:
      for (Node* n = node->body; n; n = n->next)
        gen_stmt(n);
      return;
    case ND_COMMA:
      gen_expr(node->lhs);
      gen_expr(node->rhs);
      return;
    case ND_CAST:
      gen_expr(node->lhs);
      cast(node->lhs->ty, node->ty);
      return;
    case ND_MEMZERO:
      // `rep stosb` is equivalent to `memset(rdi, al, rcx)`.
#if X64WIN
      ///| push rdi
#endif
      ///| mov rcx, node->var->ty->size
      ///| lea rdi, [rbp+node->var->offset]
      ///| mov al, 0
      ///| rep
      ///| stosb
#if X64WIN
      ///| pop rdi
#endif
      return;
    case ND_COND: {
      int lelse = codegen_pclabel();
      int lend = codegen_pclabel();
      gen_expr(node->cond);
      cmp_zero(node->cond->ty);
      ///| je =>lelse
      gen_expr(node->then);
      ///| jmp =>lend
      ///|=>lelse:
      gen_expr(node->els);
      ///|=>lend:
      return;
    }
    case ND_NOT:
      gen_expr(node->lhs);
      cmp_zero(node->lhs->ty);
      ///| sete al
      ///| movzx rax, al
      return;
    case ND_BITNOT:
      gen_expr(node->lhs);
      ///| not rax
      return;
    case ND_LOGAND: {
      int lfalse = codegen_pclabel();
      int lend = codegen_pclabel();
      gen_expr(node->lhs);
      cmp_zero(node->lhs->ty);
      ///| je =>lfalse
      gen_expr(node->rhs);
      cmp_zero(node->rhs->ty);
      ///| je =>lfalse
      ///| mov rax, 1
      ///| jmp =>lend
      ///|=>lfalse:
      ///| mov rax, 0
      ///|=>lend:
      return;
    }
    case ND_LOGOR: {
      int ltrue = codegen_pclabel();
      int lend = codegen_pclabel();
      gen_expr(node->lhs);
      cmp_zero(node->lhs->ty);
      ///| jne =>ltrue
      gen_expr(node->rhs);
      cmp_zero(node->rhs->ty);
      ///| jne =>ltrue
      ///| mov rax, 0
      ///| jmp =>lend
      ///|=>ltrue:
      ///| mov rax, 1
      ///|=>lend:
      return;
    }
    case ND_FUNCALL: {
      if (node->lhs->kind == ND_VAR && !strcmp(node->lhs->var->name, "alloca")) {
        gen_expr(node->args);
        ///| mov CARG1, rax
        builtin_alloca();
        return;
      }

#if X64WIN
      if (node->lhs->kind == ND_VAR && !strcmp(node->lhs->var->name, "__va_start")) {
        // va_start(ap, x) turns into __va_start(&ap, x), so we only want the
        // expr here, not the address.
        gen_expr(node->args);
        push();
        // ToS is now &ap.

        gen_addr(node->args->next);
        // RAX is now &x, move it to the next qword.
        ///| add rax, 8

        // Store one-past the second argument into &ap.
        pop(REG_UTIL);
        ///| mov [RUTIL], rax
        return;
      }
#endif

#if X64WIN

      int by_ref_copies_size = 0;
      int stack_args = push_args_win(node, &by_ref_copies_size);
      gen_expr(node->lhs);

      int reg = 0;

      // If the return type is a large struct/union, the caller passes
      // a pointer to a buffer as if it were the first argument.
      if (node->ret_buffer && !type_passed_in_register(node->ty)) {
        pop(dasmargreg[reg++]);
      }

      for (Node* arg = node->args; arg; arg = arg->next) {
        Type* ty = arg->ty;

        switch (ty->kind) {
          case TY_STRUCT:
          case TY_UNION:
            if (type_passed_in_register(ty) || (arg->pass_by_reference && reg < X64WIN_REG_MAX)) {
              pop(dasmargreg[reg++]);
            }
            break;
          case TY_FLOAT:
          case TY_DOUBLE:
            if (reg < X64WIN_REG_MAX) {
              popf(reg);
              // Varargs requires a copy of fp in gp.
              ///| movd Rq(dasmargreg[reg]), xmm(reg)
              ++reg;
            }
            break;
          default:
            if (reg < X64WIN_REG_MAX) {
              pop(dasmargreg[reg++]);
            }
        }
      }

      ///| sub rsp, PARAMETER_SAVE_SIZE
      ///| mov r10, rax
      ///| call r10
      ///| add rsp, stack_args*8 + PARAMETER_SAVE_SIZE + by_ref_copies_size
      if (by_ref_copies_size > 0) {
        ///| pop r11
      }

      depth -= by_ref_copies_size / 8;
      depth -= stack_args;

      // It looks like the most significant 48 or 56 bits in RAX may
      // contain garbage if a function return type is short or bool/char,
      // respectively. We clear the upper bits here.
      switch (node->ty->kind) {
        case TY_BOOL:
          ///| movzx eax, al
          return;
        case TY_CHAR:
          if (node->ty->is_unsigned) {
            ///| movzx eax, al
          } else {
            ///| movsx eax, al
          }
          return;
        case TY_SHORT:
          if (node->ty->is_unsigned) {
            ///| movzx eax, ax
          } else {
            ///| movsx eax, ax
          }
          return;
      }

      // If the return type is a small struct, a value is returned it's actually
      // returned in rax, so copy it back into the return buffer where we're
      // expecting it.
      if (node->ret_buffer && type_passed_in_register(node->ty)) {
        ///| mov [rbp+node->ret_buffer->offset], rax
        ///| lea rax, [rbp+node->ret_buffer->offset]
      }

#else  // SysV

      int stack_args = push_args_sysv(node);
      gen_expr(node->lhs);

      int gp = 0, fp = 0;

      // If the return type is a large struct/union, the caller passes
      // a pointer to a buffer as if it were the first argument.
      if (node->ret_buffer && node->ty->size > 16) {
        pop(dasmargreg[gp++]);
      }

      for (Node* arg = node->args; arg; arg = arg->next) {
        Type* ty = arg->ty;

        switch (ty->kind) {
          case TY_STRUCT:
          case TY_UNION:
            if (ty->size > 16)
              continue;

            bool fp1 = has_flonum1(ty);
            bool fp2 = has_flonum2(ty);

            if (fp + fp1 + fp2 < SYSV_FP_MAX && gp + !fp1 + !fp2 < SYSV_GP_MAX) {
              if (fp1) {
                popf(fp++);
              } else {
                pop(dasmargreg[gp++]);
              }

              if (ty->size > 8) {
                if (fp2) {
                  popf(fp++);
                } else {
                  pop(dasmargreg[gp++]);
                }
              }
            }
            break;
          case TY_FLOAT:
          case TY_DOUBLE:
            if (fp < SYSV_FP_MAX)
              popf(fp++);
            break;
          case TY_LDOUBLE:
            break;
          default:
            if (gp < SYSV_GP_MAX) {
              pop(dasmargreg[gp++]);
            }
        }
      }

      ///| mov r10, rax
      ///| mov rax, fp
      ///| call r10
      ///| add rsp, stack_args*8

      depth -= stack_args;

      // It looks like the most significant 48 or 56 bits in RAX may
      // contain garbage if a function return type is short or bool/char,
      // respectively. We clear the upper bits here.
      switch (node->ty->kind) {
        case TY_BOOL:
          ///| movzx eax, al
          return;
        case TY_CHAR:
          if (node->ty->is_unsigned) {
            ///| movzx eax, al
          } else {
            ///| movsx eax, al
          }
          return;
        case TY_SHORT:
          if (node->ty->is_unsigned) {
            ///| movzx eax, ax
          } else {
            ///| movsx eax, ax
          }
          return;
      }

      // If the return type is a small struct, a value is returned
      // using up to two registers.
      if (node->ret_buffer && node->ty->size <= 16) {
        copy_ret_buffer(node->ret_buffer);
        ///| lea rax, [rbp+node->ret_buffer->offset]
      }

#endif  // SysV

      return;
    }
    case ND_LABEL_VAL:
      ///| lea rax, [=>node->unique_pc_label]
      return;
    case ND_CAS: {
      gen_expr(node->cas_addr);
      push();
      gen_expr(node->cas_new);
      push();
      gen_expr(node->cas_old);
      ///| mov r8, rax
      load(node->cas_old->ty->base);
      pop(REG_DX);    // new
      pop(REG_UTIL);  // addr

      int sz = node->cas_addr->ty->base->size;
      // dynasm doesn't support cmpxchg, and I didn't grok the encoding yet.
      // Hack in the various bytes for the instructions we want since there's
      // limited forms. RUTILenc is either 0x17 for RDI or 0x11 for RCX
      // depending on whether we're encoding for Windows or SysV.
      switch (sz) {
        case 1:
          // lock cmpxchg BYTE PTR [rdi/rcx], dl
          ///| .byte 0xf0
          ///| .byte 0x0f
          ///| .byte 0xb0
          ///| .byte RUTILenc
          break;
        case 2:
          // lock cmpxchg WORD PTR [rdi/rcx],dx
          ///| .byte 0x66
          ///| .byte 0xf0
          ///| .byte 0x0f
          ///| .byte 0xb1
          ///| .byte RUTILenc
          break;
        case 4:
          // lock cmpxchg DWORD PTR [rdi/rcx],edx
          ///| .byte 0xf0
          ///| .byte 0x0f
          ///| .byte 0xb1
          ///| .byte RUTILenc
          break;
        case 8:
          // lock cmpxchg QWORD PTR [rdi/rcx],rdx
          ///| .byte 0xf0
          ///| .byte 0x48
          ///| .byte 0x0f
          ///| .byte 0xb1
          ///| .byte RUTILenc
          break;
        default:
          unreachable();
      }
      ///| sete cl
      ///| je >1
      switch (sz) {
        case 1:
          ///| mov [r8], al
          break;
        case 2:
          ///| mov [r8], ax
          break;
        case 4:
          ///| mov [r8], eax
          break;
        case 8:
          ///| mov [r8], rax
          break;
        default:
          unreachable();
      }
      ///|1:
      ///| movzx eax, cl

      return;
    }
    case ND_EXCH: {
      gen_expr(node->lhs);
      push();
      gen_expr(node->rhs);
      pop(REG_UTIL);

      int sz = node->lhs->ty->base->size;
      switch (sz) {
        case 1:
          ///| xchg [RUTIL], al
          break;
        case 2:
          ///| xchg [RUTIL], ax
          break;
        case 4:
          ///| xchg [RUTIL], eax
          break;
        case 8:
          ///| xchg [RUTIL], rax
          break;
        default:
          unreachable();
      }
      return;
    }
  }

  switch (node->lhs->ty->kind) {
    case TY_FLOAT:
    case TY_DOUBLE: {
      gen_expr(node->rhs);
      pushf();
      gen_expr(node->lhs);
      popf(1);

      bool is_float = node->lhs->ty->kind == TY_FLOAT;

      switch (node->kind) {
        case ND_ADD:
          if (is_float) {
            ///| addss xmm0, xmm1
          } else {
            ///| addsd xmm0, xmm1
          }
          return;
        case ND_SUB:
          if (is_float) {
            ///| subss xmm0, xmm1
          } else {
            ///| subsd xmm0, xmm1
          }
          return;
        case ND_MUL:
          if (is_float) {
            ///| mulss xmm0, xmm1
          } else {
            ///| mulsd xmm0, xmm1
          }
          return;
        case ND_DIV:
          if (is_float) {
            ///| divss xmm0, xmm1
          } else {
            ///| divsd xmm0, xmm1
          }
          return;
        case ND_EQ:
        case ND_NE:
        case ND_LT:
        case ND_LE:
          if (is_float) {
            ///| ucomiss xmm1, xmm0
          } else {
            ///| ucomisd xmm1, xmm0
          }

          if (node->kind == ND_EQ) {
            ///| sete al
            ///| setnp dl
            ///| and al, dl
          } else if (node->kind == ND_NE) {
            ///| setne al
            ///| setp dl
            ///| or al, dl
          } else if (node->kind == ND_LT) {
            ///| seta al
          } else {
            ///| setae al
          }

          ///| and al, 1
          ///| movzx rax, al
          return;
      }

      error_tok(node->tok, "invalid expression");
    }
#if !X64WIN
    case TY_LDOUBLE: {
      gen_expr(node->lhs);
      gen_expr(node->rhs);

      switch (node->kind) {
        case ND_ADD:
          ///| faddp st1, st0
          return;
        case ND_SUB:
          ///| fsubrp st1, st0
          return;
        case ND_MUL:
          ///| fmulp st1, st0
          return;
        case ND_DIV:
          ///| fdivrp st1, st0
          return;
        case ND_EQ:
        case ND_NE:
        case ND_LT:
        case ND_LE:
          ///| fcomip st1
          ///| fstp st0

          if (node->kind == ND_EQ) {
            ///| sete al
          } else if (node->kind == ND_NE) {
            ///| setne al
          } else if (node->kind == ND_LT) {
            ///| seta al
          } else {
            ///| setae al
          }

          ///| movzx rax, al
          return;
      }

      error_tok(node->tok, "invalid expression");
    }
#endif
  }

  gen_expr(node->rhs);
  push();
  gen_expr(node->lhs);
  pop(REG_UTIL);

  bool is_long = node->lhs->ty->kind == TY_LONG || node->lhs->ty->base;

  switch (node->kind) {
    case ND_ADD:
      if (is_long) {
        ///| add rax, RUTIL
      } else {
        ///| add eax, RUTILd
      }
      return;
    case ND_SUB:
      if (is_long) {
        ///| sub rax, RUTIL
      } else {
        ///| sub eax, RUTILd
      }
      return;
    case ND_MUL:
      if (is_long) {
        ///| imul rax, RUTIL
      } else {
        ///| imul eax, RUTILd
      }
      return;
    case ND_DIV:
    case ND_MOD:
      if (node->ty->is_unsigned) {
        if (is_long) {
          ///| mov rdx, 0
          ///| div RUTIL
        } else {
          ///| mov edx, 0
          ///| div RUTILd
        }
      } else {
        if (node->lhs->ty->size == 8) {
          ///| cqo
        } else {
          ///| cdq
        }
        if (is_long) {
          ///| idiv RUTIL
        } else {
          ///| idiv RUTILd
        }
      }

      if (node->kind == ND_MOD) {
        ///| mov rax, rdx
      }
      return;
    case ND_BITAND:
      if (is_long) {
        ///| and rax, RUTIL
      } else {
        ///| and eax, RUTILd
      }
      return;
    case ND_BITOR:
      if (is_long) {
        ///| or rax, RUTIL
      } else {
        ///| or eax, RUTILd
      }
      return;
    case ND_BITXOR:
      if (is_long) {
        ///| xor rax, RUTIL
      } else {
        ///| xor eax, RUTILd
      }
      return;
    case ND_EQ:
    case ND_NE:
    case ND_LT:
    case ND_LE:
      if (is_long) {
        ///| cmp rax, RUTIL
      } else {
        ///| cmp eax, RUTILd
      }

      if (node->kind == ND_EQ) {
        ///| sete al
      } else if (node->kind == ND_NE) {
        ///| setne al
      } else if (node->kind == ND_LT) {
        if (node->lhs->ty->is_unsigned) {
          ///| setb al
        } else {
          ///| setl al
        }
      } else if (node->kind == ND_LE) {
        if (node->lhs->ty->is_unsigned) {
          ///| setbe al
        } else {
          ///| setle al
        }
      }

      ///| movzx rax, al
      return;
    case ND_SHL:
      ///| mov rcx, RUTIL
      if (is_long) {
        ///| shl rax, cl
      } else {
        ///| shl eax, cl
      }
      return;
    case ND_SHR:
      ///| mov rcx, RUTIL
      if (node->lhs->ty->is_unsigned) {
        if (is_long) {
          ///| shr rax, cl
        } else {
          ///| shr eax, cl
        }
      } else {
        if (is_long) {
          ///| sar rax, cl
        } else {
          ///| sar eax, cl
        }
      }
      return;
  }

  error_tok(node->tok, "invalid expression");
}

static void gen_stmt(Node* node) {
  switch (node->kind) {
    case ND_IF: {
      int lelse = codegen_pclabel();
      int lend = codegen_pclabel();
      gen_expr(node->cond);
      cmp_zero(node->cond->ty);
      ///| je =>lelse
      gen_stmt(node->then);
      ///| jmp =>lend
      ///|=>lelse:
      if (node->els)
        gen_stmt(node->els);
      ///|=>lend:
      return;
    }
    case ND_FOR: {
      if (node->init)
        gen_stmt(node->init);
      int lbegin = codegen_pclabel();
      ///|=>lbegin:
      if (node->cond) {
        gen_expr(node->cond);
        cmp_zero(node->cond->ty);
        ///| je =>node->brk_pc_label
      }
      gen_stmt(node->then);
      ///|=>node->cont_pc_label:
      if (node->inc)
        gen_expr(node->inc);
      ///| jmp =>lbegin
      ///|=>node->brk_pc_label:
      return;
    }
    case ND_DO: {
      int lbegin = codegen_pclabel();
      ///|=>lbegin:
      gen_stmt(node->then);
      ///|=>node->cont_pc_label:
      gen_expr(node->cond);
      cmp_zero(node->cond->ty);
      ///| jne =>lbegin
      ///|=>node->brk_pc_label:
      return;
    }
    case ND_SWITCH:
      gen_expr(node->cond);

      for (Node* n = node->case_next; n; n = n->case_next) {
        bool is_long = node->cond->ty->size == 8;

        if (n->begin == n->end) {
          if (is_long) {
            ///| cmp rax, n->begin
          } else {
            ///| cmp eax, n->begin
          }
          ///| je =>n->pc_label
          continue;
        }

        // [GNU] Case ranges
        if (is_long) {
          ///| mov RUTIL, rax
          ///| sub RUTIL, n->begin
          ///| cmp RUTIL, n->end - n->begin
        } else {
          ///| mov RUTILd, eax
          ///| sub RUTILd, n->begin
          ///| cmp RUTILd, n->end - n->begin
        }
        ///| jbe =>n->pc_label
      }

      if (node->default_case) {
        ///| jmp =>node->default_case->pc_label
      }

      ///| jmp =>node->brk_pc_label
      gen_stmt(node->then);
      ///|=>node->brk_pc_label:
      return;
    case ND_CASE:
      ///|=>node->pc_label:
      gen_stmt(node->lhs);
      return;
    case ND_BLOCK:
      for (Node* n = node->body; n; n = n->next)
        gen_stmt(n);
      return;
    case ND_GOTO:
      ///| jmp =>node->unique_pc_label
      return;
    case ND_GOTO_EXPR:
      gen_expr(node->lhs);
      ///| jmp rax
      return;
    case ND_LABEL:
      ///|=>node->unique_pc_label:
      gen_stmt(node->lhs);
      return;
    case ND_RETURN:
      if (node->lhs) {
        gen_expr(node->lhs);
        Type* ty = node->lhs->ty;

        switch (ty->kind) {
          case TY_STRUCT:
          case TY_UNION:
            if (
#if X64WIN
                type_passed_in_register(ty)
#else
                ty->size <= 16
#endif
            ) {
              copy_struct_reg();
            } else {
              copy_struct_mem();
            }
            break;
        }
      }

      ///| jmp =>current_fn->dasm_return_label
      return;
    case ND_EXPR_STMT:
      gen_expr(node->lhs);
      return;
    case ND_ASM:
      error_tok(node->tok, "asm statement not supported");
  }

  error_tok(node->tok, "invalid statement");
}

#if X64WIN

// Assign offsets to local variables.
static void assign_lvar_offsets(Obj* prog) {
  for (Obj* fn = prog; fn; fn = fn->next) {
    if (!fn->is_function || !fn->is_definition || !fn->is_live)
      continue;

    // fprintf(stderr, "--- %s\n", fn->name);

    // The parameter home area starts at 16 above rbp:
    //   ...
    //   stack arg 2 (6th arg)
    //   stack arg 1 (5th arg)
    //   R9 home
    //   R8 home
    //   RDX home
    //   RCX home
    //   return address pushed by call instr
    //   old RBP (for the called function)  <<< RBP after push rbp; mov rbp, rsp
    //   ...
    //   ... stack space used by called function
    //   ...
    //
    // The top of the diagram is addr 0xffffffff.. and the bottom is 0.
    // PUSH decrements RSP and then stores.
    // So, "top" means the highest numbered address corresponding the to root
    // function and bottom moves to the frames for the leaf-ward functions.
    int top = 16;
    int bottom = 0;

    int reg = 0;

    // Assign offsets to pass-by-stack parameters and register homes.
    for (Obj* var = fn->params; var; var = var->next) {
      Type* ty = var->ty;

      switch (ty->kind) {
        case TY_STRUCT:
        case TY_UNION:
          if (!type_passed_in_register(ty)) {
            // If it's too big for a register, then the value we're getting is a
            // pointer to a copy, rather than the actual value, so flag it as
            // such and then either assign a register or stack slot for the
            // reference.
            // fprintf(stderr, "by ref %s\n", var->name);
            // var->passed_by_reference = true;
          }

          // If the pointer to a referenced value or the value itself can be
          // passed in a register then assign here.
          if (reg++ < X64WIN_REG_MAX) {
            var->offset = top;
            // fprintf(stderr, "  assigned reg offset 0x%x\n", var->offset);
            top += 8;
            continue;
          }

          // Otherwise fall through to the stack slot assignment below.
          break;
        case TY_FLOAT:
        case TY_DOUBLE:
          if (reg++ < X64WIN_REG_MAX) {
            var->offset = top;
            top += 8;
            continue;
          }
          break;
        default:
          if (reg++ < X64WIN_REG_MAX) {
            var->offset = top;
            top += 8;
            // fprintf(stderr, "int reg %s at home 0x%x\n", var->name, var->offset);
            continue;
          }
      }

      var->offset = top;
      // fprintf(stderr, "int stack %s at stack 0x%x\n", var->name, var->offset);
      top += MAX(8, var->ty->size);
    }

    // Assign offsets to local variables.
    for (Obj* var = fn->locals; var; var = var->next) {
      if (var->offset) {
        continue;
      }

      int align =
          (var->ty->kind == TY_ARRAY && var->ty->size >= 16) ? MAX(16, var->align) : var->align;

      bottom += var->ty->size;
      bottom = (int)align_to_s(bottom, align);
      var->offset = -bottom;
      // fprintf(stderr, "local %s at -0x%x\n", var->name, -var->offset);
    }

    fn->stack_size = (int)align_to_s(bottom, 16);
  }
}

#else  // SysV

// Assign offsets to local variables.
static void assign_lvar_offsets(Obj* prog) {
  for (Obj* fn = prog; fn; fn = fn->next) {
    if (!fn->is_function)
      continue;

    // If a function has many parameters, some parameters are
    // inevitably passed by stack rather than by register.
    // The first passed-by-stack parameter resides at RBP+16.
    int top = 16;
    int bottom = 0;

    int gp = 0, fp = 0;

    // Assign offsets to pass-by-stack parameters.
    for (Obj* var = fn->params; var; var = var->next) {
      Type* ty = var->ty;

      switch (ty->kind) {
        case TY_STRUCT:
        case TY_UNION:
          if (ty->size <= 16) {
            bool fp1 = has_flonum(ty, 0, 8, 0);
            bool fp2 = has_flonum(ty, 8, 16, 8);
            if (fp + fp1 + fp2 < SYSV_FP_MAX && gp + !fp1 + !fp2 < SYSV_GP_MAX) {
              fp = fp + fp1 + fp2;
              gp = gp + !fp1 + !fp2;
              continue;
            }
          }
          break;
        case TY_FLOAT:
        case TY_DOUBLE:
          if (fp++ < SYSV_FP_MAX)
            continue;
          break;
        case TY_LDOUBLE:
          break;
        default:
          if (gp++ < SYSV_GP_MAX)
            continue;
      }

      top = align_to_s(top, 8);
      var->offset = top;
      top += var->ty->size;
    }

    // Assign offsets to pass-by-register parameters and local variables.
    for (Obj* var = fn->locals; var; var = var->next) {
      if (var->offset)
        continue;

      // AMD64 System V ABI has a special alignment rule for an array of
      // length at least 16 bytes. We need to align such array to at least
      // 16-byte boundaries. See p.14 of
      // https://github.com/hjl-tools/x86-psABI/wiki/x86-64-psABI-draft.pdf.
      int align =
          (var->ty->kind == TY_ARRAY && var->ty->size >= 16) ? MAX(16, var->align) : var->align;

      bottom += var->ty->size;
      bottom = align_to_s(bottom, align);
      var->offset = -bottom;
    }

    fn->stack_size = align_to_s(bottom, 16);
  }
}

#endif  // SysV

static void emit_data(Obj* prog) {
  for (Obj* var = prog; var; var = var->next) {
    // printf("var->name %s %d %d %d %d\n", var->name, var->is_function, var->is_definition,
    // var->is_static, var->is_tentative);
    if (var->is_function)
      continue;

    if (!var->is_definition) {
      continue;
    }

    int align =
        (var->ty->kind == TY_ARRAY && var->ty->size >= 16) ? MAX(16, var->align) : var->align;

    // Common symbol
    // TODO: Currently forcing -fno-common because... I don't really understand
    // common, apparently.
    // if (false && var->is_tentative && !var->is_static) {
    // println("  common %s %d:%d", var->name, var->ty->size, align);
    // continue;
    //}

    write_dyo_initialized_data(dyo_file, var->ty->size, align, var->is_static, var->name);

    // .data or .tdata
    if (var->init_data) {
      Relocation* rel = var->rel;
      int pos = 0;
      ByteArray bytes = {NULL, 0, 0};
      while (pos < var->ty->size) {
        if (rel && rel->offset == pos) {
          if (bytes.len > 0) {
            write_dyo_initializer_bytes(dyo_file, bytes.data, bytes.len);
            bytes = (ByteArray){NULL, 0, 0};
          }

          assert(!(rel->data_label && rel->code_label));  // Shouldn't be both.
          assert(rel->data_label || rel->code_label);  // But should be at least one if we're here.

          if (rel->data_label) {
            write_dyo_initializer_data_relocation(dyo_file, *rel->data_label, rel->addend);
          } else {
            int file_loc;
            write_dyo_initializer_code_relocation(dyo_file, -1, rel->addend, &file_loc);
            intintarray_push(&pending_code_pclabels, (IntInt){file_loc, *rel->code_label});
          }

          rel = rel->next;
          pos += 8;
        } else {
          bytearray_push(&bytes, var->init_data[pos]);
          ++pos;
        }
      }

      if (bytes.len > 0) {
        write_dyo_initializer_bytes(dyo_file, bytes.data, bytes.len);
        bytes = (ByteArray){NULL, 0, 0};
      }

      write_dyo_initializer_end(dyo_file);
      continue;
    }

    // .bss or .tbss
    write_dyo_initializer_end(dyo_file);
  }
}

static void store_fp(int r, int offset, int sz) {
  switch (sz) {
    case 4:
      ///| movss dword [rbp+offset], xmm(r)
      return;
    case 8:
      ///| movsd qword [rbp+offset], xmm(r)
      return;
  }
  unreachable();
}

static void store_gp(int r, int offset, int sz) {
  switch (sz) {
    case 1:
      ///| mov [rbp+offset], Rb(dasmargreg[r])
      return;
    case 2:
      ///| mov [rbp+offset], Rw(dasmargreg[r])
      return;
      return;
    case 4:
      ///| mov [rbp+offset], Rd(dasmargreg[r])
      return;
    case 8:
      ///| mov [rbp+offset], Rq(dasmargreg[r])
      return;
    default:
      for (int i = 0; i < sz; i++) {
        ///| mov [rbp+offset+i], Rb(dasmargreg[r])
        ///| shr Rq(dasmargreg[r]), 8
      }
      return;
  }
}

static void emit_text(Obj* prog) {
  // Preallocate the dasm labels so they can be used in functions out of order.
  for (Obj* fn = prog; fn; fn = fn->next) {
    if (!fn->is_function || !fn->is_definition || !fn->is_live)
      continue;

    fn->dasm_return_label = codegen_pclabel();
    fn->dasm_entry_label = codegen_pclabel();
  }

  for (Obj* fn = prog; fn; fn = fn->next) {
    if (!fn->is_function || !fn->is_definition || !fn->is_live)
      continue;

    ///|=>fn->dasm_entry_label:

    current_fn = fn;

    // fprintf(stderr, "---- %s\n", fn->name);

    // Prologue
    ///| push rbp
    ///| mov rbp, rsp
    ///| sub rsp, fn->stack_size
    ///| mov [rbp+fn->alloca_bottom->offset], rsp

#if !X64WIN
    // Save arg registers if function is variadic
    if (fn->va_area) {
      int gp = 0, fp = 0;
      for (Obj* var = fn->params; var; var = var->next) {
        if (is_flonum(var->ty))
          fp++;
        else
          gp++;
      }

      int off = fn->va_area->offset;

      // va_elem
      ///| mov dword [rbp+off], gp*8            // gp_offset
      ///| mov dword [rbp+off+4], fp * 8 + 48   // fp_offset
      ///| mov [rbp+off+8], rbp                 // overflow_arg_area
      ///| add qword [rbp+off+8], 16
      ///| mov [rbp+off+16], rbp                // reg_save_area
      ///| add qword [rbp+off+16], off+24

      // __reg_save_area__
      ///| mov [rbp + off + 24], rdi
      ///| mov [rbp + off + 32], rsi
      ///| mov [rbp + off + 40], rdx
      ///| mov [rbp + off + 48], rcx
      ///| mov [rbp + off + 56], r8
      ///| mov [rbp + off + 64], r9
      ///| movsd qword [rbp + off + 72], xmm0
      ///| movsd qword [rbp + off + 80], xmm1
      ///| movsd qword [rbp + off + 88], xmm2
      ///| movsd qword [rbp + off + 96], xmm3
      ///| movsd qword [rbp + off + 104], xmm4
      ///| movsd qword [rbp + off + 112], xmm5
      ///| movsd qword [rbp + off + 120], xmm6
      ///| movsd qword [rbp + off + 128], xmm7
    }
#endif

#if X64WIN
    // If variadic, we have to store all registers; floats will have been
    // duplicated into the integer registers.
    if (fn->ty->is_variadic) {
      ///| mov [rbp + 16], CARG1
      ///| mov [rbp + 24], CARG2
      ///| mov [rbp + 32], CARG3
      ///| mov [rbp + 40], CARG4
    } else {
      // Save passed-by-register arguments to the stack
      int reg = 0;
      for (Obj* var = fn->params; var; var = var->next) {
        if (var->offset >= 16 + PARAMETER_SAVE_SIZE)
          continue;

        Type* ty = var->ty;

        switch (ty->kind) {
          case TY_STRUCT:
          case TY_UNION:
            // It's either small and so passed in a register, or isn't and then
            // we're instead storing the pointer to the larger struct.
            store_gp(reg++, var->offset, MIN(8, ty->size));
            break;
          case TY_FLOAT:
          case TY_DOUBLE:
            store_fp(reg++, var->offset, ty->size);
            break;
          default:
            store_gp(reg++, var->offset, ty->size);
            break;
        }
      }
    }
#else
    // Save passed-by-register arguments to the stack
    int gp = 0, fp = 0;
    for (Obj* var = fn->params; var; var = var->next) {
      if (var->offset > 0)
        continue;

      Type* ty = var->ty;

      switch (ty->kind) {
        case TY_STRUCT:
        case TY_UNION:
          assert(ty->size <= 16);
          if (has_flonum(ty, 0, 8, 0))
            store_fp(fp++, var->offset, MIN(8, ty->size));
          else
            store_gp(gp++, var->offset, MIN(8, ty->size));

          if (ty->size > 8) {
            if (has_flonum(ty, 8, 16, 0))
              store_fp(fp++, var->offset + 8, ty->size - 8);
            else
              store_gp(gp++, var->offset + 8, ty->size - 8);
          }
          break;
        case TY_FLOAT:
        case TY_DOUBLE:
          store_fp(fp++, var->offset, ty->size);
          break;
        default:
          store_gp(gp++, var->offset, ty->size);
      }
    }
#endif

    // Emit code
    gen_stmt(fn->body);
    assert(depth == 0);

    // [https://www.sigbus.info/n1570#5.1.2.2.3p1] The C spec defines
    // a special rule for the main function. Reaching the end of the
    // main function is equivalent to returning 0, even though the
    // behavior is undefined for the other functions.
    if (strcmp(fn->name, "main") == 0) {
      ///| mov rax, 0
      dasm_label_main_entry = fn->dasm_entry_label;
    }

    // Epilogue
    ///|=>fn->dasm_return_label:
    ///| mov rsp, rbp
    ///| pop rbp
    ///| ret
  }
}

static void write_text_exports(Obj* prog) {
  for (Obj* fn = prog; fn; fn = fn->next) {
    if (!fn->is_function || !fn->is_definition || !fn->is_live)
      continue;

    if (!fn->is_static) {
      write_dyo_function_export(dyo_file, fn->name, dasm_getpclabel(&dynasm, fn->dasm_entry_label));
    }
  }
}

static void write_imports(void) {
  for (int i = 0; i < import_fixups.len; ++i) {
    int offset = dasm_getpclabel(&dynasm, import_fixups.data[i].i);
    // +2 is a hack taking advantage of the fact that import fixups are always
    // of the form `mov64 rax, <ADDR>` which is encoded as:
    //   48 B8 <8 byte address>
    // so skip over the mov64 prefix and point directly at the address to be
    // slapped into place.
    offset += 2;

    write_dyo_import(dyo_file, import_fixups.data[i].str, offset);
  }
}

static void write_data_fixups(void) {
  for (int i = 0; i < data_fixups.len; ++i) {
    int offset = dasm_getpclabel(&dynasm, data_fixups.data[i].i);
    // +2 is a hack taking advantage of the fact that import fixups are always
    // of the form `mov64 rax, <ADDR>` which is encoded as:
    //   48 B8 <8 byte address>
    // so skip over the mov64 prefix and point directly at the address to be
    // slapped into place.
    offset += 2;

    write_dyo_code_reference_to_global(dyo_file, data_fixups.data[i].str, offset);
  }
}

static void update_pending_code_relocations(void) {
  for (int i = 0; i < pending_code_pclabels.len; ++i) {
    int file_loc = pending_code_pclabels.data[i].a;
    int pclabel = pending_code_pclabels.data[i].b;
    int offset = dasm_getpclabel(&dynasm, pclabel);
    // printf("update at %d, label %d, offset %d\n", file_loc, pclabel, offset);
    patch_dyo_initializer_code_relocation(dyo_file, file_loc, offset);
  }
}

void codegen_init(void) {
  dasm_init(&dynasm, DASM_MAXSECTION);
  dasm_growpc(&dynasm, 1 << 16);  // Arbitrary number to avoid lots of reallocs of that array.
}

void codegen(Obj* prog, FILE* dyo_out) {
  dyo_file = dyo_out;
  write_dyo_begin(dyo_file);

  void* globals[dynasm_globals_MAX + 1];
  dasm_setupglobal(&dynasm, globals, dynasm_globals_MAX + 1);

  dasm_setup(&dynasm, dynasm_actions);

  assign_lvar_offsets(prog);
  emit_data(prog);
  emit_text(prog);

  size_t code_size;
  dasm_link(&dynasm, &code_size);

  write_text_exports(prog);
  write_imports();
  write_data_fixups();
  update_pending_code_relocations();

  void* code_buf = malloc(code_size);

  dasm_encode(&dynasm, code_buf);

  int check_result = dasm_checkstep(&dynasm, DASM_SECTION_MAIN);
  if (check_result != DASM_S_OK) {
    fprintf(stderr, "got dasm_checkstep: 0x%08x\n", check_result);
    abort();
  }

  if (dasm_label_main_entry >= 0) {
    int offset = dasm_getpclabel(&dynasm, dasm_label_main_entry);
    write_dyo_entrypoint(dyo_file, offset);
  }

  write_dyo_code(dyo_file, code_buf, code_size);

  free(code_buf);

  dasm_free(&dynasm);
}

void codegen_reset(void) {
  depth = 0;
  current_fn = NULL;

  dynasm = NULL;
  numlabels = 1;
  dasm_label_main_entry = -1;
  import_fixups = (StringIntArray){NULL, 0, 0};
  data_fixups = (StringIntArray){NULL, 0, 0};
  pending_code_pclabels = (IntIntArray){NULL, 0, 0};
}
