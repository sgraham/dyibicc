#include "test.h"
#include <stdint.h>
#include <stdlib.h>
struct $Str {
  char* ptr;
  int64_t len;
};

#define $Str$__lit__(s) $Str$from_n(s, sizeof(s) - 1)

struct $Str $Str$from_n(char* data, size_t len) {
  struct $Str s = {malloc(len + 1), len};
  memcpy(s.ptr, data, len + 1);
  return s;
}

static void printstr(struct $Str s) {
  ASSERT('h', s.ptr[0]);
  ASSERT('e', s.ptr[1]);
  ASSERT('l', s.ptr[2]);
  ASSERT('l', s.ptr[3]);
  ASSERT('o', s.ptr[4]);
  ASSERT(0, s.ptr[5]);
  ASSERT(5, s.len);
  free(s.ptr);
}

int main(void) {
  {
    printstr($Str$__lit__("hello"));
  }
}
