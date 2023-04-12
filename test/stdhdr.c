// RUN: -Itest test/common.c {self}
// RET: 0
#include "test.h"
#include <float.h>
#include <stdalign.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdnoreturn.h>

int main() {
  printf("OK\n");
  return 0;
}
