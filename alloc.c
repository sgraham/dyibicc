#include "dyibicc.h"

#if X64WIN
#include <windows.h>
#else
#include <sys/mman.h>
#endif

// MSVC chokes during preprocess on __has_feature().
#if defined(__has_feature)
#if __has_feature(address_sanitizer)
#define __SANITIZE_ADDRESS__ 1
#endif
#endif

#if defined(__SANITIZE_ADDRESS__)
void __asan_poison_memory_region(void const volatile* addr, size_t size);
void __asan_unpoison_memory_region(void const volatile* addr, size_t size);
#define ASAN_POISON_MEMORY_REGION(addr, size) __asan_poison_memory_region((addr), (size))
#define ASAN_UNPOISON_MEMORY_REGION(addr, size) __asan_unpoison_memory_region((addr), (size))
#else
#define ASAN_POISON_MEMORY_REGION(addr, size) ((void)(addr), (void)(size))
#define ASAN_UNPOISON_MEMORY_REGION(addr, size) ((void)(addr), (void)(size))
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
  assert(!allmem);
  allmem = allocate_writable_memory(HEAP_SIZE);
  current_alloc_pointer = allmem;
  ASAN_POISON_MEMORY_REGION(allmem, HEAP_SIZE);
}

void* bumpcalloc(size_t num, size_t size) {
  size_t toalloc = align_to_u(num * size, 8);
  char* ret = current_alloc_pointer;
  current_alloc_pointer += toalloc;
  if (current_alloc_pointer > allmem + HEAP_SIZE) {
    error("heap exhausted");
  }
  ASAN_UNPOISON_MEMORY_REGION(ret, toalloc);
  memset(ret, 0, toalloc);
  return ret;
}

void* bumplamerealloc(void* old, size_t old_size, size_t new_size) {
  void* newptr = bumpcalloc(1, new_size);
  memcpy(newptr, old, MIN(old_size, new_size));
  ASAN_POISON_MEMORY_REGION(old, old_size);
  return newptr;
}

void bumpcalloc_reset(void) {
  free_executable_memory(allmem, HEAP_SIZE);
  ASAN_POISON_MEMORY_REGION(allmem, HEAP_SIZE);
  allmem = NULL;
  current_alloc_pointer = NULL;
}

void* aligned_allocate(size_t size, size_t alignment) {
  size = align_to_u(size, alignment);
#if X64WIN
  return _aligned_malloc(size, alignment);
#else
  return aligned_alloc(alignment, size);
#endif
}

void aligned_free(void* p) {
#if X64WIN
  _aligned_free(p);
#else
  free(p);
#endif
}

// Allocates RW memory of given size and returns a pointer to it. On failure,
// prints out the error and returns NULL. Unlike malloc, the memory is allocated
// on a page boundary so it's suitable for calling mprotect.
void* allocate_writable_memory(size_t size) {
#if X64WIN
  void* p = VirtualAlloc(0, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
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
  DWORD old_protect;
  if (!VirtualProtect(m, size, PAGE_EXECUTE_READ, &old_protect)) {
    error("VirtualProtect failed: 0x%x\n", GetLastError());
  }
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
  (void)size;  // If |size| is passed, free will fail.
  if (!VirtualFree(p, 0, MEM_RELEASE)) {
    error("VirtualFree failed: 0x%x\n", GetLastError());
  }
#else
  munmap(p, size);
#endif
}
