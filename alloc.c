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

CompilerState compiler_state;
LinkerState linker_state;

#define C(x) compiler_state.alloc__##x
#define L(x) linker_state.alloc__##x

// Reports an error and exit.
void error(char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  output_fn(2, fmt, ap);
  logerr("\n");
  exit(1);
}

void alloc_init(AllocLifetime lifetime) {
  assert(lifetime < NUM_BUMP_HEAPS);
  if (lifetime == AL_Compile) {
    memset(&compiler_state, 0, offsetof(CompilerState, alloc__heap));
    C(current_alloc_pointer) = C(heap);
    ASAN_POISON_MEMORY_REGION(C(heap), sizeof(C(heap)));
  } else if (lifetime == AL_Link) {
    memset(&linker_state, 0, offsetof(LinkerState, alloc__heap));
    L(current_alloc_pointer) = L(heap);
    ASAN_POISON_MEMORY_REGION(L(heap), sizeof(L(heap)));
  } else {
    unreachable();
  }
}

void alloc_reset(AllocLifetime lifetime) {
  assert(lifetime < NUM_BUMP_HEAPS);
  if (lifetime == AL_Compile) {
    C(current_alloc_pointer) = NULL;
    ASAN_POISON_MEMORY_REGION(C(heap), sizeof(C(heap)));
  } else if (lifetime == AL_Link) {
    L(current_alloc_pointer) = NULL;
    ASAN_POISON_MEMORY_REGION(L(heap), sizeof(L(heap)));
  } else {
    unreachable();
  }
}

void* bumpcalloc(size_t num, size_t size, AllocLifetime lifetime) {
  if (lifetime == AL_Manual) {
    return calloc(num, size);
  }

  assert(lifetime < NUM_BUMP_HEAPS);
  size_t toalloc = align_to_u(num * size, 8);
  char* ret;
  if (lifetime == AL_Compile) {
    ret = C(current_alloc_pointer);
    C(current_alloc_pointer) += toalloc;
    if (C(current_alloc_pointer) > C(heap) + sizeof(C(heap))) {
      error("heap exhausted");
    }
  } else if (lifetime == AL_Link) {
    ret = L(current_alloc_pointer);
    L(current_alloc_pointer) += toalloc;
    if (L(current_alloc_pointer) > L(heap) + sizeof(L(heap))) {
      error("heap exhausted");
    }
  } else {
    unreachable();
  }

  ASAN_UNPOISON_MEMORY_REGION(ret, toalloc);
  memset(ret, 0, toalloc);

  return ret;
}

void alloc_free(void* p, AllocLifetime lifetime) {
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
