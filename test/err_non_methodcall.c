// RET: 255
// TXT: test/err_non_methodcall.c:13:   x..func(32);
// TXT:                                 ^ error: not an __attribute__((methodcall(prefix))) type

struct X {
  int a;
  int b;
};
typedef struct X X;

int main(void) {
  X x = {5, 6};
  x..func(32);
}
