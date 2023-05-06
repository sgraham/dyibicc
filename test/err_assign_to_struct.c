// RUN: {self}
// RET: 255
// TXT: {self}:8:   abc = -4;
// TXT:                                  ^ error: cannot assign to struct
typedef struct Blah { float x, y; } Blah;
Blah abc;
int main(void) {
  abc = -4;
}
