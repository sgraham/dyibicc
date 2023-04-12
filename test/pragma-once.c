// RUN: -Itest test/common.c {self}
// RET: 0
#include "test.h"

#pragma once

#include "test/pragma-once.c"

int main() {
  printf("OK\n");
  return 0;
}
