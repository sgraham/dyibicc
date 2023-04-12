// RUN: {self}
// RET: 255
// TXT: {self}:8:   c += f();
// TXT:                           ^ error: += expression with type void
void f(void);
int main() {
  double c;
  c += f();
}
