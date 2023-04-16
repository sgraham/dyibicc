// RET: 255
// TXT: test/err_arrayelem.c:13:   Thing* x = things[3];
// TXT:                                       ^ error: value of type Thing can't be assigned to a pointer
typedef struct Thing {
  double x;
  double y;
  double yspeed;
} Thing;

Thing things[5];

int main(void) {
  Thing* x = things[3];
}
