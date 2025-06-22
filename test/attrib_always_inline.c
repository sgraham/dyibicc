// RET: 12

#define header_inline extern __inline

#define header_always_inline header_inline __attribute__((__always_inline__))

header_always_inline int __sputc(int _c, void *_p) {
  return 1;
}

int main(void) {
  return 12;
}
