#include "dyibicc.h"

#ifdef _WIN64
#include <windows.h>
#else
#include <errno.h>
#include <sys/stat.h>
#endif

char* bumpstrndup(const char* s, size_t n, AllocLifetime lifetime) {
  size_t l = strnlen(s, n);
  char* d = bumpcalloc(1, l + 1, lifetime);
  if (!d)
    return NULL;
  memcpy(d, s, l);
  d[l] = 0;
  return d;
}

char* bumpstrdup(const char* s, AllocLifetime lifetime) {
  size_t l = strlen(s);
  char* d = bumpcalloc(1, l + 1, lifetime);
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

void strarray_push(StringArray* arr, char* s, AllocLifetime lifetime) {
  if (!arr->data) {
    arr->data = bumpcalloc(8, sizeof(char*), lifetime);
    arr->capacity = 8;
  }

  if (arr->capacity == arr->len) {
    arr->data = bumplamerealloc(arr->data, sizeof(char*) * arr->capacity,
                                sizeof(char*) * arr->capacity * 2, lifetime);
    arr->capacity *= 2;
    for (int i = arr->len; i < arr->capacity; i++)
      arr->data[i] = NULL;
  }

  arr->data[arr->len++] = s;
}

void strintarray_push(StringIntArray* arr, StringInt item, AllocLifetime lifetime) {
  if (!arr->data) {
    arr->data = bumpcalloc(8, sizeof(StringInt), lifetime);
    arr->capacity = 8;
  }

  if (arr->capacity == arr->len) {
    arr->data = bumplamerealloc(arr->data, sizeof(StringInt) * arr->capacity,
                                sizeof(StringInt) * arr->capacity * 2, lifetime);
    arr->capacity *= 2;
    for (int i = arr->len; i < arr->capacity; i++)
      arr->data[i] = (StringInt){NULL, -1};
  }

  arr->data[arr->len++] = item;
}

void bytearray_push(ByteArray* arr, char b, AllocLifetime lifetime) {
  if (!arr->data) {
    arr->data = bumpcalloc(8, sizeof(char), lifetime);
    arr->capacity = 8;
  }

  if (arr->capacity == arr->len) {
    arr->data = bumplamerealloc(arr->data, sizeof(char) * arr->capacity,
                                sizeof(char) * arr->capacity * 2, lifetime);
    arr->capacity *= 2;
    for (int i = arr->len; i < arr->capacity; i++)
      arr->data[i] = 0;
  }

  arr->data[arr->len++] = b;
}

void intintarray_push(IntIntArray* arr, IntInt item, AllocLifetime lifetime) {
  if (!arr->data) {
    arr->data = bumpcalloc(8, sizeof(IntInt), lifetime);
    arr->capacity = 8;
  }

  if (arr->capacity == arr->len) {
    arr->data = bumplamerealloc(arr->data, sizeof(IntInt) * arr->capacity,
                                sizeof(IntInt) * arr->capacity * 2, lifetime);
    arr->capacity *= 2;
    for (int i = arr->len; i < arr->capacity; i++)
      arr->data[i] = (IntInt){0, 0};
  }

  arr->data[arr->len++] = item;
}

#ifdef _WIN64
// From ninja.
int64_t timestamp_from_filetime(const FILETIME* filetime) {
  // FILETIME is in 100-nanosecond increments since the Windows epoch.
  // We don't much care about epoch correctness but we do want the
  // resulting value to fit in a 64-bit integer.
  uint64_t mtime = ((uint64_t)filetime->dwHighDateTime << 32) | ((uint64_t)filetime->dwLowDateTime);
  // 1600 epoch -> 2000 epoch (subtract 400 years).
  return (int64_t)mtime - 12622770400LL * (1000000000LL / 100);
}

int64_t stat_single_file(const char* path) {
  WIN32_FILE_ATTRIBUTE_DATA attrs;
  if (!GetFileAttributesExA(path, GetFileExInfoStandard, &attrs)) {
    DWORD win_err = GetLastError();
    if (win_err == ERROR_FILE_NOT_FOUND || win_err == ERROR_PATH_NOT_FOUND)
      return 0;
    return -1;
  }
  return timestamp_from_filetime(&attrs.ftLastWriteTime);
}
#else

int64_t stat_single_file(const char* path) {
  struct stat st;
  if (stat(path, &st) < 0) {
    if (errno == ENOENT || errno == ENOTDIR)
      return 0;
    return -1;
  }
#if defined(st_mtime)  // A macro, so we're likely on modern POSIX.
  return (int64_t)st.st_mtim.tv_sec * 1000000000LL + st.st_mtim.tv_nsec;
#else
  return (int64_t)st.st_mtime * 1000000000LL + st.st_mtimensec;
#endif
}

#endif

// Returns the contents of a given file. Doesn't support '-' for reading from
// stdin.
char* read_file(char* path, AllocLifetime lifetime) {
  FILE* fp = fopen(path, "rb");
  if (!fp) {
    return NULL;
  }

  fseek(fp, 0, SEEK_END);
  long long size = ftell(fp);
  rewind(fp);
  char* buf = bumpcalloc(1, size + 1, lifetime);  // TODO: doesn't really need a calloc
  long long n = fread(buf, 1, size, fp);
  fclose(fp);
  buf[n] = 0;
  return buf;
}

// Takes a printf-style format string and returns a formatted string.
char* format(AllocLifetime lifetime, char* fmt, ...) {
  char buf[4096];

  va_list ap;
  va_start(ap, fmt);
  vsprintf(buf, fmt, ap);
  va_end(ap);
  return bumpstrdup(buf, lifetime);
}

int outaf(const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  int ret = user_context->output_function(fmt, ap);
  va_end(ap);
  return ret;
}
