#pragma once

#include <stdarg.h>
#include <stdbool.h>

// ANSI escape code decoding expected if use_ansi_codes is set in
// DyibiccEnviromentData.
typedef int (*DyibiccOutputFn)(const char* fmt, va_list ap);

// Returns the address of a function by name.
typedef void* (*DyibiccFunctionLookupFn)(const char* name);

typedef struct DyibiccEnviromentData {
  const char** include_paths;
  const char** files;
  const char* dyibicc_include_dir;
  DyibiccFunctionLookupFn get_function_address;
  DyibiccOutputFn output_function;
  bool use_ansi_codes;
  bool padding[7];  // Avoid C4820 padding warning on MSVC /Wall.
} DyibiccEnviromentData;

typedef struct DyibiccContext DyibiccContext;

DyibiccContext* dyibicc_set_environment(DyibiccEnviromentData* env_data);

bool dyibicc_update(DyibiccContext* context, char* file, char* contents);

// The returned address cannot be cached across dyibicc_update() calls.
void* dyibicc_find_export(DyibiccContext* context, char* name);

void dyibicc_free(DyibiccContext* context);
