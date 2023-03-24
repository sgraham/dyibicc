#include "dyibicc.h"

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
  return strdup(buf);
}
