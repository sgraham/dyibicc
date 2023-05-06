// RUN: {self}
// RET: 255
// TXT: {self}:10:   abc = xyz;
// TXT:                                             ^ error: cannot assign incompatible structs
typedef struct Blah { float x, y; } Blah;
typedef struct Bloop { int x; int y; int z; } Bloop;
Blah abc;
Bloop xyz;
int main(void) {
  abc = xyz;
}
