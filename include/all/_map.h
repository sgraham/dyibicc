// BEGIN regen_container_headers.py
#ifndef __dyibicc_internal_include__
#error Can only be included by the compiler, or confusing errors will result!
#endif
#undef __attribute__
typedef unsigned char uint8_t;
typedef unsigned long long uint64_t;
typedef unsigned long long size_t;
typedef long long int64_t;
typedef signed char int8_t;
typedef signed short int16_t;
typedef unsigned short uint16_t;
typedef signed int int32_t;
typedef long long intptr_t;
typedef unsigned long long uintptr_t;
typedef long long ptrdiff_t;
typedef unsigned int uint32_t;
void* memset(void* dest, int ch, size_t count);
void* memcpy(void* dest, const void* src, size_t count);
int memcmp(const void* lhs, const void* rhs, size_t count);
void* memmove(void* dest, const void* src, size_t count);
size_t strlen(const char* str);
void free(void* ptr);
void *malloc(size_t size);
void *calloc(size_t num, size_t size);
void* realloc(void* ptr, size_t new_size);
#define NULL ((void*)0) /* todo! */
// ### END regen_container_headers.py
// ### BEGIN_FILE_INCLUDE: hmap.h

// Unordered set/map - implemented with the robin-hood hashing scheme.
// ### BEGIN_FILE_INCLUDE: linkage.h
#undef STC_API
#undef STC_DEF

#if !defined i_static && !defined STC_STATIC  && (defined i_header || defined STC_HEADER  || \
                                                  defined i_implement || defined STC_IMPLEMENT)
  #define STC_API extern
  #define STC_DEF
#else
  #define i_implement
  #if defined __GNUC__ || defined __clang__ || defined __INTEL_LLVM_COMPILER
    #define STC_API static __attribute__((unused))
  #else
    #define STC_API static inline
  #endif
  #define STC_DEF static
#endif
#if defined STC_IMPLEMENT || defined i_import
  #define i_implement
#endif

#if !defined i_allocator
  #define i_allocator c
#endif
#ifndef i_free
  #define i_malloc c_JOIN(i_allocator, _malloc)
  #define i_calloc c_JOIN(i_allocator, _calloc)
  #define i_realloc c_JOIN(i_allocator, _realloc)
  #define i_free c_JOIN(i_allocator, _free)
#endif

#if defined __clang__ && !defined __cplusplus
  #pragma clang diagnostic push
  #pragma clang diagnostic warning "-Wall"
  #pragma clang diagnostic warning "-Wextra"
  #pragma clang diagnostic warning "-Wpedantic"
  #pragma clang diagnostic warning "-Wconversion"
  #pragma clang diagnostic warning "-Wwrite-strings"
  // ignored
  #pragma clang diagnostic ignored "-Wmissing-field-initializers"
#elif defined __GNUC__ && !defined __cplusplus
  #pragma GCC diagnostic push
  #pragma GCC diagnostic warning "-Wall"
  #pragma GCC diagnostic warning "-Wextra"
  #pragma GCC diagnostic warning "-Wpedantic"
  #pragma GCC diagnostic warning "-Wconversion"
  #pragma GCC diagnostic warning "-Wwrite-strings"
  // ignored
  #pragma GCC diagnostic ignored "-Wclobbered"
  #pragma GCC diagnostic ignored "-Wimplicit-fallthrough=3"
  #pragma GCC diagnostic ignored "-Wstringop-overflow="
  #pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#endif
// ### END_FILE_INCLUDE: linkage.h
// ### BEGIN_FILE_INCLUDE: types.h

#ifdef i_aux
  #define _i_aux_def i_aux aux;
#else
  #define _i_aux_def
#endif

#ifndef STC_TYPES_H_INCLUDED
#define STC_TYPES_H_INCLUDED

// EXCLUDED BY regen_container_headers.py #include <stdint.h>
// EXCLUDED BY regen_container_headers.py #include <stddef.h>

#define declare_rc(C, KEY) declare_arc(C, KEY)
#define declare_vec(C, KEY) declare_stack(C, KEY)
#define declare_pqueue(C, KEY) declare_stack(C, KEY)
#define declare_deque(C, KEY) declare_queue(C, KEY)
#define declare_hashmap(C, KEY, VAL) declare_htable(C, KEY, VAL, c_true, c_false)
#define declare_hashset(C, KEY) declare_htable(C, cset, KEY, KEY, c_false, c_true)
#define declare_sortedmap(C, KEY, VAL) declare_aatree(C, KEY, VAL, c_true, c_false)
#define declare_sortedset(C, KEY) declare_aatree(C, KEY, KEY, c_false, c_true)

#define declare_hmap(...) declare_hashmap(__VA_ARGS__) // [deprecated]
#define declare_hset(...) declare_hashset(__VA_ARGS__) // [deprecated]
#define declare_smap(...) declare_sortedmap(__VA_ARGS__) // [deprecated]
#define declare_sset(...) declare_sortedset(__VA_ARGS__) // [deprecated]

// csview : non-null terminated string view
typedef const char csview_value;
typedef struct csview {
    csview_value* buf;
    ptrdiff_t size;
} csview;

typedef union {
    csview_value* ref;
    csview chr;
    struct { csview chr; csview_value* end; } u8;
} csview_iter;

#define c_sv(...) c_MACRO_OVERLOAD(c_sv, __VA_ARGS__)
#define c_sv_1(literal) c_sv_2(literal, c_litstrlen(literal))
#define c_sv_2(str, n) (c_literal(csview){str, n})
#define c_svfmt "%.*s"
#define c_svarg(sv) (int)(sv).size, (sv).buf // printf(c_svfmt "\n", c_svarg(sv));

// zsview : zero-terminated string view
typedef csview_value zsview_value;
typedef struct zsview {
    zsview_value* str;
    ptrdiff_t size;
} zsview;

typedef union {
    zsview_value* ref;
    csview chr;
} zsview_iter;

#define c_zv(literal) (c_literal(zsview){literal, c_litstrlen(literal)})

// cstr : zero-terminated owning string (short string optimized - sso)
typedef char cstr_value;
typedef struct { cstr_value* data; intptr_t size, cap; } cstr_buf;
typedef union cstr {
    struct { uintptr_t size; cstr_value* data; uintptr_t ncap; } lon;
    struct { cstr_value data[ sizeof(cstr_buf) - 1 ]; uint8_t size; } sml;
} cstr;

typedef union {
    csview chr; // utf8 character/codepoint
    const cstr_value* ref;
} cstr_iter;

#define c_true(...) __VA_ARGS__
#define c_false(...)

#define declare_arc(SELF, VAL) \
    typedef VAL SELF##_value; \
    typedef struct SELF##_ctrl SELF##_ctrl; \
\
    typedef union SELF { \
        SELF##_value* get; \
        SELF##_ctrl* ctrl1; \
    } SELF

#define declare_arc2(SELF, VAL) \
    typedef VAL SELF##_value; \
    typedef struct SELF##_ctrl SELF##_ctrl; \
\
typedef struct __attribute__((methodcall(SELF##_))) SELF { \
        SELF##_value* get; \
        SELF##_ctrl* ctrl2; \
    } SELF

#define declare_box(SELF, VAL) \
    typedef VAL SELF##_value; \
\
typedef struct __attribute__((methodcall(SELF##_))) SELF { \
        SELF##_value* get; \
    } SELF

#define declare_queue(SELF, VAL) \
    typedef VAL SELF##_value; \
\
typedef struct __attribute__((methodcall(SELF##_))) SELF { \
        SELF##_value *cbuf; \
        ptrdiff_t start, end, capmask; \
        _i_aux_def \
    } SELF; \
\
    typedef struct { \
        SELF##_value *ref; \
        ptrdiff_t pos; \
        const SELF* _s; \
    } SELF##_iter

#define declare_list(SELF, VAL) \
    typedef VAL SELF##_value; \
    typedef struct SELF##_node SELF##_node; \
\
    typedef struct { \
        SELF##_value *ref; \
        SELF##_node *const *_last, *prev; \
    } SELF##_iter; \
\
typedef struct __attribute__((methodcall(SELF##_))) SELF { \
        SELF##_node *last; \
        _i_aux_def \
    } SELF

#define declare_htable(SELF, KEY, VAL, MAP_ONLY, SET_ONLY) \
    typedef KEY SELF##_key; \
    typedef VAL SELF##_mapped; \
\
    typedef SET_ONLY( SELF##_key ) \
            MAP_ONLY( struct SELF##_value ) \
    SELF##_value, SELF##_entry; \
\
    typedef struct { \
        SELF##_value *ref; \
        size_t idx; \
        _Bool inserted; \
        uint8_t hashx; \
        uint16_t dist; \
    } SELF##_result; \
\
    typedef struct { \
        SELF##_value *ref, *_end; \
        struct hmap_meta *_mref; \
    } SELF##_iter; \
\
typedef struct __attribute__((methodcall(SELF##_))) SELF { \
        SELF##_value* table; \
        struct hmap_meta* meta; \
        ptrdiff_t size, bucket_count; \
        _i_aux_def \
    } SELF

#define declare_aatree(SELF, KEY, VAL, MAP_ONLY, SET_ONLY) \
    typedef KEY SELF##_key; \
    typedef VAL SELF##_mapped; \
    typedef struct SELF##_node SELF##_node; \
\
    typedef SET_ONLY( SELF##_key ) \
            MAP_ONLY( struct SELF##_value ) \
    SELF##_value, SELF##_entry; \
\
    typedef struct { \
        SELF##_value *ref; \
        _Bool inserted; \
    } SELF##_result; \
\
    typedef struct { \
        SELF##_value *ref; \
        SELF##_node *_d; \
        int _top; \
        int32_t _tn, _st[36]; \
    } SELF##_iter; \
\
typedef struct __attribute__((methodcall(SELF##_))) SELF { \
        SELF##_node *nodes; \
        int32_t root, disp, head, size, capacity; \
        _i_aux_def \
    } SELF

#define declare_stack_fixed(SELF, VAL, CAP) \
    typedef VAL SELF##_value; \
    typedef struct { SELF##_value *ref, *end; } SELF##_iter; \
typedef struct __attribute__((methodcall(SELF##_))) SELF { SELF##_value data[CAP]; ptrdiff_t size; } SELF

#define declare_stack(SELF, VAL) \
    typedef VAL SELF##_value; \
    typedef struct { SELF##_value *ref, *end; } SELF##_iter; \
typedef struct __attribute__((methodcall(SELF##_))) SELF { SELF##_value *data; ptrdiff_t size, capacity; _i_aux_def } SELF

#endif // STC_TYPES_H_INCLUDED
// ### END_FILE_INCLUDE: types.h

#ifndef STC_HMAP_H_INCLUDED
#define STC_HMAP_H_INCLUDED
// ### BEGIN_FILE_INCLUDE: common.h
#ifndef STC_COMMON_H_INCLUDED
#define STC_COMMON_H_INCLUDED

#ifdef _MSC_VER
    #pragma warning(disable: 4116 4996) // unnamed type definition in parentheses
#endif
// EXCLUDED BY regen_container_headers.py #include <inttypes.h>
// EXCLUDED BY regen_container_headers.py #include <stddef.h>
// EXCLUDED BY regen_container_headers.py #include <stdbool.h>
// EXCLUDED BY regen_container_headers.py #include <string.h>
// EXCLUDED BY regen_container_headers.py #include <assert.h>

typedef ptrdiff_t       isize;
#ifndef STC_NO_INT_DEFS
    typedef int8_t      int8;
    typedef uint8_t     uint8;
    typedef int16_t     int16;
    typedef uint16_t    uint16;
    typedef int32_t     int32;
    typedef uint32_t    uint32;
    typedef int64_t     int64;
    typedef uint64_t    uint64;
#endif
#if !defined STC_HAS_TYPEOF && (_MSC_FULL_VER >= 193933428 || \
    defined __GNUC__ || defined __clang__ || defined __TINYC__)
    #define STC_HAS_TYPEOF 1
#endif
#if defined __GNUC__
  #define c_GNUATTR(...) __attribute__((__VA_ARGS__))
#else
  #define c_GNUATTR(...)
#endif
#define STC_INLINE static inline c_GNUATTR(unused)
#define c_ZI PRIiPTR
#define c_ZU PRIuPTR
#define c_NPOS INTPTR_MAX

// Macro overloading feature support based on: https://rextester.com/ONP80107
#define c_MACRO_OVERLOAD(name, ...) \
    c_JOIN(c_JOIN0(name,_),c_NUMARGS(__VA_ARGS__))(__VA_ARGS__)
#define c_JOIN0(a, b) a ## b
#define c_JOIN(a, b) c_JOIN0(a, b)
#define c_EXPAND(...) __VA_ARGS__
#define c_NUMARGS(...) _c_APPLY_ARG_N((__VA_ARGS__, _c_RSEQ_N))
#define _c_APPLY_ARG_N(args) _c_ARG_N args  // wrap c_EXPAND(..) for MSVC without /std:c11
#define _c_RSEQ_N 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1,
#define _c_ARG_N(_1,_2,_3,_4,_5,_6,_7,_8,_9,_10,_11,_12,_13,_14,_15,_16,N,...) N

// Saturated overloading
// #define foo(...) foo_I(__VA_ARGS__, c_COMMA_N(foo_3), c_COMMA_N(foo_2), c_COMMA_N(foo_1),)(__VA_ARGS__)
// #define foo_I(a,b,c, n, ...) c_TUPLE_AT_1(n, foo_n,)
#define c_TUPLE_AT_1(x,y,...) y
#define c_COMMA_N(x) ,x

// Select arg, e.g. for #define i_type A,B then c_GETARG(2, i_type) is B
#define c_GETARG(N, ...) c_ARG_##N(__VA_ARGS__,) // wrap c_EXPAND(..) for MSVC without /std:c11
#define c_ARG_1(a, ...) a
#define c_ARG_2(a, b, ...) b
#define c_ARG_3(a, b, c, ...) c
#define c_ARG_4(a, b, c, d, ...) d

#define _i_malloc(T, n)     ((T*)i_malloc((n)*c_sizeof(T)))
#define _i_calloc(T, n)     ((T*)i_calloc((n), c_sizeof(T)))
#ifndef __cplusplus
    #define c_new(T, ...)   ((T*)c_safe_memcpy(c_malloc(c_sizeof(T)), ((T[]){__VA_ARGS__}), c_sizeof(T)))
    #define c_literal(T)    (T)
    #define c_make_array(T, ...) ((T[])__VA_ARGS__)
    #define c_make_array2d(T, N, ...) ((T[][N])__VA_ARGS__)
#else
// EXCLUDED BY regen_container_headers.py     #include <new>
    #define c_new(T, ...)       new (c_malloc(c_sizeof(T))) T(__VA_ARGS__)
    #define c_literal(T)        T
    template<typename T, int M, int N> struct _c_Array { T data[M][N]; };
    #define c_make_array(T, ...) (_c_Array<T, 1, sizeof((T[])__VA_ARGS__)/sizeof(T)>{{__VA_ARGS__}}.data[0])
    #define c_make_array2d(T, N, ...) (_c_Array<T, sizeof((T[][N])__VA_ARGS__)/sizeof(T[N]), N>{__VA_ARGS__}.data)
#endif
#ifdef STC_ALLOCATOR
    #define c_malloc c_JOIN(STC_ALLOCATOR, _malloc)
    #define c_calloc c_JOIN(STC_ALLOCATOR, _calloc)
    #define c_realloc c_JOIN(STC_ALLOCATOR, _realloc)
    #define c_free c_JOIN(STC_ALLOCATOR, _free)
#else
    #define c_malloc(sz)        malloc(c_i2u_size(sz))
    #define c_calloc(n, sz)     calloc(c_i2u_size(n), c_i2u_size(sz))
    #define c_realloc(ptr, old_sz, sz) realloc(ptr, c_i2u_size(1 ? (sz) : (old_sz)))
    #define c_free(ptr, sz)     ((void)(sz), free(ptr))
#endif
#define c_new_n(T, n)           ((T*)c_calloc(n, c_sizeof(T)))
#define c_delete(T, ptr)        do { T* _tp = ptr; T##_drop(_tp); c_free(_tp, c_sizeof(T)); } while (0)
#define c_delete_n(T, ptr, n)   do { T* _tp = ptr; isize _n = n, _m = _n; \
                                     while (_n--) T##_drop((_tp + _n)); \
                                     c_free(_tp, _m*c_sizeof(T)); } while (0)

#define c_static_assert(expr)   (void)sizeof(int[(expr) ? 1 : -1])
#if defined STC_NDEBUG || defined NDEBUG
    #define c_assert(expr)      (void)sizeof(expr)
#else
#ifndef STC_ASSERT
#define STC_ASSERT(expr)
#endif
    #define c_assert(expr)      STC_ASSERT(expr)
#endif
#define c_container_of(p, C, m) ((C*)((char*)(1 ? (p) : &((C*)0)->m) - offsetof(C, m)))
#define c_const_cast(Tp, p)     ((Tp)(1 ? (p) : (Tp)0))
#define c_litstrlen(literal)    (c_sizeof("" literal) - 1)
#define c_countof(a)            (isize)(sizeof(a)/sizeof 0[a])
#define c_arraylen(a)           c_countof(a)

// expect signed ints to/from these (use with gcc -Wconversion)
#define c_sizeof                (isize)sizeof
#define c_strlen(s)             (isize)strlen(s)
#define c_strncmp(a, b, ilen)   strncmp(a, b, c_i2u_size(ilen))
#define c_memcpy(d, s, ilen)    memcpy(d, s, c_i2u_size(ilen))
#define c_memmove(d, s, ilen)   memmove(d, s, c_i2u_size(ilen))
#define c_memset(d, val, ilen)  memset(d, val, c_i2u_size(ilen))
#define c_memcmp(a, b, ilen)    memcmp(a, b, c_i2u_size(ilen))
// library internal, but may be useful in user code:
#define c_u2i_size(u)           (isize)(1 ? (u) : (size_t)1) // warns if u is signed
#define c_i2u_size(i)           (size_t)(1 ? (i) : -1)       // warns if i is unsigned
#define c_uless(a, b)           ((size_t)(a) < (size_t)(b))
#define c_safe_cast(T, From, x) ((T)(1 ? (x) : (From){0}))

// x, y are i_keyraw* type, which defaults to i_key*. vp is i_key* type.
#define c_memcmp_eq(x, y)       (memcmp(x, y, sizeof *(x)) == 0)
#define c_default_eq(x, y)      (*(x) == *(y))
#define c_default_less(x, y)    (*(x) < *(y))
#define c_default_cmp(x, y)     (c_default_less(y, x) - c_default_less(x, y))
#define c_default_hash(vp)      c_hash_n(vp, sizeof *(vp))
#define c_default_clone(v)      (v)
#define c_default_toraw(vp)     (*(vp))
#define c_default_drop(vp)      ((void) (vp))

// non-owning char pointer
typedef const char* cstr_raw;
#define cstr_raw_cmp(x, y)      strcmp(*(x), *(y))
#define cstr_raw_eq(x, y)       (cstr_raw_cmp(x, y) == 0)
#define cstr_raw_hash(vp)       c_hash_str(*(vp))
#define cstr_raw_clone(v)       (v)
#define cstr_raw_drop(vp)       ((void)vp)

// Control block macros

// [deprecated]:
#define c_init(...) c_make(__VA_ARGS__)
#define c_forlist(...) for (c_items(_VA_ARGS__))
#define c_foritems(...) for (c_items(__VA_ARGS__))
#define c_foreach(...) for (c_each(__VA_ARGS__))
#define c_foreach_n(...) for (c_each_n(__VA_ARGS__))
#define c_foreach_kv(...) for (c_each_kv(__VA_ARGS__))
#define c_foreach_reverse(...) for (c_each_reverse(__VA_ARGS__))
#define c_forrange(...) for (c_range(__VA_ARGS__))
#define c_forrange32(...) for (c_range32(__VA_ARGS__))

// New:
#define c_each(...) c_MACRO_OVERLOAD(c_each, __VA_ARGS__)
#define c_each_3(it, C, cnt) \
    C##_iter it = C##_begin(&cnt); it.ref; C##_next(&it)
#define c_each_4(it, C, start, end) \
    _c_each(it, C, start, (end).ref, _)

#define c_each_n(it, C, cnt, n) \
    struct {C##_iter iter; C##_value* ref; isize size, index;} \
    it = {.iter=C##_begin(&cnt), .size=n}; (it.ref = it.iter.ref) && it.index < it.size; C##_next(&it.iter), ++it.index

#define c_each_reverse(...) c_MACRO_OVERLOAD(c_each_reverse, __VA_ARGS__)
#define c_each_reverse_3(it, C, cnt) /* works for stack, vec, queue, deque */ \
    C##_iter it = C##_rbegin(&cnt); it.ref; C##_rnext(&it)
#define c_each_reverse_4(it, C, start, end) \
    _c_each(it, C, start, (end).ref, _r)

#define _c_each(it, C, start, endref, rev) /* private */ \
    C##_iter it = (start), *_endref_##it = c_safe_cast(C##_iter*, C##_value*, endref) \
         ; it.ref != (C##_value*)_endref_##it; C##rev##next(&it)

#define c_each_kv(...) c_MACRO_OVERLOAD(c_each_kv, __VA_ARGS__)
#define c_each_kv_4(key, val, C, cnt) /* structured binding for maps */ \
    _c_each_kv(key, val, C, C##_begin(&cnt), NULL)
#define c_each_kv_5(key, val, C, start, end) \
    _c_each_kv(key, val, C, start, (end).ref)

#define _c_each_kv(key, val, C, start, endref) /* private */ \
    const C##_key *key = (const C##_key*)&key; key; ) \
    for (C##_mapped *val; key; key = NULL) \
    for (C##_iter _it_##key = start, *_endref_##key = c_safe_cast(C##_iter*, C##_value*, endref); \
         _it_##key.ref != (C##_value*)_endref_##key && (key = &_it_##key.ref->first, val = &_it_##key.ref->second); \
         C##_next(&_it_##key)

#define c_items(it, T, ...) \
    struct {T* ref; int size, index;} \
    it = {.ref=c_make_array(T, __VA_ARGS__), .size=(int)(sizeof((T[])__VA_ARGS__)/sizeof(T))} \
    ; it.index < it.size ; ++it.ref, ++it.index

// c_range, c_range32: python-like int range iteration
#define c_range_t(...) c_MACRO_OVERLOAD(c_range_t, __VA_ARGS__)
#define c_range_t_3(T, i, stop) c_range_t_4(T, i, 0, stop)
#define c_range_t_4(T, i, start, stop) \
    T i=start, _c_end_##i=stop; i < _c_end_##i; ++i
#define c_range_t_5(T, i, start, stop, step) \
    T i=start, _c_inc_##i=step, _c_end_##i=(stop) - (_c_inc_##i > 0) \
    ; (_c_inc_##i > 0) == (i <= _c_end_##i) ; i += _c_inc_##i

#define c_range(...) c_MACRO_OVERLOAD(c_range, __VA_ARGS__)
#define c_range_1(stop) c_range_t_4(isize, _c_i1, 0, stop)
#define c_range_2(i, stop) c_range_t_4(isize, i, 0, stop)
#define c_range_3(i, start, stop) c_range_t_4(isize, i, start, stop)
#define c_range_4(i, start, stop, step) c_range_t_5(isize, i, start, stop, step)

#define c_range32(...) c_MACRO_OVERLOAD(c_range32, __VA_ARGS__)
#define c_range32_2(i, stop) c_range_t_4(int32_t, i, 0, stop)
#define c_range32_3(i, start, stop) c_range_t_4(int32_t, i, start, stop)
#define c_range32_4(i, start, stop, step) c_range_t_5(int32_t, i, start, stop, step)

// make container from a literal list
#define c_make(C, ...) \
    C##_from_n(c_make_array(C##_raw, __VA_ARGS__), c_sizeof((C##_raw[])__VA_ARGS__)/c_sizeof(C##_raw))

// put multiple raw-type elements from a literal list into a container
#define c_put_items(C, cnt, ...) \
    C##_put_n(cnt, c_make_array(C##_raw, __VA_ARGS__), c_sizeof((C##_raw[])__VA_ARGS__)/c_sizeof(C##_raw))

// drop multiple containers of same type
#define c_drop(C, ...) \
    do { for (c_items(_c_i2, C*, {__VA_ARGS__})) C##_drop(*_c_i2.ref); } while(0)

// RAII scopes
#define c_defer(...) \
    for (int _c_i3 = 0; _c_i3++ == 0; __VA_ARGS__)

#define c_with(...) c_MACRO_OVERLOAD(c_with, __VA_ARGS__)
#define c_with_2(init, deinit) \
    for (int _c_i4 = 0; _c_i4 == 0; ) for (init; _c_i4++ == 0; deinit)
#define c_with_3(init, condition, deinit) \
    for (int _c_i5 = 0; _c_i5 == 0; ) for (init; _c_i5++ == 0 && (condition); deinit)

// General functions

STC_INLINE void* c_safe_memcpy(void* dst, const void* src, isize size)
    { return dst ? memcpy(dst, src, (size_t)size) : NULL; }

STC_INLINE size_t c_basehash_n(const void* key, isize len) {
    size_t block = 0, hash = 0x811c9dc5;
    const uint8_t* msg = (const uint8_t*)key;
    while (len > c_sizeof(size_t)) {
        memcpy(&block, msg, sizeof(size_t));
        hash = (hash ^ block) * (size_t)0x89bb179901000193;
        msg += c_sizeof(size_t);
        len -= c_sizeof(size_t);
    }
    c_memcpy(&block, msg, len);
    hash = (hash ^ block) * (size_t)0xb0340f4501000193;
    return hash ^ (hash >> 3);
}

STC_INLINE size_t c_hash_n(const void* key, isize len) {
    uint64_t b8; uint32_t b4;
    switch (len) {
        case 8: memcpy(&b8, key, 8); return (size_t)(b8 * 0xc6a4a7935bd1e99d);
        case 4: memcpy(&b4, key, 4); return b4 * (size_t)0xa2ffeb2f01000193;
        default: return c_basehash_n(key, len);
    }
}

STC_INLINE size_t c_hash_str(const char *str)
    { return c_basehash_n(str, c_strlen(str)); }

#define c_hash_mix(...) /* non-commutative hash combine! */ \
    _chash_mix(c_make_array(size_t, {__VA_ARGS__}), c_NUMARGS(__VA_ARGS__))

STC_INLINE size_t _chash_mix(size_t h[], int n) {
    for (int i = 1; i < n; ++i) h[0] += h[0] ^ h[i];
    return h[0];
}

// generic typesafe swap
#define c_swap(xp, yp) do { \
    (void)sizeof((xp) == (yp)); \
    char _tv[sizeof *(xp)]; \
    void *_xp = xp, *_yp = yp; \
    memcpy(_tv, _xp, sizeof _tv); \
    memcpy(_xp, _yp, sizeof _tv); \
    memcpy(_yp, _tv, sizeof _tv); \
} while (0)

// get next power of two
STC_INLINE isize c_next_pow2(isize n) {
    n--;
    n |= n >> 1, n |= n >> 2;
    n |= n >> 4, n |= n >> 8;
    n |= n >> 16;
    #if INTPTR_MAX == INT64_MAX
    n |= n >> 32;
    #endif
    return n + 1;
}

STC_INLINE char* c_strnstrn(const char *str, isize slen, const char *needle, isize nlen) {
    if (nlen == 0) return (char *)str;
    if (nlen > slen) return NULL;
    slen -= nlen;
    do {
        if (*str == *needle && !c_memcmp(str, needle, nlen))
            return (char *)str;
        ++str;
    } while (slen--);
    return NULL;
}
#endif // STC_COMMON_H_INCLUDED
// ### END_FILE_INCLUDE: common.h
// EXCLUDED BY regen_container_headers.py #include <stdlib.h>
#define _hashmask 0x3fU
#define _distmask 0x3ffU
struct hmap_meta { uint16_t hashx:6, dist:10; }; // dist: 0=empty, 1=PSL 0, 2=PSL 1, ...
#endif // STC_HMAP_H_INCLUDED

#ifndef _i_prefix
  #define _i_prefix hmap_
#endif
#ifndef _i_is_set
  #define _i_is_map
  #define _i_MAP_ONLY c_true
  #define _i_SET_ONLY c_false
  #define _i_keyref(vp) (&(vp)->first)
#else
  #define _i_MAP_ONLY c_false
  #define _i_SET_ONLY c_true
  #define _i_keyref(vp) (vp)
#endif
#define _i_is_hash
// ### BEGIN_FILE_INCLUDE: template.h
// IWYU pragma: private
#ifndef _i_template
#define _i_template

#ifndef STC_TEMPLATE_H_INCLUDED
#define STC_TEMPLATE_H_INCLUDED

  #define _c_MEMB(name) c_JOIN(Self, name)
  #define _c_DEFTYPES(macro, SELF, ...) macro(SELF, __VA_ARGS__)
  #define _m_value _c_MEMB(_value)
  #define _m_key _c_MEMB(_key)
  #define _m_mapped _c_MEMB(_mapped)
  #define _m_rmapped _c_MEMB(_rmapped)
  #define _m_raw _c_MEMB(_raw)
  #define _m_keyraw _c_MEMB(_keyraw)
  #define _m_iter _c_MEMB(_iter)
  #define _m_result _c_MEMB(_result)
  #define _m_node _c_MEMB(_node)

  #define c_OPTION(flag)  ((i_opt) & (flag))
  #define c_declared      (1<<0)
  #define c_no_atomic     (1<<1)
  #define c_arc2          (1<<2)
  #define c_no_clone      (1<<3)
  #define c_no_hash       (1<<4)
  #define c_use_cmp       (1<<5)
  #define c_use_eq        (1<<6)
  #define c_cmpclass      (1<<7)
  #define c_keyclass      (1<<8)
  #define c_valclass      (1<<9)
  #define c_keypro        (1<<10)
  #define c_valpro        (1<<11)
#endif

#if defined i_rawclass   // [deprecated]
  #define i_cmpclass i_rawclass
#elif defined i_key_arcbox // [deprecated]
  #define i_keypro i_key_arcbox
#elif defined i_key_str  // [deprecated]
  #define i_keypro cstr
  #define i_tag str
#endif
#if defined i_val_arcbox // [deprecated]
  #define i_valpro i_val_arcbox
#elif defined i_val_str  // [deprecated]
  #define i_valpro cstr
  #define i_tag str
#endif

#if defined T && !defined i_type
  #define i_type T
#endif
#if defined i_type && c_NUMARGS(i_type) > 1
  #define Self c_GETARG(1, i_type)
  #define i_key c_GETARG(2, i_type)
  #if c_NUMARGS(i_type) == 3
    #if defined _i_is_map
      #define i_val c_GETARG(3, i_type)
    #else
      #define i_opt c_GETARG(3, i_type)
    #endif
  #elif c_NUMARGS(i_type) == 4
    #define i_val c_GETARG(3, i_type)
    #define i_opt c_GETARG(4, i_type)
  #endif
#elif !defined Self && defined i_type
  #define Self i_type
#elif !defined Self
  #define Self c_JOIN(_i_prefix, i_tag)
#endif

#if c_OPTION(c_declared)
  #define i_declared
#endif
#if c_OPTION(c_no_hash)
  #define i_no_hash
#endif
#if c_OPTION(c_use_cmp)
  #define i_use_cmp
#endif
#if c_OPTION(c_use_eq)
  #define i_use_eq
#endif
#if c_OPTION(c_no_clone) || defined _i_is_arc
  #define i_no_clone
#endif
#if c_OPTION(c_keyclass)
  #define i_keyclass i_key
#endif
#if c_OPTION(c_valclass)
  #define i_valclass i_val
#endif
#if c_OPTION(c_cmpclass)
  #define i_cmpclass i_key
  #define i_use_cmp
#endif
#if c_OPTION(c_keypro)
  #define i_keypro i_key
#endif
#if c_OPTION(c_valpro)
  #define i_valpro i_val
#endif

#if defined i_keypro
  #define i_keyclass i_keypro
  #define i_cmpclass c_JOIN(i_keypro, _raw)
#endif

#if defined i_cmpclass
  #define i_keyraw i_cmpclass
  #if !(defined i_key || defined i_keyclass)
    #define i_key i_cmpclass
  #endif
#elif defined i_keyclass && !defined i_keyraw
  // Special: When only i_keyclass is defined, also define i_cmpclass to the same.
  // Do not define i_keyraw here, otherwise _from() / _toraw() is expected to exist.
  #define i_cmpclass i_key
#endif

// Bind to i_key "class members": _clone, _drop, _from and _toraw (when conditions are met).
#if defined i_keyclass
  #ifndef i_key
    #define i_key i_keyclass
  #endif
  #if !defined i_keyclone && !defined i_no_clone
    #define i_keyclone c_JOIN(i_keyclass, _clone)
  #endif
  #ifndef i_keydrop
    #define i_keydrop c_JOIN(i_keyclass, _drop)
  #endif
  #if !defined i_keyfrom && defined i_keyraw
    #define i_keyfrom c_JOIN(i_keyclass, _from)
  #elif !defined i_keyfrom && !defined i_no_clone
    #define i_keyfrom c_JOIN(i_keyclass, _clone)
  #endif
  #if !defined i_keytoraw && defined i_keyraw
    #define i_keytoraw c_JOIN(i_keyclass, _toraw)
  #endif
#endif

// Define when container has support for sorting (cmp) and linear search (eq)
#if defined i_use_cmp || defined i_cmp || defined i_less
  #define _i_has_cmp
#endif
#if defined i_use_cmp || defined i_cmp || defined i_use_eq || defined i_eq
  #define _i_has_eq
#endif

// Bind to i_cmpclass "class members": _cmp, _eq and _hash (when conditions are met).
#if defined i_cmpclass
  #if !(defined i_cmp || defined i_less) && (defined i_use_cmp || defined _i_sorted)
    #define i_cmp c_JOIN(i_cmpclass, _cmp)
  #endif
  #if !defined i_eq && (defined i_use_eq || defined i_hash || defined _i_is_hash)
    #define i_eq c_JOIN(i_cmpclass, _eq)
  #endif
  #if !(defined i_hash || defined i_no_hash)
    #define i_hash c_JOIN(i_cmpclass, _hash)
  #endif
#endif

#if !defined i_key
  #error "No i_key defined"
#elif defined i_keyraw && !(c_OPTION(c_cmpclass) || defined i_keytoraw)
  #error "If i_cmpclass / i_keyraw is defined, i_keytoraw must be defined too"
#elif !defined i_no_clone && (defined i_keyclone ^ defined i_keydrop)
  #error "Both i_keyclone and i_keydrop must be defined, if any (unless i_no_clone defined)."
#elif defined i_from || defined i_drop
  #error "i_from / i_drop not supported. Use i_keyfrom/i_keydrop"
#elif defined i_keyto || defined i_valto
  #error i_keyto / i_valto not supported. Use i_keytoraw / i_valtoraw
#elif defined i_keyraw && defined i_use_cmp && !defined _i_has_cmp
  #error "For smap / sset / pqueue, i_cmp or i_less must be defined when i_keyraw is defined."
#endif

// Fill in missing i_eq, i_less, i_cmp functions with defaults.
#if !defined i_eq && defined i_cmp
  #define i_eq(x, y) (i_cmp(x, y)) == 0
#elif !defined i_eq
  #define i_eq(x, y) *x == *y // works for integral types
#endif
#if !defined i_less && defined i_cmp
  #define i_less(x, y) (i_cmp(x, y)) < 0
#elif !defined i_less
  #define i_less(x, y) *x < *y // works for integral types
#endif
#if !defined i_cmp && defined i_less
  #define i_cmp(x, y) (i_less(y, x)) - (i_less(x, y))
#endif
#if !(defined i_hash || defined i_no_hash)
  #define i_hash c_default_hash
#endif

#define i_no_emplace

#ifndef i_tag
  #define i_tag i_key
#endif
#if !defined i_keyfrom && defined i_keyclone && !defined i_keyraw
  #define i_keyfrom i_keyclone
#elif !defined i_keyfrom
  #define i_keyfrom c_default_clone
#else
  #undef i_no_emplace
#endif
#ifndef i_keyraw
  #define i_keyraw i_key
#endif
#ifndef i_keytoraw
  #define i_keytoraw c_default_toraw
#endif
#ifndef i_keyclone
  #define i_keyclone c_default_clone
#endif
#ifndef i_keydrop
  #define i_keydrop c_default_drop
#endif

#if defined _i_is_map // ---- process hashmap/sortedmap value i_val, ... ----

#if defined i_valpro
  #define i_valclass i_valpro
  #define i_valraw c_JOIN(i_valpro, _raw)
#endif

#ifdef i_valclass
  #ifndef i_val
    #define i_val i_valclass
  #endif
  #if !defined i_valclone && !defined i_no_clone
    #define i_valclone c_JOIN(i_valclass, _clone)
  #endif
  #ifndef i_valdrop
    #define i_valdrop c_JOIN(i_valclass, _drop)
  #endif
  #if !defined i_valfrom && defined i_valraw
    #define i_valfrom c_JOIN(i_valclass, _from)
  #elif !defined i_valfrom && !defined i_no_clone
    #define i_valfrom c_JOIN(i_valclass, _clone)
  #endif
  #if !defined i_valtoraw && defined i_valraw
    #define i_valtoraw c_JOIN(i_valclass, _toraw)
  #endif
#endif

#ifndef i_val
  #error "i_val* must be defined for maps"
#elif defined i_valraw && !defined i_valtoraw
  #error "If i_valraw is defined, i_valtoraw must be defined too"
#elif !defined i_no_clone && (defined i_valclone ^ defined i_valdrop)
  #error "Both i_valclone and i_valdrop must be defined, if any"
#endif

#if !defined i_valfrom && defined i_valclone && !defined i_valraw
  #define i_valfrom i_valclone
#elif !defined i_valfrom
  #define i_valfrom c_default_clone
#else
  #undef i_no_emplace
#endif
#ifndef i_valraw
  #define i_valraw i_val
#endif
#ifndef i_valtoraw
  #define i_valtoraw c_default_toraw
#endif
#ifndef i_valclone
  #define i_valclone c_default_clone
#endif
#ifndef i_valdrop
  #define i_valdrop c_default_drop
#endif

#endif // !_i_is_map

#ifndef i_val
  #define i_val i_key
#endif
#ifndef i_valraw
  #define i_valraw i_keyraw
#endif
#endif // STC_TEMPLATE_H_INCLUDED
// ### END_FILE_INCLUDE: template.h
#ifndef i_declared
  _c_DEFTYPES(declare_htable, Self, i_key, i_val, _i_MAP_ONLY, _i_SET_ONLY);
#endif

_i_MAP_ONLY( struct _m_value {
    _m_key first;
    _m_mapped second;
}; )

typedef i_keyraw _m_keyraw;
typedef i_valraw _m_rmapped;
typedef _i_SET_ONLY( i_keyraw )
        _i_MAP_ONLY( struct { _m_keyraw first;
                              _m_rmapped second; } )
_m_raw;

STC_API Self            _c_MEMB(_with_capacity)(isize cap);
#if !defined i_no_clone
STC_API Self            _c_MEMB(_clone)(Self map);
#endif
STC_API void            _c_MEMB(_drop)(const Self* cself);
STC_API void            _c_MEMB(_clear)(Self* self);
STC_API _Bool            _c_MEMB(_reserve)(Self* self, isize capacity);
STC_API void            _c_MEMB(_erase_entry)(Self* self, _m_value* val);
STC_API float           _c_MEMB(_max_load_factor)(const Self* self);
STC_API isize           _c_MEMB(_capacity)(const Self* map);
static _m_result        _c_MEMB(_bucket_lookup_)(const Self* self, const _m_keyraw* rkeyptr);
static _m_result        _c_MEMB(_bucket_insert_)(const Self* self, const _m_keyraw* rkeyptr);

STC_INLINE void         _c_MEMB(_shrink_to_fit)(Self* self) { _c_MEMB(_reserve)(self, (isize)self->size); }
STC_INLINE _Bool         _c_MEMB(_is_empty)(const Self* map) { return !map->size; }
STC_INLINE isize        _c_MEMB(_size)(const Self* map) { return (isize)map->size; }
STC_INLINE isize        _c_MEMB(_bucket_count)(Self* map) { return map->bucket_count; }
STC_INLINE _Bool         _c_MEMB(_contains)(const Self* self, _m_keyraw rkey)
                            { return self->size && _c_MEMB(_bucket_lookup_)(self, &rkey).ref; }

#ifndef i_max_load_factor
  #define i_max_load_factor 0.80f
#endif

STC_INLINE _m_result
_c_MEMB(_insert_entry_)(Self* self, _m_keyraw rkey) {
    if (self->size >= (isize)((float)self->bucket_count * (i_max_load_factor)))
        if (!_c_MEMB(_reserve)(self, (isize)(self->size*3/2 + 2)))
            return c_literal(_m_result){0};

    _m_result res = _c_MEMB(_bucket_insert_)(self, &rkey);
    self->size += res.inserted;
    return res;
}

#ifdef _i_is_map
    STC_API _m_result _c_MEMB(_insert_or_assign)(Self* self, _m_key key, _m_mapped mapped);
    #if !defined i_no_emplace
    STC_API _m_result _c_MEMB(_emplace_or_assign)(Self* self, _m_keyraw rkey, _m_rmapped rmapped);
    #endif

    STC_INLINE const _m_mapped* _c_MEMB(_at)(const Self* self, _m_keyraw rkey) {
        _m_result res = _c_MEMB(_bucket_lookup_)(self, &rkey);
        c_assert(res.ref);
        return &res.ref->second;
    }

    STC_INLINE _m_mapped* _c_MEMB(_at_mut)(Self* self, _m_keyraw rkey)
        { return (_m_mapped*)_c_MEMB(_at)(self, rkey); }
#endif // _i_is_map

#if !defined i_no_clone
    STC_INLINE void _c_MEMB(_copy)(Self *self, const Self* other) {
        if (self == other)
            return;
        _c_MEMB(_drop)(self);
        *self = _c_MEMB(_clone)(*other);
    }

    STC_INLINE _m_value _c_MEMB(_value_clone)(const Self* self, _m_value _val) {
        (void)self;
        *_i_keyref(&_val) = i_keyclone((*_i_keyref(&_val)));
        _i_MAP_ONLY( _val.second = i_valclone(_val.second); )
        return _val;
    }
#endif // !i_no_clone

#if !defined i_no_emplace
    STC_INLINE _m_result
    _c_MEMB(_emplace)(Self* self, _m_keyraw rkey _i_MAP_ONLY(, _m_rmapped rmapped)) {
        _m_result _res = _c_MEMB(_insert_entry_)(self, rkey);
        if (_res.inserted) {
            *_i_keyref(_res.ref) = i_keyfrom(rkey);
            _i_MAP_ONLY( _res.ref->second = i_valfrom(rmapped); )
        }
        return _res;
    }
#endif // !i_no_emplace

STC_INLINE _m_raw _c_MEMB(_value_toraw)(const _m_value* val) {
    return _i_SET_ONLY( i_keytoraw(val) )
           _i_MAP_ONLY( c_literal(_m_raw){i_keytoraw((&val->first)), i_valtoraw((&val->second))} );
}

STC_INLINE void _c_MEMB(_value_drop)(const Self* self, _m_value* _val) {
    (void)self;
    i_keydrop(_i_keyref(_val));
    _i_MAP_ONLY( i_valdrop((&_val->second)); )
}

STC_INLINE Self _c_MEMB(_move)(Self *self) {
    Self m = *self;
    self->bucket_count = self->size = 0;
    self->meta = NULL; self->table = NULL;
    return m;
}

STC_INLINE void _c_MEMB(_take)(Self *self, Self unowned) {
    _c_MEMB(_drop)(self);
    *self = unowned;
}

STC_INLINE _m_result
_c_MEMB(_insert)(Self* self, _m_key _key _i_MAP_ONLY(, _m_mapped _mapped)) {
    _m_result _res = _c_MEMB(_insert_entry_)(self, i_keytoraw((&_key)));
    if (_res.inserted)
        { *_i_keyref(_res.ref) = _key; _i_MAP_ONLY( _res.ref->second = _mapped; )}
    else
        { i_keydrop((&_key)); _i_MAP_ONLY( i_valdrop((&_mapped)); )}
    return _res;
}

STC_INLINE _m_value* _c_MEMB(_push)(Self* self, _m_value _val) {
    _m_result _res = _c_MEMB(_insert_entry_)(self, i_keytoraw(_i_keyref(&_val)));
    if (_res.inserted)
        *_res.ref = _val;
    else
        _c_MEMB(_value_drop)(self, &_val);
    return _res.ref;
}

#ifdef _i_is_map
STC_INLINE _m_result _c_MEMB(_put)(Self* self, _m_keyraw rkey, _m_rmapped rmapped) {
    #ifdef i_no_emplace
        return _c_MEMB(_insert_or_assign)(self, rkey, rmapped);
    #else
        return _c_MEMB(_emplace_or_assign)(self, rkey, rmapped);
    #endif
}
#endif

STC_INLINE void _c_MEMB(_put_n)(Self* self, const _m_raw* raw, isize n) {
    while (n--)
        #if defined _i_is_set && defined i_no_emplace
            _c_MEMB(_insert)(self, *raw++);
        #elif defined _i_is_set
            _c_MEMB(_emplace)(self, *raw++);
        #else
            _c_MEMB(_put)(self, raw->first, raw->second), ++raw;
        #endif
}

#ifndef i_aux
STC_INLINE Self _c_MEMB(_init)(void)
    { Self map = {0}; return map; }

STC_INLINE Self _c_MEMB(_from_n)(const _m_raw* raw, isize n)
    { Self cx = {0}; _c_MEMB(_put_n)(&cx, raw, n); return cx; }
#endif

STC_API _m_iter _c_MEMB(_begin)(const Self* self);

STC_INLINE _m_iter _c_MEMB(_end)(const Self* self)
    { (void)self; return c_literal(_m_iter){0}; }

STC_INLINE void _c_MEMB(_next)(_m_iter* it) {
    while ((++it->ref, (++it->_mref)->dist == 0)) ;
    if (it->ref == it->_end) it->ref = NULL;
}

STC_INLINE _m_iter _c_MEMB(_advance)(_m_iter it, size_t n) {
    while (n-- && it.ref) _c_MEMB(_next)(&it);
    return it;
}

STC_INLINE _m_iter
_c_MEMB(_find)(const Self* self, _m_keyraw rkey) {
    _m_value* ref;
    if (self->size != 0 && (ref = _c_MEMB(_bucket_lookup_)(self, &rkey).ref) != NULL)
        return c_literal(_m_iter){ref,
                                  &self->table[self->bucket_count],
                                  &self->meta[ref - self->table]};
    return _c_MEMB(_end)(self);
}

STC_INLINE const _m_value*
_c_MEMB(_get)(const Self* self, _m_keyraw rkey) {
    return self->size ? _c_MEMB(_bucket_lookup_)(self, &rkey).ref : NULL;
}

STC_INLINE _m_value*
_c_MEMB(_get_mut)(Self* self, _m_keyraw rkey)
    { return (_m_value*)_c_MEMB(_get)(self, rkey); }

STC_INLINE int
_c_MEMB(_erase)(Self* self, _m_keyraw rkey) {
    _m_value* ref;
    if (self->size != 0 && (ref = _c_MEMB(_bucket_lookup_)(self, &rkey).ref) != NULL)
        { _c_MEMB(_erase_entry)(self, ref); return 1; }
    return 0;
}

STC_INLINE _m_iter
_c_MEMB(_erase_at)(Self* self, _m_iter it) {
    _c_MEMB(_erase_entry)(self, it.ref);
    if (it._mref->dist == 0)
        _c_MEMB(_next)(&it);
    return it;
}

STC_INLINE _Bool
_c_MEMB(_eq)(const Self* self, const Self* other) {
    if (_c_MEMB(_size)(self) != _c_MEMB(_size)(other)) return 0;
    for (_m_iter i = _c_MEMB(_begin)(self); i.ref; _c_MEMB(_next)(&i)) {
        const _m_keyraw _raw = i_keytoraw(_i_keyref(i.ref));
        if (!_c_MEMB(_contains)(other, _raw)) return 0;
    }
    return 1;
}

/* -------------------------- IMPLEMENTATION ------------------------- */
#if defined i_implement

STC_DEF _m_iter _c_MEMB(_begin)(const Self* self) {
    _m_iter it = {self->table, self->table, self->meta};
    if (it.ref == NULL) return it;
    it._end += self->bucket_count;
    while (it._mref->dist == 0)
        ++it.ref, ++it._mref;
    if (it.ref == it._end) it.ref = NULL;
    return it;
}

STC_DEF float _c_MEMB(_max_load_factor)(const Self* self) {
    (void)self; return (float)(i_max_load_factor);
}

STC_DEF isize _c_MEMB(_capacity)(const Self* map) {
    return (isize)((float)map->bucket_count * (i_max_load_factor));
}

#ifndef i_aux
STC_DEF Self _c_MEMB(_with_capacity)(const isize cap) {
    Self map = {0};
    _c_MEMB(_reserve)(&map, cap);
    return map;
}
#endif

static void _c_MEMB(_wipe_)(Self* self) {
    if (self->size == 0)
        return;
    _m_value* d = self->table, *_end = &d[self->bucket_count];
    struct hmap_meta* m = self->meta;
    for (; d != _end; ++d)
        if ((m++)->dist)
            _c_MEMB(_value_drop)(self, d);
}

STC_DEF void _c_MEMB(_drop)(const Self* cself) {
    Self* self = (Self*)cself;
    if (self->bucket_count > 0) {
        _c_MEMB(_wipe_)(self);
        i_free(self->meta, (self->bucket_count + 1)*c_sizeof *self->meta);
        i_free(self->table, self->bucket_count*c_sizeof *self->table);
    }
}

STC_DEF void _c_MEMB(_clear)(Self* self) {
    _c_MEMB(_wipe_)(self);
    self->size = 0;
    c_memset(self->meta, 0, c_sizeof(struct hmap_meta)*self->bucket_count);
}

#ifdef _i_is_map
    STC_DEF _m_result
    _c_MEMB(_insert_or_assign)(Self* self, _m_key _key, _m_mapped _mapped) {
        _m_result _res = _c_MEMB(_insert_entry_)(self, i_keytoraw((&_key)));
        _m_mapped* _mp = _res.ref ? &_res.ref->second : &_mapped;
        if (_res.inserted)
            _res.ref->first = _key;
        else
            { i_keydrop((&_key)); i_valdrop(_mp); }
        *_mp = _mapped;
        return _res;
    }

    #if !defined i_no_emplace
    STC_DEF _m_result
    _c_MEMB(_emplace_or_assign)(Self* self, _m_keyraw rkey, _m_rmapped rmapped) {
        _m_result _res = _c_MEMB(_insert_entry_)(self, rkey);
        if (_res.inserted)
            _res.ref->first = i_keyfrom(rkey);
        else {
            if (_res.ref == NULL) return _res;
            i_valdrop((&_res.ref->second));
        }
        _res.ref->second = i_valfrom(rmapped);
        return _res;
    }
    #endif // !i_no_emplace
#endif // _i_is_map

static _m_result
_c_MEMB(_bucket_lookup_)(const Self* self, const _m_keyraw* rkeyptr) {
    const size_t _hash = i_hash(rkeyptr);
    const size_t _idxmask = (size_t)self->bucket_count - 1;
    _m_result _res = {.idx=_hash & _idxmask, .hashx=(uint8_t)((_hash >> 24) & _hashmask), .dist=1};

    while (_res.dist <= self->meta[_res.idx].dist) {
        if (self->meta[_res.idx].hashx == _res.hashx) {
            const _m_keyraw _raw = i_keytoraw(_i_keyref(&self->table[_res.idx]));
            if (i_eq((&_raw), rkeyptr)) {
                _res.ref = &self->table[_res.idx];
                break;
            }
        }
        _res.idx = (_res.idx + 1) & _idxmask;
        ++_res.dist;
    }
    return _res;
}

static _m_result
_c_MEMB(_bucket_insert_)(const Self* self, const _m_keyraw* rkeyptr) {
    _m_result res = _c_MEMB(_bucket_lookup_)(self, rkeyptr);
    if (res.ref) // bucket exists
        return res;
    res.ref = &self->table[res.idx];
    res.inserted = 1;
    struct hmap_meta mnew = {.hashx=(uint16_t)(res.hashx & _hashmask),
                             .dist=(uint16_t)(res.dist & _distmask)};
    struct hmap_meta mcur = self->meta[res.idx];
    self->meta[res.idx] = mnew;

    if (mcur.dist != 0) { // collision, reorder buckets
        size_t mask = (size_t)self->bucket_count - 1;
        _m_value dcur = *res.ref;
        for (;;) {
            res.idx = (res.idx + 1) & mask;
            ++mcur.dist;
            if (self->meta[res.idx].dist == 0)
                break;
            if (self->meta[res.idx].dist < mcur.dist) {
                c_swap(&mcur, &self->meta[res.idx]);
                c_swap(&dcur, &self->table[res.idx]);
            }
        }
        self->meta[res.idx] = mcur;
        self->table[res.idx] = dcur;
    }
    return res;
}


#if !defined i_no_clone
    STC_DEF Self
    _c_MEMB(_clone)(Self map) {
        if (map.bucket_count == 0)
            return c_literal(Self){0};
        Self out = map, *self = &out; // _i_malloc may refer self via i_aux
        const isize _mbytes = (map.bucket_count + 1)*c_sizeof *map.meta;
        out.table = (_m_value *)i_malloc(map.bucket_count*c_sizeof *out.table);
        out.meta = (struct hmap_meta *)i_malloc(_mbytes);

        if (out.table && out.meta) {
            c_memcpy(out.meta, map.meta, _mbytes);
            for (isize i = 0; i < map.bucket_count; ++i)
                if (map.meta[i].dist)
                    out.table[i] = _c_MEMB(_value_clone)(self, map.table[i]);
            return out;
        } else {
            if (out.meta) i_free(out.meta, _mbytes);
            if (out.table) i_free(out.table, map.bucket_count*c_sizeof *out.table);
            return c_literal(Self){0};
        }
    }
#endif

STC_DEF _Bool
_c_MEMB(_reserve)(Self* _self, const isize _newcap) {
    isize _newbucks = (isize)((float)_newcap / (i_max_load_factor)) + 4;
    _newbucks = c_next_pow2(_newbucks);

    if (_newcap < _self->size || _newbucks == _self->bucket_count)
        return 1;
    Self map = *_self, *self = &map; (void)self;
    map.table = _i_malloc(_m_value, _newbucks);
    map.meta = _i_calloc(struct hmap_meta, _newbucks + 1);
    map.bucket_count = _newbucks;

    _Bool ok = map.table && map.meta;
    if (ok) {  // Rehash:
        map.meta[_newbucks].dist = _distmask; // end-mark for iter
        const _m_value* d = _self->table;
        const struct hmap_meta* m = _self->meta;

        for (isize i = 0; i < _self->bucket_count; ++i, ++d) if (m[i].dist != 0) {
            _m_keyraw r = i_keytoraw(_i_keyref(d));
            *_c_MEMB(_bucket_insert_)(&map, &r).ref = *d; // move element
        }
        c_swap(_self, &map);
    }
    i_free(map.meta, (map.bucket_count + (int)(map.meta != NULL))*c_sizeof *map.meta);
    i_free(map.table, map.bucket_count*c_sizeof *map.table);
    return ok;
}

STC_DEF void
_c_MEMB(_erase_entry)(Self* self, _m_value* _val) {
    _m_value* d = self->table;
    struct hmap_meta *m = self->meta;
    size_t i = (size_t)(_val - d), j = i;
    size_t mask = (size_t)self->bucket_count - 1;

    _c_MEMB(_value_drop)(self, _val);
    for (;;) {
        j = (j + 1) & mask;
        if (m[j].dist < 2) // 0 => empty, 1 => PSL 0
            break;
        d[i] = d[j];
        m[i] = m[j];
        --m[i].dist;
        i = j;
    }
    m[i].dist = 0;
    --self->size;
}

#endif // i_implement
#undef i_max_load_factor
#undef _i_is_set
#undef _i_is_map
#undef _i_is_hash
#undef _i_keyref
#undef _i_MAP_ONLY
#undef _i_SET_ONLY
// ### BEGIN_FILE_INCLUDE: linkage2.h

#undef i_allocator
#undef i_malloc
#undef i_calloc
#undef i_realloc
#undef i_free
#undef i_aux
#undef _i_aux_def

#undef i_static
#undef i_header
#undef i_implement
#undef i_import

#if defined __clang__ && !defined __cplusplus
  #pragma clang diagnostic pop
#elif defined __GNUC__ && !defined __cplusplus
  #pragma GCC diagnostic pop
#endif
// ### END_FILE_INCLUDE: linkage2.h
// ### BEGIN_FILE_INCLUDE: template2.h
// IWYU pragma: private
#undef T            // alias for i_type
#undef i_type
#undef i_class
#undef i_tag
#undef i_opt
#undef i_capacity

#undef i_key
#undef i_keypro     // Replaces next two
#undef i_key_str    // [deprecated]
#undef i_key_arcbox // [deprecated]
#undef i_keyclass
#undef i_cmpclass   // define i_keyraw, and bind i_cmp, i_eq, i_hash "class members"
#undef i_rawclass   // [deprecated] for i_cmpclass
#undef i_keyclone
#undef i_keydrop
#undef i_keyraw
#undef i_keyfrom
#undef i_keytoraw
#undef i_cmp
#undef i_less
#undef i_eq
#undef i_hash

#undef i_val
#undef i_valpro     // Replaces next two
#undef i_val_str    // [deprecated]
#undef i_val_arcbox // [deprecated]
#undef i_valclass
#undef i_valclone
#undef i_valdrop
#undef i_valraw
#undef i_valfrom
#undef i_valtoraw

#undef i_use_cmp
#undef i_use_eq
#undef i_no_hash
#undef i_no_clone
#undef i_no_emplace
#undef i_declared

#undef _i_has_cmp
#undef _i_has_eq
#undef _i_prefix
#undef _i_template
#undef Self
// ### END_FILE_INCLUDE: template2.h
// ### END_FILE_INCLUDE: hmap.h

