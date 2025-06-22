// RET: 23
typedef unsigned long long size_t;

// Something without one too.
void* memset(void* dest, int ch, size_t count);

void blabha(const char* __restrict fmt, ...) __attribute__((noreturn)) __attribute__((__cold__));

void printy(const char* __restrict fmt, ...)
    __attribute__((__format__(__printf__, fmtarg, firstvararg)));

void multiple(const char* __restrict fmt, ...) __attribute__((__noreturn__))
__attribute__((__cold__)) __attribute__((__pure__));

int main(void) {
  return 23;
}
