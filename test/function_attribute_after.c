// RET: 23
typedef unsigned long long size_t;

// Something without one too.
void* memset(void* dest, int ch, size_t count);

void blabha(const char* __restrict fmt, ...) __attribute__((noreturn));

void printy(const char* __restrict fmt, ...)
    __attribute__((__format__(__printf__, fmtarg, firstvararg)));

int main(void) {
  return 23;
}
