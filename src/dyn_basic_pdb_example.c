// clang-format off

// This is the simplest possible function, compiled naively:
//
// int Func(void) {        // Line 5
//   int x = 4;            // Line 6
//   return x + 100;       // Line 7
// }                       // Line 8
//
// When debugged, breakpoints and stepping should work in the comment above.
//
static unsigned char test_data[] = {
  /* x0000 */ 0x48, 0x83, 0xec, 0x18,                    // sub rsp, 18h
  /* x0004 */ 0xc7, 0x04, 0x24, 0x04, 0x00, 0x00, 0x00,  // mov dword ptr [rsp], 4
  /* x000b */ 0x8b, 0x04, 0x24,                          // mov eax dword ptr [rsp]
  /* x000e */ 0x83, 0xc0, 0x64,                          // add eax, 64h
  /* x0011 */ 0x48, 0x83, 0xc4, 0x18,                    // add rsp, 18h
  /* x0015 */ 0xc3,                                      // ret
  /* x0016 */ /* ... */
};
// clang-format on

#define DYN_BASIC_PDB_IMPLEMENTATION
#include "dyn_basic_pdb.h"

#include <windows.h>
#include <stdio.h>

#define CHECK(x)                                                          \
  if (!(x)) {                                                             \
    fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #x); \
    abort();                                                              \
  }

int main(int argc, char** argv) {
  // "JIT" some code. The source code is lines 5-8 in this file, and the
  // compiled code is in |test_data|.
  size_t code_size = 4096;
  char* base_addr = VirtualAlloc(NULL, code_size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
  CHECK(base_addr);
  memcpy(base_addr, test_data, sizeof(test_data));

  // Create a context, and fill out file/line information.
  DbpContext* ctx = dbp_create(base_addr, code_size, argc < 2 ? "dbp.pdb" : argv[1]);
  DbpSourceFile* src = dbp_add_source_file(ctx, __FILE__);
  CHECK(dbp_add_line_mapping(src, 5, 0x00, 0x04));
  CHECK(dbp_add_line_mapping(src, 6, 0x04, 0x0b));
  CHECK(dbp_add_line_mapping(src, 7, 0x0b, 0x11));
  CHECK(dbp_add_line_mapping(src, 8, 0x11, 0x16));

  // Complete the generation of the pdb. This takes the following "fun" steps to
  // trigger pdb load in VS:
  // - writes out a final pdb
  // - makes a copy of the code block (as it's overwritten by LoadLibrary below)
  // - creates a stub dll with fixed load address that matches where the code is
  // in memory already
  // - calls LoadLibrary() on that dll, which is recognized by VS in order to
  // load the pdb.
  // - replaces the code that LoadLibrary() blasted
  // - frees |ctx|.
  CHECK(dbp_finish(ctx));

  // Finally, make the code executable.
  DWORD old_protect;
  CHECK(VirtualProtect(base_addr, code_size, PAGE_EXECUTE_READ, &old_protect));

  // Set a breakpoint here in VS, and step into the following call.
  int result = ((int (*)(void))base_addr)();

  printf("returned: %d\n", result);

  CHECK(VirtualFree(base_addr, 0, MEM_RELEASE));
}
