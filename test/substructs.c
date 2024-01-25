#include <stdio.h>

struct B {
  int x;
  int y;
  int z;
};

struct C {
  int x;
  int y;
  int z;
  int w;
};

struct D {
  int x;
  int y;
  int z;
  int w;
  int v;
};

void f(struct B b) {
  printf("%d %d %d\n", b.x, b.y, b.z);
}

void g(struct C c) {
  printf("%d %d %d %d\n", c.x, c.y, c.z, c.w);
}

void h(struct D d) {
  printf("%d %d %d %d %d\n", d.x, d.y, d.z, d.w, d.v);
}

int main(void) {
  struct B b = {10, 20, 30};
  f(b);
  struct C c = {10, 20, 30, 40};
  g(c);
  struct D d = {10, 20, 30, 40, 50};
  h(d);
  return 0;
}
