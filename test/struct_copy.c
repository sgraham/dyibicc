#include <stdio.h>
static void printint(int x) {
  printf("%d\n", x);
}

struct B {
  int b;
  int c;
  int x;
};

struct A {
  struct B b;
};

void A(struct B b) {
  struct A a;
  a.b = b;
  printint(a.b.b);
  printint(a.b.c);
  printint(a.b.x);
}

int main(void) {
  struct B b = (struct B){3, 2, 1};
  A(b);
}
