// RUN: {self}
// RET: 255
// TXT: test/err_redefstruct.c:13: struct X {
// TXT:                                   ^ error: redefinition of type
typedef struct A {
  int y;
} A;

struct X {
  int x;
};

struct X {
  int x;
};
