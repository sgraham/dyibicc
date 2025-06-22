// RET: 255
// TXT: test/err_attrib_func_without_include.c:4: static inline __attribute__((unused)) uint64_t some_attrib(void) {
// TXT:                                                                                 ^ error: expected type
static inline __attribute__((unused)) uint64_t some_attrib(void) {
  return 123;
}
