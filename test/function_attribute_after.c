// DISABLED

void blabha(const char* __restrict, ...) __attribute__((noreturn));

void printy(const char* __restrict, ...)
    __attribute__((__format__(__printf__, fmtarg, firstvararg)));

int main(void) {
  return 3;
}
