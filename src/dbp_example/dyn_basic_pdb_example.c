// This is an example of dyn_basic_pdb.h, which handles generating a .pdb for
// JIT-generated code, and then entices Visual Studio into loading the pdb and
// associating it with the JIT'd code.
//
// Stepping through main() in Visual Studio should give a general idea of how
// the library is supposed to be used.

// clang-format off
// This is the pretend output of the JIT compiler. Two simple functions that
// correspond to helper.py:Func and entry.py:Entry (also in this directory).
static unsigned char test_data[] = {
  /* --- Func */
  /* 0x0000: */ 0x48, 0x83, 0xec, 0x18,                    // sub         rsp,18h
  /* 0x0004: */ 0xc7, 0x04, 0x24, 0x04, 0x00, 0x00, 0x00,  // mov         dword ptr [rsp],4
  /* 0x000b: */ 0x8b, 0x04, 0x24,                          // mov         eax,dword ptr [rsp]
  /* 0x000e: */ 0x83, 0xc0, 0x64,                          // add         eax,64h
  /* 0x0011: */ 0x48, 0x83, 0xc4, 0x18,                    // add         rsp,18h
  /* 0x0015: */ 0xc3,                                      // ret
  /* --- Entry */
  /* 0x0016: */ 0x48, 0x83, 0xec, 0x38,        // sub         rsp,38h
  /* 0x001a: */ 0xe8, 0xe1, 0xff, 0xff, 0xff,  // call        Func
  /* 0x001f: */ 0x05, 0xe8, 0x03, 0x00, 0x00,  // add         eax,3E8h
  /* 0x0024: */ 0x89, 0x44, 0x24, 0x20,        // mov         dword ptr [rsp+20h],eax
  /* 0x0028: */ 0x8b, 0x44, 0x24, 0x20,        // mov         eax,dword ptr [rsp+20h]
  /* 0x002c: */ 0x48, 0x83, 0xc4, 0x38,        // add         rsp,38h
  /* 0x0030: */ 0xc3,                          // ret

  /* --- padding */
  /* 0x0031  */ 0xcc, // int 3
  /* 0x0032  */ 0xcc, // int 3
  /* 0x0033  */ 0xcc, // int 3

  // --- UnwindData for RtlAddFunctionTable(), see exception_tables.
  /* 0x0034  */ 0x01, 0x04, 0x01, 0x00, 0x04, 0x22, 0x00, 0x00,
  /* 0x003c  */ 0x01, 0x04, 0x01, 0x00, 0x04, 0x62, 0x00, 0x00,
};
// clang-format on

#ifdef _DEBUG
#ifdef __clang__
#pragma clang diagnostic ignored "-Wreserved-macro-identifier"
#endif
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#endif

#define DYN_BASIC_PDB_IMPLEMENTATION
#include "dyn_basic_pdb.h"

#include <Windows.h>
#include <stdio.h>

#define CHECK(x)                                                            \
  do {                                                                      \
    if (!(x)) {                                                             \
      fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #x); \
      abort();                                                              \
    }                                                                       \
  } while (0)

// Just a goofy example function to get the fully qualified correct path
// to the pretend interpreter sources so that VS doesn't ask to browse to
// find them locally if the cwd has changed.
static void get_file_path(const char* fn, char* buf_out, DWORD buf_out_size) {
  char buf[MAX_PATH];
  sprintf_s(buf, sizeof(buf), "%s\\..\\%s", __FILE__, fn);
  CHECK(GetFullPathName(buf, buf_out_size, buf_out, NULL));
}

static DbpRUNTIME_FUNCTION function_table[] = {
    {.begin_address = 0, .end_address = 0x16, .unwind_data = 0},
    {.begin_address = 0x16, .end_address = 0x31, .unwind_data = 8},
};

static DbpExceptionTables exception_tables = {
  .pdata = function_table,
  .num_pdata_entries = 2,
  .unwind_info = &test_data[0x34],
  .unwind_info_byte_length = 16,
};

int main(int argc, char** argv) {
  // Create a context. |image_size| is VirtualAlloc()d so must be a multiple of
  // PAGE_SIZE (== 4096).
  size_t image_size = 4096;
  DbpContext* ctx = dbp_create(image_size, argc < 2 ? "dbp.pdb" : argv[1]);

  // "JIT" some code. The source code is found in entry.py and helper.py, and
  // the compiled binary code is above in |test_data|.
  char* image_addr = dbp_get_image_base(ctx);
  CHECK(image_addr);
  memcpy(image_addr, test_data, sizeof(test_data));

  // Silly example code to get full path to sources. If dynamically allocated,
  // the file name pointers passed in can be freed immediately after the call to
  // dbp_add_function_symbol().
  char helper_fn[MAX_PATH], entry_fn[MAX_PATH];
  get_file_path("helper.py", helper_fn, MAX_PATH);
  get_file_path("entry.py", entry_fn, MAX_PATH);

  // Add the name and byte range of our two functions.
  DbpFunctionSymbol* fs_func = dbp_add_function_symbol(ctx, "Func", helper_fn, 0x00, 0x16);
  DbpFunctionSymbol* fs_zippy = dbp_add_function_symbol(ctx, "Entry", entry_fn, 0x16, 0x31);

  // Fill out offset/line information. The offset corresponds to the byte offset
  // in |test_data| above, and the line number is a one-based line number in the
  // source file that contains the symbol. TODO: Probably need to expand this
  // interface for inlining, etc.
  dbp_add_line_mapping(ctx, fs_func, 0x00, 1);
  dbp_add_line_mapping(ctx, fs_func, 0x04, 2);
  dbp_add_line_mapping(ctx, fs_func, 0x0b, 3);

  dbp_add_line_mapping(ctx, fs_zippy, 0x16, 1);
  dbp_add_line_mapping(ctx, fs_zippy, 0x1a, 2);
  dbp_add_line_mapping(ctx, fs_zippy, 0x28, 3);

  // dyn_basic_pdb does not support any type information, only function symbols,
  // and source line mappings, so that's all the information we have to add.

  // Completes the generation of the pdb, and tricks VS into loading it. The
  // code is also VirtualProtect()d to PAGE_EXECUTE_READ. (You should see a DLL
  // in the "Modules" window with "Symbols loaded" after stepping over this
  // call.)
  CHECK(dbp_ready_to_execute(ctx, &exception_tables));

  // Try: Set a breakpoint here in VS, and step into the following calls
  // First, we step into a simple helper.
  int result = ((int (*)(void))(image_addr))();
  printf("Func() returned: %d\n", result);

  // Then step into another function that also calls `Func()` itself.
  // Step In, Step Out, Set Next Statement, View Disassembly, etc. should all
  // work as expected.
  int result2 = ((int (*)(void))(image_addr + 0x0016))();
  printf("Zippy() returned: %d\n", result2);

  // Frees the DbpContext and associated resources, including the memory holding
  // the JIT'd code.
  dbp_free(ctx);

#ifdef _DEBUG
  assert(!_CrtDumpMemoryLeaks());
#endif
}
