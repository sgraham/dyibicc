#include "dyibicc.h"

#if X64WIN
#include <windows.h>
#else
#include <sys/mman.h>
#endif

static char* allmem;
static char* current_alloc_pointer;

#define HEAP_SIZE (256 << 20)

// Reports an error and exit.
void error(char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  fprintf(stderr, "\n");
  exit(1);
}

void bumpcalloc_init(void) {
  allmem = allocate_writable_memory(HEAP_SIZE);
  current_alloc_pointer = allmem;
}

void* bumpcalloc(size_t num, size_t size) {
  size_t toalloc = align_to((int)(num * size), 8);
  char* ret = current_alloc_pointer;
  current_alloc_pointer += toalloc;
  if (current_alloc_pointer > allmem + HEAP_SIZE) {
    error("heap exhausted");
  }
  memset(ret, 0, toalloc);
  return ret;
}

void* bumplamerealloc(void* old, size_t old_size, size_t new_size) {
  void* newptr = bumpcalloc(1, new_size);
  memcpy(newptr, old, MIN(old_size, new_size));
  return newptr;
}

void bumpcalloc_reset(void) {
  free_executable_memory(allmem, HEAP_SIZE);
  allmem = NULL;
  current_alloc_pointer = NULL;
}

void* aligned_allocate(size_t size, size_t alignment) {
#if X64WIN
  return _aligned_malloc(size, alignment);
#else
  return aligned_alloc(alignment, size);
#endif
}

// Allocates RW memory of given size and returns a pointer to it. On failure,
// prints out the error and returns NULL. Unlike malloc, the memory is allocated
// on a page boundary so it's suitable for calling mprotect.
void* allocate_writable_memory(size_t size) {
#if X64WIN
  void* p = VirtualAlloc(0, size, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
  if (!p) {
    error("VirtualAlloc failed: 0x%x\n", GetLastError());
  }
  return p;
#else
  void* ptr = mmap(0, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (ptr == (void*)-1) {
    perror("mmap");
    return NULL;
  }
  return ptr;
#endif
}

// Sets a RX permission on the given memory, which must be page-aligned. Returns
// 0 on success. On failure, prints out the error and returns -1.
bool make_memory_executable(void* m, size_t size) {
#if X64WIN
  (void)m;
  (void)size;
  // TODO: alloc as non-execute
  return true;
#else
  if (mprotect(m, size, PROT_READ | PROT_EXEC) == -1) {
    perror("mprotect");
    return false;
  }
  return true;
#endif
}

void free_executable_memory(void* p, size_t size) {
#if X64WIN
  VirtualFree(p, size, MEM_RELEASE);
#else
  munmap(p, size);
#endif
}
