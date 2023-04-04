#include "dyibicc.h"

DyibiccOutputFn output_fn;

char* bumpstrndup(const char* s, size_t n) {
  size_t l = strnlen(s, n);
  char* d = bumpcalloc(1, l + 1);
  if (!d)
    return NULL;
  memcpy(d, s, l);
  d[l] = 0;
  return d;
}

char* bumpstrdup(const char* s) {
  size_t l = strlen(s);
  char* d = bumpcalloc(1, l + 1);
  if (!d)
    return NULL;
  memcpy(d, s, l);
  d[l] = 0;
  return d;
}

char* dirname(char* s) {
  size_t i;
  if (!s || !*s)
    return ".";
  i = strlen(s) - 1;
  for (; s[i] == '/' || s[i] == '\\'; i--)
    if (!i)
      return "/";
  for (; s[i] != '/' || s[i] == '\\'; i--)
    if (!i)
      return ".";
  for (; s[i] == '/' || s[i] == '\\'; i--)
    if (!i)
      return "/";
  s[i + 1] = 0;
  return s;
}

char* basename(char* s) {
  size_t i;
  if (!s || !*s)
    return ".";
  i = strlen(s) - 1;
  for (; i && (s[i] == '/' || s[i] == '\\'); i--)
    s[i] = 0;
  for (; i && s[i - 1] != '/' && s[i - 1] != '\\'; i--)
    ;
  return s + i;
}

// Round up `n` to the nearest multiple of `align`. For instance,
// align_to(5, 8) returns 8 and align_to(11, 8) returns 16.
uint64_t align_to_u(uint64_t n, uint64_t align) {
  return (n + align - 1) / align * align;
}

int64_t align_to_s(int64_t n, int64_t align) {
  return (n + align - 1) / align * align;
}

void strarray_push(StringArray* arr, char* s) {
  if (!arr->data) {
    arr->data = bumpcalloc(8, sizeof(char*));
    arr->capacity = 8;
  }

  if (arr->capacity == arr->len) {
    arr->data = bumplamerealloc(arr->data, sizeof(char*) * arr->capacity,
                                sizeof(char*) * arr->capacity * 2);
    arr->capacity *= 2;
    for (int i = arr->len; i < arr->capacity; i++)
      arr->data[i] = NULL;
  }

  arr->data[arr->len++] = s;
}

void strintarray_push(StringIntArray* arr, StringInt item) {
  if (!arr->data) {
    arr->data = bumpcalloc(8, sizeof(StringInt));
    arr->capacity = 8;
  }

  if (arr->capacity == arr->len) {
    arr->data = bumplamerealloc(arr->data, sizeof(StringInt) * arr->capacity,
                                sizeof(StringInt) * arr->capacity * 2);
    arr->capacity *= 2;
    for (int i = arr->len; i < arr->capacity; i++)
      arr->data[i] = (StringInt){NULL, -1};
  }

  arr->data[arr->len++] = item;
}

void bytearray_push(ByteArray* arr, char b) {
  if (!arr->data) {
    arr->data = bumpcalloc(8, sizeof(char));
    arr->capacity = 8;
  }

  if (arr->capacity == arr->len) {
    arr->data =
        bumplamerealloc(arr->data, sizeof(char) * arr->capacity, sizeof(char) * arr->capacity * 2);
    arr->capacity *= 2;
    for (int i = arr->len; i < arr->capacity; i++)
      arr->data[i] = 0;
  }

  arr->data[arr->len++] = b;
}

void intintarray_push(IntIntArray* arr, IntInt item) {
  if (!arr->data) {
    arr->data = bumpcalloc(8, sizeof(IntInt));
    arr->capacity = 8;
  }

  if (arr->capacity == arr->len) {
    arr->data = bumplamerealloc(arr->data, sizeof(IntInt) * arr->capacity,
                                sizeof(IntInt) * arr->capacity * 2);
    arr->capacity *= 2;
    for (int i = arr->len; i < arr->capacity; i++)
      arr->data[i] = (IntInt){0, 0};
  }

  arr->data[arr->len++] = item;
}

// Takes a printf-style format string and returns a formatted string.
char* format(char* fmt, ...) {
  char buf[4096];

  va_list ap;
  va_start(ap, fmt);
  vsprintf(buf, fmt, ap);
  va_end(ap);
  return bumpstrdup(buf);
}

int logdbg(const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  int ret = output_fn(0, fmt, ap);
  va_end(ap);
  return ret;
}

int logout(const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  int ret = output_fn(1, fmt, ap);
  va_end(ap);
  return ret;
}

int logerr(const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  int ret = output_fn(2, fmt, ap);
  va_end(ap);
  return ret;
}
