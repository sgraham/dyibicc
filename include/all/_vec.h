// BEGIN regen_container_headers.py
#ifndef __dyibicc_internal_include__
#error Can only be included by the compiler, or confusing errors will result!
#endif
#undef __attribute__
typedef unsigned char uint8_t;
typedef unsigned long long uint64_t;
typedef unsigned long long size_t;
typedef long long int64_t;
typedef long long intptr_t;
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
// ### BEGIN_FILE_INCLUDE: cvec.h

// ### BEGIN_FILE_INCLUDE: linkage.h
#undef STC_API
#undef STC_DEF

#if !defined i_static  && !defined STC_STATIC  && (defined i_header || defined STC_HEADER  || \
                                                   defined i_implement || defined STC_IMPLEMENT)
  #define STC_API extern
  #define STC_DEF
#else
  #define i_static
  #if defined __GNUC__ || defined __clang__
    #define STC_API static __attribute__((unused))
  #else
    #define STC_API static
  #endif
  #define STC_DEF static
#endif
#if defined STC_IMPLEMENT || defined i_import
  #define i_implement
#endif

#if defined STC_ALLOCATOR && !defined i_allocator
  #define i_allocator STC_ALLOCATOR
#elif !defined i_allocator
  #define i_allocator c
#endif
#ifndef i_malloc
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
  #pragma clang diagnostic warning "-Wdouble-promotion"
  #pragma clang diagnostic warning "-Wwrite-strings"
  // ignored
  #pragma clang diagnostic ignored "-Wmissing-field-initializers"
#elif defined __GNUC__ && !defined __cplusplus
  #pragma GCC diagnostic push
  #pragma GCC diagnostic warning "-Wall"
  #pragma GCC diagnostic warning "-Wextra"
  #pragma GCC diagnostic warning "-Wpedantic"
  #pragma GCC diagnostic warning "-Wconversion"
  #pragma GCC diagnostic warning "-Wdouble-promotion"
  #pragma GCC diagnostic warning "-Wwrite-strings"
  // ignored
  #pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#endif
// ### END_FILE_INCLUDE: linkage.h

#ifndef CVEC_H_INCLUDED
// ### BEGIN_FILE_INCLUDE: ccommon.h
#ifndef CCOMMON_H_INCLUDED
#define CCOMMON_H_INCLUDED

#ifdef _MSC_VER
    #pragma warning(disable: 4116 4996) // unnamed type definition in parentheses
#endif
// EXCLUDED BY regen_container_headers.py #include <inttypes.h>
// EXCLUDED BY regen_container_headers.py #include <stddef.h>
// EXCLUDED BY regen_container_headers.py #include <stdbool.h>
// EXCLUDED BY regen_container_headers.py #include <string.h>
// EXCLUDED BY regen_container_headers.py #include <assert.h>

typedef long long _llong;
#define c_NPOS INTPTR_MAX
#define c_ZI PRIiPTR
#define c_ZU PRIuPTR

#if defined __GNUC__ // includes __clang__
    #define STC_INLINE static inline __attribute((unused))
#else
    #define STC_INLINE static inline
#endif

/* Macro overloading feature support based on: https://rextester.com/ONP80107 */
#define c_MACRO_OVERLOAD(name, ...) \
    c_JOIN(c_JOIN0(name,_),c_NUMARGS(__VA_ARGS__))(__VA_ARGS__)
#define c_JOIN0(a, b) a ## b
#define c_JOIN(a, b) c_JOIN0(a, b)
#define c_EXPAND(...) __VA_ARGS__
#define c_NUMARGS(...) _c_APPLY_ARG_N((__VA_ARGS__, _c_RSEQ_N))
#define _c_APPLY_ARG_N(args) c_EXPAND(_c_ARG_N args)
#define _c_RSEQ_N 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0
#define _c_ARG_N(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, \
                 _14, _15, _16, N, ...) N

#ifndef __cplusplus 
    #define _i_alloc(T)         ((T*)i_malloc(c_sizeof(T)))
    #define _i_new(T, ...)      ((T*)memcpy(_i_alloc(T), ((T[]){__VA_ARGS__}), sizeof(T)))
    #define c_new(T, ...)       ((T*)memcpy(malloc(sizeof(T)), ((T[]){__VA_ARGS__}), sizeof(T)))
    #define c_LITERAL(T)        (T)
#else
// EXCLUDED BY regen_container_headers.py     #include <new>
    #define _i_alloc(T)         static_cast<T*>(i_malloc(c_sizeof(T)))
    #define _i_new(T, ...)      new (_i_alloc(T)) T(__VA_ARGS__)
    #define c_new(T, ...)       new (malloc(sizeof(T))) T(__VA_ARGS__)
    #define c_LITERAL(T)        T
#endif
#define c_new_n(T, n)           ((T*)malloc(sizeof(T)*c_i2u_size(n)))
#define c_malloc(sz)            malloc(c_i2u_size(sz))
#define c_calloc(n, sz)         calloc(c_i2u_size(n), c_i2u_size(sz))
#define c_realloc(p, old_sz, sz) realloc(p, c_i2u_size(1 ? (sz) : (old_sz)))
#define c_free(p, sz)           do { (void)(sz); free(p); } while(0)
#define c_delete(T, ptr)        do { T *_tp = ptr; T##_drop(_tp); free(_tp); } while (0)

#define c_static_assert(expr)   (1 ? 0 : (int)sizeof(int[(expr) ? 1 : -1]))
#if defined STC_NDEBUG || defined NDEBUG
    #define c_assert(expr)      ((void)0)
#else
#ifndef STC_ASSERT
#define STC_ASSERT(expr)
#endif
    #define c_assert(expr)      STC_ASSERT(expr)
#endif
#define c_container_of(p, C, m) ((C*)((char*)(1 ? (p) : &((C*)0)->m) - offsetof(C, m)))
#define c_const_cast(Tp, p)     ((Tp)(1 ? (p) : (Tp)0))
#define c_safe_cast(T, F, x)    ((T)(1 ? (x) : (F){0}))
#define c_swap(T, xp, yp)       do { T *_xp = xp, *_yp = yp, \
                                    _tv = *_xp; *_xp = *_yp; *_yp = _tv; } while (0)
// use with gcc -Wconversion
#define c_sizeof                (intptr_t)sizeof
#define c_strlen(s)             (intptr_t)strlen(s)
#define c_strncmp(a, b, ilen)   strncmp(a, b, c_i2u_size(ilen))
#define c_memcpy(d, s, ilen)    memcpy(d, s, c_i2u_size(ilen))
#define c_memmove(d, s, ilen)   memmove(d, s, c_i2u_size(ilen))
#define c_memset(d, val, ilen)  memset(d, val, c_i2u_size(ilen))
#define c_memcmp(a, b, ilen)    memcmp(a, b, c_i2u_size(ilen))
#define c_u2i_size(u)           (intptr_t)(1 ? (u) : (size_t)1)
#define c_i2u_size(i)           (size_t)(1 ? (i) : -1)
#define c_less_unsigned(a, b)   ((size_t)(a) < (size_t)(b))

// x and y are i_keyraw* type, defaults to i_key*:
#define c_default_cmp(x, y)     (c_default_less(y, x) - c_default_less(x, y))
#define c_default_less(x, y)    (*(x) < *(y))
#define c_default_eq(x, y)      (*(x) == *(y))
#define c_memcmp_eq(x, y)       (memcmp(x, y, sizeof *(x)) == 0)
#define c_default_hash          stc_hash_1

#define c_default_clone(v)      (v)
#define c_default_toraw(vp)     (*(vp))
#define c_default_drop(vp)      ((void) (vp))

/* Function macros and others */

#define c_litstrlen(literal) (c_sizeof("" literal) - 1)
#define c_arraylen(a) (intptr_t)(sizeof(a)/sizeof 0[a])

// Non-owning c-string "class"
typedef const char* ccharptr;
#define ccharptr_cmp(xp, yp) strcmp(*(xp), *(yp))
#define ccharptr_hash(p) stc_strhash(*(p))
#define ccharptr_clone(s) (s)
#define ccharptr_drop(p) ((void)p)

#define c_sv(...) c_MACRO_OVERLOAD(c_sv, __VA_ARGS__)
#define c_sv_1(literal) c_sv_2(literal, c_litstrlen(literal))
#define c_sv_2(str, n) (c_LITERAL(csview){str, n})
#define c_SV(sv) (int)(sv).size, (sv).buf // printf("%.*s\n", c_SV(sv));

#define c_rs(literal) c_rs_2(literal, c_litstrlen(literal))
#define c_rs_2(str, n) (c_LITERAL(crawstr){str, n})

#define c_ROTL(x, k) (x << (k) | x >> (8*sizeof(x) - (k)))

#define stc_hash(...) c_MACRO_OVERLOAD(stc_hash, __VA_ARGS__)
#define stc_hash_1(x) stc_hash_2(x, c_sizeof(*(x)))

STC_INLINE uint64_t stc_hash_2(const void* key, intptr_t len) {
    uint32_t u4; uint64_t u8;
    switch (len) {
        case 8: memcpy(&u8, key, 8); return u8*0xc6a4a7935bd1e99d;
        case 4: memcpy(&u4, key, 4); return u4*0xc6a4a7935bd1e99d;
        case 0: return 1;
    }
    const uint8_t *x = (const uint8_t*)key;
    uint64_t h = (uint64_t)*x << 7, n = (uint64_t)len >> 3;
    len &= 7;
    while (n--) {
        memcpy(&u8, x, 8), x += 8;
        h = (h ^ u8)*0xc6a4a7935bd1e99d;
    }
    while (len--) h = (h ^ *x++)*0x100000001b3;
    return h ^ c_ROTL(h, 26);
}

STC_INLINE uint64_t stc_strhash(const char *str)
    { return stc_hash_2(str, c_strlen(str)); }

STC_INLINE uint64_t _stc_hash_mix(uint64_t h[], int n) { // n > 0
    for (int i = 1; i < n; ++i) h[0] ^= h[0] + h[i]; // non-commutative!
    return h[0];
}

STC_INLINE char* stc_strnstrn(const char *str, intptr_t slen, 
                              const char *needle, intptr_t nlen) {
    if (!nlen) return (char *)str;
    if (nlen > slen) return NULL;
    slen -= nlen;
    do {
        if (*str == *needle && !c_memcmp(str, needle, nlen))
            return (char *)str;
        ++str;
    } while (slen--);
    return NULL;
}

STC_INLINE intptr_t stc_nextpow2(intptr_t n) {
    n--;
    n |= n >> 1, n |= n >> 2;
    n |= n >> 4, n |= n >> 8;
    n |= n >> 16;
    #if INTPTR_MAX == INT64_MAX
    n |= n >> 32;
    #endif
    return n + 1;
}
/* Control block macros */

#define c_foreach(...) c_MACRO_OVERLOAD(c_foreach, __VA_ARGS__)
#define c_foreach_3(it, C, cnt) \
    for (C##_iter it = C##_begin(&cnt); it.ref; C##_next(&it))
#define c_foreach_4(it, C, start, finish) \
    for (C##_iter it = (start), *_endref = c_safe_cast(C##_iter*, C##_value*, (finish).ref) \
         ; it.ref != (C##_value*)_endref; C##_next(&it))

#define c_forpair(key, val, C, cnt) /* structured binding */ \
    for (struct {C##_iter iter; const C##_key* key; C##_mapped* val;} _ = {.iter=C##_begin(&cnt)} \
         ; _.iter.ref && (_.key = &_.iter.ref->first, _.val = &_.iter.ref->second) \
         ; C##_next(&_.iter))

#define c_forindexed(it, C, cnt) \
    for (struct {C##_iter iter; C##_value* ref; intptr_t index;} it = {.iter=C##_begin(&cnt)} \
         ; (it.ref = it.iter.ref) ; C##_next(&it.iter), ++it.index)

#define c_foriter(existing_iter, C, cnt) \
    for (existing_iter = C##_begin(&cnt); (existing_iter).ref; C##_next(&existing_iter))

#define c_forrange(...) c_MACRO_OVERLOAD(c_forrange, __VA_ARGS__)
#define c_forrange_1(stop) c_forrange_3(_i, 0, stop)
#define c_forrange_2(i, stop) c_forrange_3(i, 0, stop)
#define c_forrange_3(i, start, stop) \
    for (_llong i=start, _end=stop; i < _end; ++i)
#define c_forrange_4(i, start, stop, step) \
    for (_llong i=start, _inc=step, _end=(_llong)(stop) - (_inc > 0) \
         ; (_inc > 0) ^ (i > _end); i += _inc)

#ifndef __cplusplus
    #define c_init(C, ...) \
        C##_from_n((C##_raw[])__VA_ARGS__, c_sizeof((C##_raw[])__VA_ARGS__)/c_sizeof(C##_raw))
    #define c_forlist(it, T, ...) \
        for (struct {T* ref; int size, index;} \
             it = {.ref=(T[])__VA_ARGS__, .size=(int)(sizeof((T[])__VA_ARGS__)/sizeof(T))} \
             ; it.index < it.size; ++it.ref, ++it.index)
    #define stc_hash_mix(...) \
        _stc_hash_mix((uint64_t[]){__VA_ARGS__}, c_NUMARGS(__VA_ARGS__))
#else
// EXCLUDED BY regen_container_headers.py     #include <initializer_list>
// EXCLUDED BY regen_container_headers.py     #include <array>
    template <class C, class T>
    inline C _from_n(C (*func)(const T[], intptr_t), std::initializer_list<T> il)
        { return func(&*il.begin(), il.size()); }
    #define c_init(C, ...) _from_n<C,C##_raw>(C##_from_n, __VA_ARGS__)
    #define c_forlist(it, T, ...) \
        for (struct {std::initializer_list<T> _il; std::initializer_list<T>::iterator ref; size_t size, index;} \
             it = {._il=__VA_ARGS__, .ref=it._il.begin(), .size=it._il.size()} \
             ; it.index < it.size; ++it.ref, ++it.index)
    #define stc_hash_mix(...) \
        _stc_hash_mix(std::array<uint64_t, c_NUMARGS(__VA_ARGS__)>{__VA_ARGS__}.data(), c_NUMARGS(__VA_ARGS__))
#endif

#define c_defer(...) \
    for (int _i = 1; _i; _i = 0, __VA_ARGS__)

#define c_with(...) c_MACRO_OVERLOAD(c_with, __VA_ARGS__)
#define c_with_2(declvar, drop) \
    for (declvar, *_i, **_ip = &_i; _ip; _ip = 0, drop)
#define c_with_3(declvar, pred, drop) \
    for (declvar, *_i, **_ip = &_i; _ip && (pred); _ip = 0, drop)

#define c_scope(...) c_MACRO_OVERLOAD(c_scope, __VA_ARGS__)
#define c_scope_2(init, drop) \
    for (int _i = (init, 1); _i; _i = 0, drop)
#define c_scope_3(init, pred, drop) \
    for (int _i = (init, 1); _i && (pred); _i = 0, drop)

#define c_drop(C, ...) \
    do { c_forlist (_i, C*, {__VA_ARGS__}) C##_drop(*_i.ref); } while(0)

#if defined(__SIZEOF_INT128__)
    #define c_umul128(a, b, lo, hi) \
        do { __uint128_t _z = (__uint128_t)(a)*(b); \
             *(lo) = (uint64_t)_z, *(hi) = (uint64_t)(_z >> 64U); } while(0)
#elif defined(_MSC_VER) && defined(_WIN64)
// EXCLUDED BY regen_container_headers.py     #include <intrin.h>
    #define c_umul128(a, b, lo, hi) ((void)(*(lo) = _umul128(a, b, hi)))
#elif defined(__x86_64__)
    #define c_umul128(a, b, lo, hi) \
        asm("mulq %3" : "=a"(*(lo)), "=d"(*(hi)) : "a"(a), "rm"(b))
#endif

#endif // CCOMMON_H_INCLUDED
// ### END_FILE_INCLUDE: ccommon.h
// ### BEGIN_FILE_INCLUDE: forward.h
#ifndef STC_FORWARD_H_INCLUDED
#define STC_FORWARD_H_INCLUDED

// EXCLUDED BY regen_container_headers.py #include <stdint.h>
// EXCLUDED BY regen_container_headers.py #include <stddef.h>

#define forward_carc(C, VAL) _c_carc_types(C, VAL)
#define forward_cbox(C, VAL) _c_cbox_types(C, VAL)
#define forward_cdeq(C, VAL) _c_cdeq_types(C, VAL)
#define forward_clist(C, VAL) _c_clist_types(C, VAL)
#define forward_cmap(C, KEY, VAL) _c_chash_types(C, KEY, VAL, c_true, c_false)
#define forward_cset(C, KEY) _c_chash_types(C, cset, KEY, KEY, c_false, c_true)
#define forward_csmap(C, KEY, VAL) _c_aatree_types(C, KEY, VAL, c_true, c_false)
#define forward_csset(C, KEY) _c_aatree_types(C, KEY, KEY, c_false, c_true)
#define forward_cstack(C, VAL) _c_cstack_types(C, VAL)
#define forward_cpque(C, VAL) _c_cpque_types(C, VAL)
#define forward_cqueue(C, VAL) _c_cdeq_types(C, VAL)
#define forward_cvec(C, VAL) _c_cvec_types(C, VAL)
// alternative names (include/stx):
#define forward_arc forward_carc
#define forward_box forward_cbox
#define forward_deq forward_cdeq
#define forward_list forward_clist
#define forward_hmap forward_cmap
#define forward_hset forward_cset
#define forward_smap forward_csmap
#define forward_sset forward_csset
#define forward_stack forward_cstack
#define forward_pque forward_cpque
#define forward_queue forward_cqueue
#define forward_vec forward_cvec

// csview : non-null terminated string view
typedef const char csview_value;
typedef struct csview {
    csview_value* buf;
    intptr_t size;
} csview;

typedef union {
    csview_value* ref;
    csview chr;
    struct { csview chr; csview_value* end; } u8;
} csview_iter;


// crawstr : null-terminated string view
typedef csview_value crawstr_value;
typedef struct crawstr {
    crawstr_value* str;
    intptr_t size;
} crawstr;

typedef union {
    crawstr_value* ref;
    csview chr;
} crawstr_iter;


// cstr : null-terminated owning string (short string optimized - sso)
typedef char cstr_value;
typedef struct { cstr_value* data; intptr_t size, cap; } cstr_buf;
typedef union cstr {
    struct { cstr_value data[ sizeof(cstr_buf) ]; } sml;
    struct { cstr_value* data; size_t size, ncap; } lon;
} cstr;

typedef union {
    cstr_value* ref;
    csview chr; // utf8 character/codepoint
} cstr_iter;


#if defined __GNUC__ || defined __clang__ || defined _MSC_VER
    typedef long catomic_long;
#else
    typedef _Atomic(long) catomic_long;
#endif

#define c_true(...) __VA_ARGS__
#define c_false(...)

#define _c_carc_types(SELF, VAL) \
    typedef VAL SELF##_value; \
typedef struct __attribute__((methodcall(SELF##_))) SELF { \
        SELF##_value* get; \
        catomic_long* use_count; \
    } SELF

#define _c_cbox_types(SELF, VAL) \
    typedef VAL SELF##_value; \
typedef struct __attribute__((methodcall(SELF##_))) SELF { \
        SELF##_value* get; \
    } SELF

#define _c_cdeq_types(SELF, VAL) \
    typedef VAL SELF##_value; \
\
typedef struct __attribute__((methodcall(SELF##_))) SELF { \
        SELF##_value *cbuf; \
        intptr_t start, end, capmask; \
    } SELF; \
\
    typedef struct { \
        SELF##_value *ref; \
        intptr_t pos; \
        const SELF* _s; \
    } SELF##_iter

#define _c_clist_types(SELF, VAL) \
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
    } SELF

#define _c_chash_types(SELF, KEY, VAL, MAP_ONLY, SET_ONLY) \
    typedef KEY SELF##_key; \
    typedef VAL SELF##_mapped; \
\
    typedef SET_ONLY( SELF##_key ) \
            MAP_ONLY( struct SELF##_value ) \
    SELF##_value; \
\
    typedef struct { \
        SELF##_value *ref; \
        _Bool inserted; \
        uint8_t hashx; \
    } SELF##_result; \
\
    typedef struct { \
        SELF##_value *ref, *_end; \
        struct chash_slot *_sref; \
    } SELF##_iter; \
\
typedef struct __attribute__((methodcall(SELF##_))) SELF { \
        SELF##_value* table; \
        struct chash_slot* slot; \
        intptr_t size, bucket_count; \
    } SELF

#define _c_aatree_types(SELF, KEY, VAL, MAP_ONLY, SET_ONLY) \
    typedef KEY SELF##_key; \
    typedef VAL SELF##_mapped; \
    typedef struct SELF##_node SELF##_node; \
\
    typedef SET_ONLY( SELF##_key ) \
            MAP_ONLY( struct SELF##_value ) \
    SELF##_value; \
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
        int32_t root, disp, head, size, cap; \
    } SELF

#define _c_cstack_fixed(SELF, VAL, CAP) \
    typedef VAL SELF##_value; \
    typedef struct { SELF##_value *ref, *end; } SELF##_iter; \
typedef struct __attribute__((methodcall(SELF##_))) SELF { SELF##_value data[CAP]; intptr_t _len; } SELF

#define _c_cstack_types(SELF, VAL) \
    typedef VAL SELF##_value; \
    typedef struct { SELF##_value *ref, *end; } SELF##_iter; \
typedef struct __attribute__((methodcall(SELF##_))) SELF { SELF##_value* data; intptr_t _len, _cap; } SELF

#define _c_cvec_types(SELF, VAL) \
    typedef VAL SELF##_value; \
    typedef struct { SELF##_value *ref, *end; } SELF##_iter; \
typedef struct __attribute__((methodcall(SELF##_))) SELF { SELF##_value *data; intptr_t _len, _cap; } SELF

#define _c_cpque_types(SELF, VAL) \
    typedef VAL SELF##_value; \
typedef struct __attribute__((methodcall(SELF##_))) SELF { SELF##_value* data; intptr_t _len, _cap; } SELF

#endif // STC_FORWARD_H_INCLUDED
// ### END_FILE_INCLUDE: forward.h
// EXCLUDED BY regen_container_headers.py #include <stdlib.h>
// EXCLUDED BY regen_container_headers.py #include <string.h>

#define _it2_ptr(it1, it2) (it1.ref && !it2.ref ? it2.end : it2.ref)
#define _it_ptr(it) (it.ref ? it.ref : it.end)
#endif // CVEC_H_INCLUDED

#ifndef _i_prefix
  #define _i_prefix cvec_
#endif
// ### BEGIN_FILE_INCLUDE: template.h
#ifndef _i_template
#define _i_template

#ifndef STC_TEMPLATE_H_INCLUDED
#define STC_TEMPLATE_H_INCLUDED
  #define _c_MEMB(name) c_JOIN(i_type, name)
  #define _c_DEFTYPES(macro, SELF, ...) c_EXPAND(macro(SELF, __VA_ARGS__))
  #define _m_value _c_MEMB(_value)
  #define _m_key _c_MEMB(_key)
  #define _m_mapped _c_MEMB(_mapped)
  #define _m_rmapped _c_MEMB(_rmapped)
  #define _m_raw _c_MEMB(_raw)
  #define _m_keyraw _c_MEMB(_keyraw)
  #define _m_iter _c_MEMB(_iter)
  #define _m_result _c_MEMB(_result)
  #define _m_node _c_MEMB(_node)
#endif

#ifndef i_type
  #define i_type c_JOIN(_i_prefix, i_tag)
#endif

#ifdef i_keyclass // [deprecated]
  #define i_key_class i_keyclass
#endif
#ifdef i_valclass // [deprecated]
  #define i_val_class i_valclass
#endif
#ifdef i_rawclass // [deprecated]
  #define i_raw_class i_rawclass
#endif
#ifdef i_keyboxed // [deprecated]
  #define i_key_arcbox i_keyboxed
#endif
#ifdef i_valboxed // [deprecated]
  #define i_val_arcbox i_valboxed
#endif

#if !(defined i_key || defined i_key_str || defined i_key_ssv || \
      defined i_key_class || defined i_key_arcbox)
  #if defined _i_ismap
    #error "i_key* must be defined for maps"
  #endif

  #if defined i_val_str
    #define i_key_str i_val_str
  #endif
  #if defined i_val_ssv
    #define i_key_ssv i_val_ssv
  #endif  
  #if defined i_val_arcbox
    #define i_key_arcbox i_val_arcbox
  #endif
  #if defined i_val_class
    #define i_key_class i_val_class
  #endif
  #if defined i_val
    #define i_key i_val
  #endif
  #if defined i_valraw
    #define i_keyraw i_valraw
  #endif
  #if defined i_valclone
    #define i_keyclone i_valclone
  #endif
  #if defined i_valfrom
    #define i_keyfrom i_valfrom
  #endif
  #if defined i_valto
    #define i_keyto i_valto
  #endif
  #if defined i_valdrop
    #define i_keydrop i_valdrop
  #endif
#endif

#define c_option(flag)          ((i_opt) & (flag))
#define c_is_forward            (1<<0)
#define c_no_atomic             (1<<1)
#define c_no_clone              (1<<2)
#define c_no_emplace            (1<<3)
#define c_no_hash               (1<<4)
#define c_use_cmp               (1<<5)
#define c_more                  (1<<6)

#if c_option(c_is_forward)
  #define i_is_forward
#endif
#if c_option(c_no_hash)
  #define i_no_hash
#endif
#if c_option(c_no_emplace)
  #define i_no_emplace
#endif
#if c_option(c_use_cmp) || defined i_cmp || defined i_less || \
                           defined _i_ismap || defined _i_isset || defined _i_ispque
  #define i_use_cmp
#endif
#if c_option(c_no_clone) || defined _i_carc
  #define i_no_clone
#endif
#if c_option(c_more)
  #define i_more
#endif

#if defined i_key_str
  #define i_key_class cstr
  #define i_raw_class ccharptr
  #ifndef i_tag
    #define i_tag str
  #endif
#elif defined i_key_ssv
  #define i_key_class cstr
  #define i_raw_class csview
  #define i_keyfrom cstr_from_sv
  #define i_keyto cstr_sv
  #ifndef i_tag
    #define i_tag ssv
  #endif
#elif defined i_key_arcbox
  #define i_key_class i_key_arcbox
  #define i_raw_class c_JOIN(i_key_arcbox, _raw)
  #if defined i_use_cmp
    #define i_eq c_JOIN(i_key_arcbox, _raw_eq)
  #endif
#endif

#if defined i_raw_class
  #define i_keyraw i_raw_class
#elif defined i_key_class && !defined i_keyraw
  #define i_raw_class i_key
#endif

#if defined i_key_class
  #define i_key i_key_class
  #ifndef i_keyclone
    #define i_keyclone c_JOIN(i_key, _clone)
  #endif
  #ifndef i_keydrop
    #define i_keydrop c_JOIN(i_key, _drop)
  #endif
  #if !defined i_keyfrom && defined i_keyraw
    #define i_keyfrom c_JOIN(i_key, _from)
  #endif
  #if !defined i_keyto && defined i_keyraw
    #define i_keyto c_JOIN(i_key, _toraw)
  #endif
#endif

#if defined i_raw_class
  #if !(defined i_cmp || defined i_less) && defined i_use_cmp
    #define i_cmp c_JOIN(i_keyraw, _cmp)
  #endif
  #if !(defined i_hash || defined i_no_hash)
    #define i_hash c_JOIN(i_keyraw, _hash)
  #endif
#endif

#if defined i_cmp || defined i_less || defined i_use_cmp
  #define _i_has_cmp
#endif
#if defined i_eq || defined i_use_cmp
  #define _i_has_eq
#endif
#if !(defined i_hash || defined i_no_hash)
  #define i_hash c_default_hash
#endif

#if !defined i_key
  #error "No i_key or i_val defined"
#elif defined i_keyraw ^ defined i_keyto
  #error "Both i_keyraw/i_valraw and i_keyto/i_valto must be defined, if any"
#elif !defined i_no_clone && (defined i_keyclone ^ defined i_keydrop)
  #error "Both i_keyclone/i_valclone and i_keydrop/i_valdrop must be defined, if any (unless i_no_clone defined)."
#elif defined i_from || defined i_drop
  #error "i_from / i_drop not supported. Define i_keyfrom/i_valfrom and/or i_keydrop/i_valdrop instead"
#elif defined i_keyraw && defined _i_ishash && !(defined i_hash && (defined _i_has_cmp || defined i_eq))
  #error "For cmap/cset, both i_hash and i_eq (or i_less or i_cmp) must be defined when i_keyraw is defined."
#elif defined i_keyraw && defined i_use_cmp && !defined _i_has_cmp
  #error "For csmap/csset/cpque, i_cmp or i_less must be defined when i_keyraw is defined."
#endif

// i_eq, i_less, i_cmp
#if !defined i_eq && defined i_cmp
  #define i_eq(x, y) !(i_cmp(x, y))
#elif !defined i_eq && !defined i_keyraw
  #define i_eq(x, y) *x == *y // for integral types, else define i_eq or i_cmp yourself
#endif
#if !defined i_less && defined i_cmp
  #define i_less(x, y) (i_cmp(x, y)) < 0
#elif !defined i_less && !defined i_keyraw
  #define i_less(x, y) *x < *y // for integral types, else define i_less or i_cmp yourself
#endif
#if !defined i_cmp && defined i_less
  #define i_cmp(x, y) (i_less(y, x)) - (i_less(x, y))
#endif

#ifndef i_tag
  #define i_tag i_key
#endif
#ifndef i_keyraw
  #define i_keyraw i_key
#endif
#ifndef i_keyfrom
  #define i_keyfrom c_default_clone
#else
  #define i_has_emplace
#endif
#ifndef i_keyto
  #define i_keyto c_default_toraw
#endif
#ifndef i_keyclone
  #define i_keyclone c_default_clone
#endif
#ifndef i_keydrop
  #define i_keydrop c_default_drop
#endif

#if defined _i_ismap // ---- process cmap/csmap value i_val, ... ----

#ifdef i_val_str
  #define i_val_class cstr
  #define i_valraw const char*
#elif defined i_val_ssv
  #define i_val_class cstr
  #define i_valraw csview
  #define i_valfrom cstr_from_sv
  #define i_valto cstr_sv
#elif defined i_val_arcbox
  #define i_val_class i_val_arcbox
  #define i_valraw c_JOIN(i_val_arcbox, _raw)
#endif

#ifdef i_val_class
  #define i_val i_val_class
  #ifndef i_valclone
    #define i_valclone c_JOIN(i_val, _clone)
  #endif
  #ifndef i_valdrop
    #define i_valdrop c_JOIN(i_val, _drop)
  #endif
  #if !defined i_valfrom && defined i_valraw
    #define i_valfrom c_JOIN(i_val, _from)
  #endif
  #if !defined i_valto && defined i_valraw
    #define i_valto c_JOIN(i_val, _toraw)
  #endif
#endif

#ifndef i_val
  #error "i_val* must be defined for maps"
#elif defined i_valraw ^ defined i_valto
  #error "Both i_valraw and i_valto must be defined, if any"
#elif !defined i_no_clone && (defined i_valclone ^ defined i_valdrop)
  #error "Both i_valclone and i_valdrop must be defined, if any"
#endif

#ifndef i_valraw
  #define i_valraw i_val
#endif
#ifndef i_valfrom
  #define i_valfrom c_default_clone
#else
  #define i_has_emplace
#endif
#ifndef i_valto
  #define i_valto c_default_toraw
#endif
#ifndef i_valclone
  #define i_valclone c_default_clone
#endif
#ifndef i_valdrop
  #define i_valdrop c_default_drop
#endif

#endif // !_i_ismap

#ifndef i_val
  #define i_val i_key
#endif
#ifndef i_valraw
  #define i_valraw i_keyraw
#endif
#ifndef i_has_emplace
  #define i_no_emplace
#endif
#endif
// ### END_FILE_INCLUDE: template.h

#ifndef i_is_forward
   _c_DEFTYPES(_c_cvec_types, i_type, i_key);
#endif
typedef i_keyraw _m_raw;
STC_API i_type          _c_MEMB(_init)(void);
STC_API void            _c_MEMB(_drop)(i_type* self);
STC_API void            _c_MEMB(_clear)(i_type* self);
STC_API _Bool            _c_MEMB(_reserve)(i_type* self, intptr_t cap);
STC_API _Bool            _c_MEMB(_resize)(i_type* self, intptr_t size, _m_value null);
STC_API _m_iter         _c_MEMB(_erase_n)(i_type* self, intptr_t idx, intptr_t n);
STC_API _m_iter         _c_MEMB(_insert_uninit)(i_type* self, intptr_t idx, intptr_t n);
#if defined _i_has_eq || defined _i_has_cmp
STC_API _m_iter         _c_MEMB(_find_in)(_m_iter it1, _m_iter it2, _m_raw raw);
#endif
STC_INLINE void         _c_MEMB(_value_drop)(_m_value* val) { i_keydrop(val); }

STC_INLINE _m_value*    _c_MEMB(_push)(i_type* self, _m_value value) {
                            if (self->_len == self->_cap)
                            if (!_c_MEMB(_reserve)(self, self->_len*2 + 4))
                                return NULL;
                            _m_value *v = self->data + self->_len++;
                            *v = value;
                            return v;
                        }

#if !defined i_no_emplace
STC_API _m_iter
_c_MEMB(_emplace_n)(i_type* self, intptr_t idx, const _m_raw raw[], intptr_t n);

STC_INLINE _m_value* _c_MEMB(_emplace)(i_type* self, _m_raw raw) {
    return _c_MEMB(_push)(self, i_keyfrom(raw));
}
STC_INLINE _m_value* _c_MEMB(_emplace_back)(i_type* self, _m_raw raw) {
     return _c_MEMB(_push)(self, i_keyfrom(raw));
}
STC_INLINE _m_iter _c_MEMB(_emplace_at)(i_type* self, _m_iter it, _m_raw raw) {
    return _c_MEMB(_emplace_n)(self, _it_ptr(it) - self->data, &raw, 1);
}
#endif // !i_no_emplace

#if !defined i_no_clone
STC_API i_type          _c_MEMB(_clone)(i_type cx);
STC_API _m_iter         _c_MEMB(_copy_n)(i_type* self, intptr_t idx, const _m_value arr[], intptr_t n);
STC_INLINE void         _c_MEMB(_put_n)(i_type* self, const _m_raw* raw, intptr_t n)
                            { while (n--) _c_MEMB(_push)(self, i_keyfrom(*raw++)); }
STC_INLINE i_type       _c_MEMB(_from_n)(const _m_raw* raw, intptr_t n)
                            { i_type cx = {0}; _c_MEMB(_put_n)(&cx, raw, n); return cx; }
STC_INLINE _m_value     _c_MEMB(_value_clone)(_m_value val)
                            { return i_keyclone(val); }
STC_INLINE void         _c_MEMB(_copy)(i_type* self, const i_type* other) {
                            if (self->data == other->data) return;
                            _c_MEMB(_clear)(self);
                            _c_MEMB(_copy_n)(self, 0, other->data, other->_len);
                        }
#endif // !i_no_clone

STC_INLINE intptr_t     _c_MEMB(_size)(const i_type* self) { return self->_len; }
STC_INLINE intptr_t     _c_MEMB(_capacity)(const i_type* self) { return self->_cap; }
STC_INLINE _Bool         _c_MEMB(_empty)(const i_type* self) { return !self->_len; }
STC_INLINE _m_raw       _c_MEMB(_value_toraw)(const _m_value* val) { return i_keyto(val); }
STC_INLINE _m_value*    _c_MEMB(_front)(const i_type* self) { return self->data; }
STC_INLINE _m_value*    _c_MEMB(_back)(const i_type* self)
                            { return self->data + self->_len - 1; }
STC_INLINE void         _c_MEMB(_pop)(i_type* self)
                            { c_assert(self->_len); _m_value* p = &self->data[--self->_len]; i_keydrop(p); }
STC_INLINE _m_value     _c_MEMB(_pull)(i_type* self)
                            { c_assert(self->_len); return self->data[--self->_len]; }
STC_INLINE _m_value*    _c_MEMB(_push_back)(i_type* self, _m_value value)
                            { return _c_MEMB(_push)(self, value); }
STC_INLINE void         _c_MEMB(_pop_back)(i_type* self) { _c_MEMB(_pop)(self); }

STC_INLINE i_type
_c_MEMB(_with_size)(const intptr_t size, _m_value null) {
    i_type cx = _c_MEMB(_init)();
    _c_MEMB(_resize)(&cx, size, null);
    return cx;
}

STC_INLINE i_type
_c_MEMB(_with_capacity)(const intptr_t cap) {
    i_type cx = _c_MEMB(_init)();
    _c_MEMB(_reserve)(&cx, cap);
    return cx;
}

STC_INLINE void
_c_MEMB(_shrink_to_fit)(i_type* self) {
    _c_MEMB(_reserve)(self, _c_MEMB(_size)(self));
}

STC_INLINE _m_iter
_c_MEMB(_insert_n)(i_type* self, const intptr_t idx, const _m_value arr[], const intptr_t n) {
    _m_iter it = _c_MEMB(_insert_uninit)(self, idx, n);
    if (it.ref)
        c_memcpy(it.ref, arr, n*c_sizeof *arr);
    return it;
}
STC_INLINE _m_iter
_c_MEMB(_insert_at)(i_type* self, _m_iter it, const _m_value value) {
    return _c_MEMB(_insert_n)(self, _it_ptr(it) - self->data, &value, 1);
}

STC_INLINE _m_iter
_c_MEMB(_erase_at)(i_type* self, _m_iter it) {
    return _c_MEMB(_erase_n)(self, it.ref - self->data, 1);
}
STC_INLINE _m_iter
_c_MEMB(_erase_range)(i_type* self, _m_iter i1, _m_iter i2) {
    return _c_MEMB(_erase_n)(self, i1.ref - self->data, _it2_ptr(i1, i2) - i1.ref);
}

STC_INLINE const _m_value*
_c_MEMB(_at)(const i_type* self, const intptr_t idx) {
    c_assert(idx < self->_len); return self->data + idx;
}
STC_INLINE _m_value*
_c_MEMB(_at_mut)(i_type* self, const intptr_t idx) {
    c_assert(idx < self->_len); return self->data + idx;
}


STC_INLINE _m_iter _c_MEMB(_begin)(const i_type* self) { 
    intptr_t n = self->_len; 
    return c_LITERAL(_m_iter){n ? self->data : NULL, self->data + n};
}

STC_INLINE _m_iter _c_MEMB(_end)(const i_type* self) 
    { return c_LITERAL(_m_iter){NULL, self->data + self->_len}; }

STC_INLINE void _c_MEMB(_next)(_m_iter* it) 
    { if (++it->ref == it->end) it->ref = NULL; }

STC_INLINE _m_iter _c_MEMB(_advance)(_m_iter it, size_t n)
    { if ((it.ref += n) >= it.end) it.ref = NULL; return it; }

STC_INLINE intptr_t _c_MEMB(_index)(const i_type* self, _m_iter it) 
    { return (it.ref - self->data); }

STC_INLINE void _c_MEMB(_adjust_end_)(i_type* self, intptr_t n)
    { self->_len += n; }

#if defined _i_has_eq || defined _i_has_cmp
STC_INLINE _m_iter
_c_MEMB(_find)(const i_type* self, _m_raw raw) {
    return _c_MEMB(_find_in)(_c_MEMB(_begin)(self), _c_MEMB(_end)(self), raw);
}

STC_INLINE const _m_value*
_c_MEMB(_get)(const i_type* self, _m_raw raw) {
    return _c_MEMB(_find)(self, raw).ref;
}

STC_INLINE _m_value*
_c_MEMB(_get_mut)(const i_type* self, _m_raw raw)
    { return (_m_value*) _c_MEMB(_get)(self, raw); }

STC_INLINE _Bool
_c_MEMB(_eq)(const i_type* self, const i_type* other) {
    if (self->_len != other->_len) return 0;
    for (intptr_t i = 0; i < self->_len; ++i) {
        const _m_raw _rx = i_keyto(self->data+i), _ry = i_keyto(other->data+i);
        if (!(i_eq((&_rx), (&_ry)))) return 0;
    }
    return 1;
}
#endif

#if defined _i_has_cmp
STC_API int _c_MEMB(_value_cmp)(const _m_value* x, const _m_value* y);

STC_INLINE void
_c_MEMB(_sort)(i_type* self) {
    qsort(self->data, (size_t)self->_len, sizeof(_m_value),
          (int(*)(const void*, const void*))_c_MEMB(_value_cmp));
}

STC_INLINE _m_value*
_c_MEMB(_bsearch)(const i_type* self, _m_value key) {
    return (_m_value*)bsearch(&key, self->data, (size_t)self->_len, sizeof(_m_value),
                              (int(*)(const void*, const void*))_c_MEMB(_value_cmp));
}
#endif // _i_has_cmp

/* -------------------------- IMPLEMENTATION ------------------------- */
#if defined(i_implement) || defined(i_static)

STC_DEF i_type
_c_MEMB(_init)(void) {
    return c_LITERAL(i_type){NULL};
}

STC_DEF void
_c_MEMB(_clear)(i_type* self) {
    if (self->_cap) {
        for (_m_value *p = self->data, *q = p + self->_len; p != q; ) {
            --q; i_keydrop(q);
        }
        self->_len = 0;
    }
}

STC_DEF void
_c_MEMB(_drop)(i_type* self) {
    if (self->_cap == 0)
        return;
    _c_MEMB(_clear)(self);
    i_free(self->data, self->_cap*c_sizeof(*self->data));
}

STC_DEF _Bool
_c_MEMB(_reserve)(i_type* self, const intptr_t cap) {
    if (cap > self->_cap || (cap && cap == self->_len)) {
        _m_value* d = (_m_value*)i_realloc(self->data, self->_cap*c_sizeof *d,
                                                       cap*c_sizeof *d);
        if (!d)
            return 0;
        self->data = d;
        self->_cap = cap;
    }
    return 1;
}

STC_DEF _Bool
_c_MEMB(_resize)(i_type* self, const intptr_t len, _m_value null) {
    if (!_c_MEMB(_reserve)(self, len))
        return 0;
    const intptr_t n = self->_len;
    for (intptr_t i = len; i < n; ++i)
        { i_keydrop((self->data + i)); }
    for (intptr_t i = n; i < len; ++i)
        self->data[i] = null;
    self->_len = len;
    return 1;
}

STC_DEF _m_iter
_c_MEMB(_insert_uninit)(i_type* self, const intptr_t idx, const intptr_t n) {
    if (self->_len + n > self->_cap)
        if (!_c_MEMB(_reserve)(self, self->_len*3/2 + n))
            return _c_MEMB(_end)(self);

    _m_value* pos = self->data + idx;
    c_memmove(pos + n, pos, (self->_len - idx)*c_sizeof *pos);
    self->_len += n;
    return c_LITERAL(_m_iter){pos, self->data + self->_len};
}

STC_DEF _m_iter
_c_MEMB(_erase_n)(i_type* self, const intptr_t idx, const intptr_t len) {
    _m_value* d = self->data + idx, *p = d, *end = self->data + self->_len;
    for (intptr_t i = 0; i < len; ++i, ++p)
        { i_keydrop(p); }
    c_memmove(d, p, (end - p)*c_sizeof *d);
    self->_len -= len;
    return c_LITERAL(_m_iter){p == end ? NULL : d, end - len};
}

#if !defined i_no_clone
STC_DEF i_type
_c_MEMB(_clone)(i_type cx) {
    i_type out = _c_MEMB(_init)();
    _c_MEMB(_copy_n)(&out, 0, cx.data, cx._len);
    return out;
}

STC_DEF _m_iter
_c_MEMB(_copy_n)(i_type* self, const intptr_t idx,
                 const _m_value arr[], const intptr_t n) {
    _m_iter it = _c_MEMB(_insert_uninit)(self, idx, n);
    if (it.ref)
        for (_m_value* p = it.ref, *q = p + n; p != q; ++arr)
            *p++ = i_keyclone((*arr));
    return it;
}
#endif // !i_no_clone

#if !defined i_no_emplace
STC_DEF _m_iter
_c_MEMB(_emplace_n)(i_type* self, const intptr_t idx, const _m_raw raw[], intptr_t n) {
    _m_iter it = _c_MEMB(_insert_uninit)(self, idx, n);
    if (it.ref)
        for (_m_value* p = it.ref; n--; ++raw, ++p)
            *p = i_keyfrom((*raw));
    return it;
}
#endif // !i_no_emplace
#if defined _i_has_eq || defined _i_has_cmp

STC_DEF _m_iter
_c_MEMB(_find_in)(_m_iter i1, _m_iter i2, _m_raw raw) {
    const _m_value* p2 = _it2_ptr(i1, i2);
    for (; i1.ref != p2; ++i1.ref) {
        const _m_raw r = i_keyto(i1.ref);
        if (i_eq((&raw), (&r)))
            return i1;
    }
    i2.ref = NULL;
    return i2;
}
#endif
#if defined _i_has_cmp
STC_DEF int _c_MEMB(_value_cmp)(const _m_value* x, const _m_value* y) {
    const _m_raw rx = i_keyto(x);
    const _m_raw ry = i_keyto(y);
    return i_cmp((&rx), (&ry));
}
#endif // _i_has_cmp
#endif // i_implement
#define CVEC_H_INCLUDED
// ### BEGIN_FILE_INCLUDE: template2.h
#ifdef i_more
#undef i_more
#else
#undef i_type
#undef i_tag
#undef i_imp
#undef i_opt
#undef i_less
#undef i_cmp
#undef i_eq
#undef i_hash
#undef i_capacity
#undef i_raw_class

#undef i_val
#undef i_val_str
#undef i_val_ssv
#undef i_val_arcbox
#undef i_val_class
#undef i_valraw
#undef i_valclone
#undef i_valfrom
#undef i_valto
#undef i_valdrop

#undef i_key
#undef i_key_str
#undef i_key_ssv
#undef i_key_arcbox
#undef i_key_class
#undef i_keyraw
#undef i_keyclone
#undef i_keyfrom
#undef i_keyto
#undef i_keydrop

#undef i_use_cmp
#undef i_no_hash
#undef i_no_clone
#undef i_no_emplace
#undef i_is_forward
#undef i_has_emplace

#undef _i_has_cmp
#undef _i_has_eq
#undef _i_prefix
#undef _i_template

#undef i_keyclass // [deprecated]
#undef i_valclass // [deprecated]
#undef i_rawclass // [deprecated]
#undef i_keyboxed // [deprecated]
#undef i_valboxed // [deprecated]
#endif
// ### END_FILE_INCLUDE: template2.h
// ### BEGIN_FILE_INCLUDE: linkage2.h

#undef i_allocator
#undef i_malloc
#undef i_calloc
#undef i_realloc
#undef i_free

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
// ### END_FILE_INCLUDE: cvec.h

