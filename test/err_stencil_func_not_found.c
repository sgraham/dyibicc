// RET: 255
// TXT: test/err_stencil_func_not_found.c:13: XYZ_func
// TXT:                                       ^ error: undefined variable

struct __attribute__((stencil(XYZ_))) X {
  int a;
  int b;
};
typedef struct X X;

int main(void) {
  X x = {5, 6};
  x..func(32);
}
