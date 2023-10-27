#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE

#ifdef _MSC_VER
#pragma warning(disable : 4061)  // enumerator 'X' in switch of enum 'Y' is not explicitly handled
                                 // by a case label
#pragma warning(disable : 4062)  // enumerator 'X' in switch of enum 'Y' is not handled
#pragma warning(disable : 4668)  // C:\Program Files (x86)\Windows
                                 // Kits\10\\include\10.0.22621.0\\um\winioctl.h(10847): warning
                                 // C4668: '_WIN32_WINNT_WIN10_TH2' is not defined as a preprocessor
                                 // macro, replacing with '0' for '#if/#elif'
#pragma warning(disable : 4820)  // Padding bytes added
#pragma warning(disable : 5045)  // Compiler will insert Spectre mitigation for memory load if
                                 // /Qspectre switch specified
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif
#endif

#ifdef __clang__
#pragma clang diagnostic ignored "-Wmissing-field-initializers"
#pragma clang diagnostic ignored "-Wswitch"
#endif

#ifdef __GNUC__
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#pragma GCC diagnostic ignored "-Wswitch"
#endif

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>

// We use this as the global indication that we should be targeting the Win ABI
// rather than SysV. It's here rather than in config so that libdyibcc.c doesn't
// need special defines added.
#if defined(_WIN64) && defined(_M_AMD64)
#define X64WIN 1
#endif

#include "libdyibicc.h"

#ifdef _MSC_VER
#define NORETURN __declspec(noreturn)
#define strdup _strdup
#include <io.h>
#define isatty _isatty
#define fileno _fileno
#else
#define NORETURN _Noreturn
#include <unistd.h>
#endif

#if !X64WIN
#include <strings.h>
#endif

#define MAX(x, y) ((x) < (y) ? (y) : (x))
#define MIN(x, y) ((x) < (y) ? (x) : (y))

#ifndef __GNUC__
#define __attribute__(x)
#endif

typedef struct Type Type;
typedef struct Node Node;
typedef struct Member Member;
typedef struct Relocation Relocation;
typedef struct Hideset Hideset;
typedef struct Token Token;
typedef struct HashMap HashMap;
typedef struct UserContext UserContext;
typedef struct DbpContext DbpContext;
typedef struct DbpFunctionSymbol DbpFunctionSymbol;

//
// alloc.c
//
typedef enum AllocLifetime {
  AL_Compile = 0,  // Must be 0 so that 0-initialized structs default to this storage.
  AL_Temp,
  AL_Link,
  AL_UserContext,
  NUM_BUMP_HEAPS,
  AL_Manual = NUM_BUMP_HEAPS,
} AllocLifetime;

IMPLSTATIC void alloc_init(AllocLifetime lifetime);
IMPLSTATIC void alloc_reset(AllocLifetime lifetime);

IMPLSTATIC void* bumpcalloc(size_t num, size_t size, AllocLifetime lifetime);
IMPLSTATIC void* bumplamerealloc(void* old,
                                 size_t old_size,
                                 size_t new_size,
                                 AllocLifetime lifetime);
IMPLSTATIC void alloc_free(void* p, AllocLifetime lifetime);  // AL_Manual only.

IMPLSTATIC void* aligned_allocate(size_t size, size_t alignment);
IMPLSTATIC void aligned_free(void* p);
IMPLSTATIC void* allocate_writable_memory(size_t size);
IMPLSTATIC bool make_memory_readwrite(void* m, size_t size);
IMPLSTATIC bool make_memory_executable(void* m, size_t size);
IMPLSTATIC void free_executable_memory(void* p, size_t size);

//
// util.c
//

typedef struct File File;

typedef struct {
  char** data;
  int capacity;
  int len;
} StringArray;

typedef struct StringInt {
  char* str;
  int i;
} StringInt;

typedef struct StringIntArray {
  StringInt* data;
  int capacity;
  int len;
} StringIntArray;

typedef struct FilePtrArray {
  File** data;
  int capacity;
  int len;
} FilePtrArray;

typedef struct IntIntInt {
  int a;
  int b;
  int c;
} IntIntInt;

typedef struct IntIntIntArray {
  IntIntInt* data;
  int capacity;
  int len;
} IntIntIntArray;

IMPLSTATIC char* bumpstrndup(const char* s, size_t n, AllocLifetime lifetime);
IMPLSTATIC char* bumpstrdup(const char* s, AllocLifetime lifetime);
IMPLSTATIC char* dirname(char* s);
IMPLSTATIC uint64_t align_to_u(uint64_t n, uint64_t align);
IMPLSTATIC int64_t align_to_s(int64_t n, int64_t align);
IMPLSTATIC unsigned int get_page_size(void);
IMPLSTATIC void strarray_push(StringArray* arr, char* s, AllocLifetime lifetime);
IMPLSTATIC void strintarray_push(StringIntArray* arr, StringInt item, AllocLifetime lifetime);
IMPLSTATIC void fileptrarray_push(FilePtrArray* arr, File* item, AllocLifetime lifetime);
#if X64WIN
IMPLSTATIC void intintintarray_push(IntIntIntArray* arr, IntIntInt item, AllocLifetime lifetime);
#endif
IMPLSTATIC char* format(AllocLifetime lifetime, char* fmt, ...)
    __attribute__((format(printf, 2, 3)));
IMPLSTATIC char* read_file_wrap_user(char* path, AllocLifetime lifetime);
IMPLSTATIC NORETURN void error(char* fmt, ...) __attribute__((format(printf, 1, 2)));
IMPLSTATIC NORETURN void error_at(char* loc, char* fmt, ...) __attribute__((format(printf, 2, 3)));
IMPLSTATIC NORETURN void error_tok(Token* tok, char* fmt, ...)
    __attribute__((format(printf, 2, 3)));
IMPLSTATIC NORETURN void error_internal(char* file, int line, char* msg);
IMPLSTATIC int outaf(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
IMPLSTATIC void warn_tok(Token* tok, char* fmt, ...) __attribute__((format(printf, 2, 3)));
#if X64WIN
IMPLSTATIC void register_function_table_data(UserContext* ctx, int func_count, char* base_addr);
IMPLSTATIC void unregister_and_free_function_table_data(UserContext* ctx);
IMPLSTATIC char* get_temp_pdb_filename(AllocLifetime lifetime);
#endif

//
// tokenize.c
//

// Token
typedef enum {
  TK_IDENT,    // Identifiers
  TK_PUNCT,    // Punctuators
  TK_KEYWORD,  // Keywords
  TK_STR,      // String literals
  TK_NUM,      // Numeric literals
  TK_PP_NUM,   // Preprocessing numbers
  TK_EOF,      // End-of-file markers
} TokenKind;

struct File {
  char* name;
  char* contents;
  int file_no;  // Index into tokenize__all_tokenized_files.

  // For #line directive
  char* display_name;
  int line_delta;
};

// Token type
typedef struct Token Token;
struct Token {
  TokenKind kind;    // Token kind
  Token* next;       // Next token
  int64_t val;       // If kind is TK_NUM, its value
  long double fval;  // If kind is TK_NUM, its value
  char* loc;         // Token location
  int len;           // Token length
  Type* ty;          // Used if TK_NUM or TK_STR
  char* str;         // String literal contents including terminating '\0'

  File* file;        // Source location
  char* filename;    // Filename
  int line_no;       // Line number
  int line_delta;    // Line number
  bool at_bol;       // True if this token is at beginning of line
  bool has_space;    // True if this token follows a space character
  Hideset* hideset;  // For macro expansion
  Token* origin;     // If this is expanded from a macro, the original token
};

IMPLSTATIC bool equal(Token* tok, char* op);
IMPLSTATIC Token* skip(Token* tok, char* op);
IMPLSTATIC bool consume(Token** rest, Token* tok, char* str);
IMPLSTATIC void convert_pp_tokens(Token* tok);
IMPLSTATIC File* new_file(char* name, char* contents);
IMPLSTATIC Token* tokenize_string_literal(Token* tok, Type* basety);
IMPLSTATIC Token* tokenize(File* file);
IMPLSTATIC Token* tokenize_file(char* filename);
IMPLSTATIC Token* tokenize_filecontents(char* path, char* contents);

#define unreachable() error_internal(__FILE__, __LINE__, "unreachable")
#define ABORT(msg) error_internal(__FILE__, __LINE__, msg)

//
// preprocess.c
//

IMPLSTATIC char* search_include_paths(char* filename);
IMPLSTATIC void init_macros(void);
IMPLSTATIC void define_macro(char* name, char* buf);
IMPLSTATIC void undef_macro(char* name);
IMPLSTATIC Token* preprocess(Token* tok);

//
// parse.c
//

// Variable or function
typedef struct Obj Obj;
struct Obj {
  Obj* next;
  char* name;     // Variable name
  Type* ty;       // Type
  Token* tok;     // representative token
  bool is_local;  // local or global/function
#if X64WIN
  bool is_param_passed_by_reference;
#endif
  int align;  // alignment

  // Local variable
  int offset;

  // Global variable or function
  bool is_function;
  bool is_definition;
  bool is_static;
  int dasm_entry_label;
  int dasm_return_label;
  int dasm_end_of_function_label;
  int dasm_unwind_info_label;
#if X64WIN
  IntIntIntArray file_line_label_data;
#endif

  // Global variable
  bool is_tentative;
  bool is_tls;
  bool is_rodata;
  char* init_data;
  Relocation* rel;

  // Function
  bool is_inline;
  Obj* params;
  Node* body;
  Obj* locals;
  Obj* va_area;
  Obj* alloca_bottom;
  int stack_size;

  // Static inline function
  bool is_live;  // No code is emitted for "static inline" functions if no one is referencing them.
  bool is_root;
  StringArray refs;
};

// Global variable can be initialized either by a constant expression
// or a pointer to another global variable. This struct represents the
// latter.
struct Relocation {
  Relocation* next;
  int offset;
  char** string_label;
  int* internal_code_label;
  long addend;
};

// AST node
typedef enum {
  ND_NULL_EXPR,         // Do nothing
  ND_ADD,               // +
  ND_SUB,               // -
  ND_MUL,               // *
  ND_DIV,               // /
  ND_NEG,               // unary -
  ND_MOD,               // %
  ND_BITAND,            // &
  ND_BITOR,             // |
  ND_BITXOR,            // ^
  ND_SHL,               // <<
  ND_SHR,               // >>
  ND_EQ,                // ==
  ND_NE,                // !=
  ND_LT,                // <
  ND_LE,                // <=
  ND_ASSIGN,            // =
  ND_COND,              // ?:
  ND_COMMA,             // ,
  ND_MEMBER,            // . (struct member access)
  ND_ADDR,              // unary &
  ND_DEREF,             // unary *
  ND_NOT,               // !
  ND_BITNOT,            // ~
  ND_LOGAND,            // &&
  ND_LOGOR,             // ||
  ND_RETURN,            // "return"
  ND_IF,                // "if"
  ND_FOR,               // "for" or "while"
  ND_DO,                // "do"
  ND_SWITCH,            // "switch"
  ND_CASE,              // "case"
  ND_BLOCK,             // { ... }
  ND_GOTO,              // "goto"
  ND_GOTO_EXPR,         // "goto" labels-as-values
  ND_LABEL,             // Labeled statement
  ND_LABEL_VAL,         // [GNU] Labels-as-values
  ND_FUNCALL,           // Function call
  ND_EXPR_STMT,         // Expression statement
  ND_STMT_EXPR,         // Statement expression
  ND_VAR,               // Variable
  ND_VLA_PTR,           // VLA designator
  ND_REFLECT_TYPE_PTR,  // _ReflectType*
  ND_NUM,               // Integer
  ND_CAST,              // Type cast
  ND_MEMZERO,           // Zero-clear a stack variable
  ND_ASM,               // "asm"
  ND_CAS,               // Atomic compare-and-swap
  ND_LOCKCE,            // _InterlockedCompareExchange
  ND_EXCH,              // Atomic exchange
} NodeKind;

// AST node type
struct Node {
  NodeKind kind;  // Node kind
  Node* next;     // Next node
  Type* ty;       // Type, e.g. int or pointer to int
  Token* tok;     // Representative token

  Node* lhs;  // Left-hand side
  Node* rhs;  // Right-hand side

  // "if" or "for" statement
  Node* cond;
  Node* then;
  Node* els;
  Node* init;
  Node* inc;

  // "break" and "continue" labels
  int brk_pc_label;
  int cont_pc_label;

  // Block or statement expression
  Node* body;

  // Struct member access
  Member* member;

  // Function call
  Type* func_ty;
  Node* args;
  bool pass_by_stack;
#if X64WIN
  int pass_by_reference;  // Offset to copy of large struct.
#endif
  Obj* ret_buffer;

  // Goto or labeled statement, or labels-as-values
  char* label;
  int pc_label;
  Node* goto_next;

  // Switch
  Node* case_next;
  Node* default_case;

  // Case
  long begin;
  long end;

  // "asm" string literal
  char* asm_str;

  // Atomic compare-and-swap
  Node* cas_addr;
  Node* cas_old;
  Node* cas_new;

  // Atomic op= operators
  Obj* atomic_addr;
  Node* atomic_expr;

  // Variable
  Obj* var;

  // Numeric literal
  int64_t val;
  long double fval;

  uintptr_t reflect_ty;
};

IMPLSTATIC Node* new_cast(Node* expr, Type* ty);
IMPLSTATIC int64_t pp_const_expr(Token** rest, Token* tok);
IMPLSTATIC Obj* parse(Token* tok);

//
// type.c
//

typedef enum {
  TY_VOID,
  TY_BOOL,
  TY_CHAR,
  TY_SHORT,
  TY_INT,
  TY_LONG,
  TY_FLOAT,
  TY_DOUBLE,
#if X64WIN
  TY_LDOUBLE = TY_DOUBLE,
#else
  TY_LDOUBLE,
#endif
  TY_ENUM = 9,
  TY_PTR,
  TY_FUNC,
  TY_ARRAY,
  TY_VLA,  // variable-length array
  TY_STRUCT,
  TY_UNION,
} TypeKind;

struct Type {
  TypeKind kind;
  int size;          // sizeof() value
  int align;         // alignment
  bool is_unsigned;  // unsigned or signed
  bool is_atomic;    // true if _Atomic
  Type* origin;      // for type compatibility check

  // Pointer-to or array-of type. We intentionally use the same member
  // to represent pointer/array duality in C.
  //
  // In many contexts in which a pointer is expected, we examine this
  // member instead of "kind" member to determine whether a type is a
  // pointer or not. That means in many contexts "array of T" is
  // naturally handled as if it were "pointer to T", as required by
  // the C spec.
  Type* base;

  // Declaration
  Token* name;
  Token* name_pos;

  // Array
  int array_len;

  // Variable-length array
  Node* vla_len;  // # of elements
  Obj* vla_size;  // sizeof() value

  // Struct
  Member* members;
  bool is_flexible;
  bool is_packed;
  Token* stencil_prefix;

  // Function type
  Type* return_ty;
  Type* params;
  bool is_variadic;
  Type* next;
};

// Struct member
struct Member {
  Member* next;
  Type* ty;
  Token* tok;  // for error message
  Token* name;
  int idx;
  int align;
  int offset;

  // Bitfield
  bool is_bitfield;
  int bit_offset;
  int bit_width;
};

IMPLEXTERN Type* ty_void;
IMPLEXTERN Type* ty_bool;

IMPLEXTERN Type* ty_char;
IMPLEXTERN Type* ty_short;
IMPLEXTERN Type* ty_int;
IMPLEXTERN Type* ty_long;

IMPLEXTERN Type* ty_uchar;
IMPLEXTERN Type* ty_ushort;
IMPLEXTERN Type* ty_uint;
IMPLEXTERN Type* ty_ulong;

IMPLEXTERN Type* ty_float;
IMPLEXTERN Type* ty_double;
IMPLEXTERN Type* ty_ldouble;

IMPLEXTERN Type* ty_typedesc;

IMPLSTATIC bool is_integer(Type* ty);
IMPLSTATIC bool is_flonum(Type* ty);
IMPLSTATIC bool is_numeric(Type* ty);
IMPLSTATIC bool is_void(Type* ty);
IMPLSTATIC bool is_compatible(Type* t1, Type* t2);
IMPLSTATIC Type* copy_type(Type* ty);
IMPLSTATIC Type* pointer_to(Type* base);
IMPLSTATIC Type* func_type(Type* return_ty);
IMPLSTATIC Type* array_of(Type* base, int size, Token* err_tok);
IMPLSTATIC Type* vla_of(Type* base, Node* expr);
IMPLSTATIC Type* enum_type(void);
IMPLSTATIC Type* struct_type(void);
IMPLSTATIC void add_type(Node* node);

//
// codegen.c
//
typedef struct CompileOutputs {
  HashMap* codeseg_static_symbols;
  HashMap* codeseg_global_symbols;
} CompileOutputs;

IMPLSTATIC void codegen_init(void);
IMPLSTATIC void codegen(Obj* prog, size_t file_index);
IMPLSTATIC void codegen_free(void);
IMPLSTATIC int codegen_pclabel(void);
#if X64WIN
IMPLSTATIC bool type_passed_in_register(Type* ty);
#endif

//
// unicode.c
//

IMPLSTATIC int encode_utf8(char* buf, uint32_t c);
IMPLSTATIC uint32_t decode_utf8(char** new_pos, char* p);
IMPLSTATIC bool is_ident1(uint32_t c);
IMPLSTATIC bool is_ident2(uint32_t c);
IMPLSTATIC int display_width(char* p, int len);

//
// hashmap.c
//

typedef struct {
  char* key;
  int keylen;
  void* val;
} HashEntry;

struct HashMap {
  HashEntry* buckets;
  int capacity;
  int used;
  AllocLifetime alloc_lifetime;
};

IMPLSTATIC void* hashmap_get(HashMap* map, char* key);
IMPLSTATIC void* hashmap_get2(HashMap* map, char* key, int keylen);
IMPLSTATIC void hashmap_put(HashMap* map, char* key, void* val);
IMPLSTATIC void hashmap_put2(HashMap* map, char* key, int keylen, void* val);
IMPLSTATIC void hashmap_delete(HashMap* map, char* key);
IMPLSTATIC void hashmap_delete2(HashMap* map, char* key, int keylen);
IMPLSTATIC void hashmap_clear_manual_key_owned_value_owned_aligned(HashMap* map);
IMPLSTATIC void hashmap_clear_manual_key_owned_value_unowned(HashMap* map);

//
// link.c
//
IMPLSTATIC bool link_all_files(void);

//
// Entire compiler state in one struct and linker in a second for clearing, esp.
// after longjmp. There should be no globals outside of these structures.
//

typedef struct CondIncl CondIncl;

typedef struct Scope Scope;
// Represents a block scope.
struct Scope {
  Scope* next;

  // C has two block scopes; one is for variables/typedefs and
  // the other is for struct/union/enum tags.
  HashMap vars;
  HashMap tags;
};

typedef struct LinkFixup {
  // The address to fix up.
  void* at;

  // Name of the symbol at which the fixup should point.
  // TODO: Intern pool for all the import/export/global names.
  char* name;

  // Added to the address that |name| resolves to.
  int addend;
} LinkFixup;

typedef struct FileLinkData {
  char* source_name;
  char* codeseg_base_address;  // Just the address, not a string.
  size_t codeseg_size;

  LinkFixup* fixups;
  int flen;
  int fcap;
} FileLinkData;

IMPLSTATIC void free_link_fixups(FileLinkData* fld);

struct UserContext {
  DyibiccLoadFileContents load_file_contents;
  DyibiccFunctionLookupFn get_function_address;
  DyibiccOutputFn output_function;
  bool use_ansi_codes;
  bool generate_debug_symbols;

  size_t num_include_paths;
  char** include_paths;

  size_t num_files;
  FileLinkData* files;

  // This is an array of num_files+1; 0..num_files-1 correspond to static
  // globals in the files in FileLinkData, and global_data[num_files] is the
  // fully global (exported) symbols. These HashMaps are also special because
  // they're lifetime == AL_Manual.
  HashMap* global_data;

  HashMap* exports;

  HashMap reflect_types;

#if X64WIN
  char* function_table_data;
  DbpContext* dbp_ctx;
#endif
};

typedef struct dasm_State dasm_State;

typedef struct CompilerState {
  // tokenize.c
  File* tokenize__current_file;  // Input file
  bool tokenize__at_bol;         // True if the current position is at the beginning of a line
  bool tokenize__has_space;      // True if the current position follows a space character
  HashMap tokenize__keyword_map;
  FilePtrArray tokenize__all_tokenized_files;

  // preprocess.c
  HashMap preprocess__macros;
  CondIncl* preprocess__cond_incl;
  HashMap preprocess__pragma_once;
  int preprocess__include_next_idx;
  HashMap preprocess__include_path_cache;
  HashMap preprocess__include_guards;
  int preprocess__counter_macro_i;

  // parse.c
  Obj* parse__locals;   // All local variable instances created during parsing are accumulated to
                        // this list.
  Obj* parse__globals;  // Likewise, global variables are accumulated to this list.
  Scope* parse__scope;  // NOTE: needs to be reinitialized after clear to point at empty_scope.
  Scope parse__empty_scope;
  Obj* parse__current_fn;  // Points to the function object the parser is currently parsing.
  Node* parse__gotos;      // Lists of all goto statements and labels in the curent function.
  Node* parse__labels;
  int parse__brk_pc_label;  // Current "goto" and "continue" jump targets.
  int parse__cont_pc_label;
  Node* parse__current_switch;  // Points to a node representing a switch if we are parsing a switch
                                // statement. Otherwise, NULL.
  Obj* parse__builtin_alloca;
  int parse__unique_name_id;
  HashMap parse__typename_map;
  bool parse__evaluating_pp_const;

  // codegen.in.c
  int codegen__depth;
  size_t codegen__file_index;
  dasm_State* codegen__dynasm;
  Obj* codegen__current_fn;
  int codegen__numlabels;
  StringIntArray codegen__fixups;

  // main.c
  char* main__base_file;
} CompilerState;

typedef struct LinkerState {
  // link.c
  HashMap link__runtime_function_map;
} LinkerState;

IMPLEXTERN UserContext* user_context;
IMPLEXTERN jmp_buf toplevel_update_jmpbuf;
IMPLEXTERN CompilerState compiler_state;
IMPLEXTERN LinkerState linker_state;
