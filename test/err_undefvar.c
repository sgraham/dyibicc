// RUN: {self}
// RET: 255
// TXT: {self}:6:   in
// TXT:                          ^ error: undefined variable
int main() {
  in
}
