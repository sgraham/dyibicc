#pragma once

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>

// ANSI escape code decoding expected if use_ansi_codes is set in
// DyibiccEnviromentData.
typedef int (*DyibiccOutputFn)(const char* fmt, va_list ap);

// Returns the address of a function by name.
typedef void* (*DyibiccFunctionLookupFn)(const char* name);

// Get the current contents of a file. Should return true on success with
// *contents and *size filled out. *contents need not be null-terminated.
// *contents ownership is taken and will be free()d.
typedef bool (*DyibiccLoadFileContents)(const char* filename, char** contents, size_t* size);

typedef struct DyibiccEnviromentData {
  // NULL-terminated list of user include paths to search.
  const char** include_paths;

  // NULL-terminated list of .c files to include in the project.
  const char** files;

  // Path to the compiler's include directory.
  const char* dyibicc_include_dir;

  // Load the contents of a file by name (typically from disk).
  //
  // NOTE/TODO: There is currently a gotcha with this callback. It is used in
  // two situations:
  //
  // 1) for loading all .c files on initial startup (when update is
  // typically called as `dyibicc_update(ctx, NULL, NULL)`), and;
  //
  // 2) at all times when files are loaded by an `#include`.
  //
  // However! If a single file and contents are provided to update via
  // `dyibicc_update(ctx, "myfile.c", "...contents...")`, then the contents will
  // be used directly and there will be no callback to this function.
  DyibiccLoadFileContents load_file_contents;

  // Should resolve a function by name, for symbols that aren't defined by code
  // in |files|. i.e. to call system functionality.
  DyibiccFunctionLookupFn get_function_address;

  // Customizable output, all output from compiler error messages, etc. will be
  // vectored through this function.
  DyibiccOutputFn output_function;

  // Are simple ANSI colours supported by |output_function|.
  bool use_ansi_codes;

  // Should debug symbols (pdb) be generated. Only implemented on Windows.
  bool generate_debug_symbols;

  bool padding[6];  // Avoid C4820 padding warning on MSVC /Wall.
} DyibiccEnviromentData;

typedef struct DyibiccContext DyibiccContext;

// Sets up the environment for the compiler. There can currently only be a
// single active DyibiccContext, despite the implication that there could be
// multiple. See notes in the structure about how it should be filled out.
DyibiccContext* dyibicc_set_environment(DyibiccEnviromentData* env_data);

// Called once on initializtion with a file == NULL and contents == NULL, and
// subsequently whenever any file contents are updated and the running code
// should be recompiled/relinked.
bool dyibicc_update(DyibiccContext* context, char* file, char* contents);

// After a successful call to dyibicc_update(), retrieve the address of a
// non-static function to call it. The returned function address cannot be
// cached across dyibicc_update() calls.
void* dyibicc_find_export(DyibiccContext* context, char* name);

// Free all memory associated with the compiler context.
void dyibicc_free(DyibiccContext* context);
