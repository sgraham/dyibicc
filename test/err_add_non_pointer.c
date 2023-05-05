// RUN: {self}
// RET: 255
// TXT: {self}:8:   s + 3;
// TXT:                                   ^ error: invalid operands
typedef struct Struct { int x; } Struct;
int main() {
  Struct s;
  s + 3;
}
