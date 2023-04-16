//#include "test.h"

void __dyibicc_qtrace_func_enter(char*, char*, ...);
void __dyibicc_qtrace_func_leave(char*, char*, ...);
int printf();

int tracey(int);

#if 0
double counter;

🔎 int zippy(int arg) {
  return arg + 5;
}

#endif
int main(void) {
  //__dyibicc_qtrace_enable();
  int x = tracey(14);
  //ASSERT(76, x);
  return x;
  //ASSERT(0, strcmp(__dyibicc_qtrace_buffer(), ""));
}

🔎 int tracey(int x) {
  printf("in tracey\n");
  return 16;
}
