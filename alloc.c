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

#define NUM_BUMP_HEAPS (AL_Link + 1)

UserContext* user_context;
CompilerState compiler_state;
LinkerState linker_state;

typedef struct HeapData {
  char* base;
  char* alloc_pointer;
  size_t size;
} HeapData;

static HeapData heap[NUM_BUMP_HEAPS] = {
    {NULL, NULL, 1024 << 20},  // AL_Compile
    {NULL, NULL, 128 << 20},   // AL_Temp
    {NULL, NULL, 128 << 20},   // AL_Link
};

// Reports an error and exit.
void error(char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  if (!user_context || !user_context->output_function) {
    vfprintf(stderr, fmt, ap);
  } else {
    user_context->output_function(2, fmt, ap);
  }
  logerr("\n");
  exit(1);
}

void alloc_init(AllocLifetime lifetime) {
  assert(lifetime < NUM_BUMP_HEAPS);
  HeapData* hd = &heap[lifetime];

  hd->alloc_pointer = hd->base = allocate_writable_memory(hd->size);
  ASAN_POISON_MEMORY_REGION(hd->base, hd->size);
  if (lifetime == AL_Compile) {
    memset(&compiler_state, 0, sizeof(compiler_state));
  } else if (lifetime == AL_Link) {
    memset(&linker_state, 0, sizeof(linker_state));
  }
}

void alloc_reset(AllocLifetime lifetime) {
  assert(lifetime < NUM_BUMP_HEAPS);
  HeapData* hd = &heap[lifetime];
  ASAN_POISON_MEMORY_REGION(hd->base, hd->size);
  free_executable_memory(hd->base, hd->size);
  hd->alloc_pointer = NULL;
}

void* bumpcalloc(size_t num, size_t size, AllocLifetime lifetime) {
  if (lifetime == AL_Manual) {
    return calloc(num, size);
  }

  size_t toalloc = align_to_u(num * size, 8);
  HeapData* hd = &heap[lifetime];
  char* ret = hd->alloc_pointer;
  hd->alloc_pointer += toalloc;
  if (hd->alloc_pointer > hd->base + hd->size) {
    error("heap exhausted");
  }
  ASAN_UNPOISON_MEMORY_REGION(ret, toalloc);
  memset(ret, 0, toalloc);
  return ret;
}

void alloc_free(void* p, AllocLifetime lifetime) {
  (void)lifetime;
  assert(lifetime == AL_Manual);
  free(p);
}

void* bumplamerealloc(void* old, size_t old_size, size_t new_size, AllocLifetime lifetime) {
  void* newptr = bumpcalloc(1, new_size, lifetime);
  memcpy(newptr, old, MIN(old_size, new_size));
  ASAN_POISON_MEMORY_REGION(old, old_size);
  return newptr;
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
  ASAN_UNPOISON_MEMORY_REGION(p, size);
  return p;
#else
  void* ptr = mmap(0, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (ptr == (void*)-1) {
    perror("mmap");
    return NULL;
  }
  ASAN_UNPOISON_MEMORY_REGION(ptr, size);
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
  ASAN_POISON_MEMORY_REGION(p, size);
#else
  munmap(p, size);
  ASAN_POISON_MEMORY_REGION(p, size);
#endif
}
