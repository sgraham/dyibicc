#pragma once

#include <stdarg.h>
#include <stdbool.h>

// level == 0 debug log
// level == 1 regular user message
// level == 2 error
typedef int (*DyibiccOutputFn)(int level, const char* fmt, va_list ap);

// Returns the address of a function by name.
typedef void* (*DyibiccFunctionLookupFn)(const char* name);

typedef void* DyibiccEntryPointFn;

typedef struct DyibiccEnviromentData {
  const char** include_paths;
  const char** files;
  const char* entry_point_name;
  const char* cache_dir;
  const char* dyibicc_include_dir;
  DyibiccFunctionLookupFn get_function_address;
  DyibiccOutputFn output_function;
} DyibiccEnviromentData;

typedef struct DyibiccContext {
  DyibiccEntryPointFn entry_point;
  // Additional internal data allocated here.
} DyibiccContext;

DyibiccContext* dyibicc_set_environment(DyibiccEnviromentData* env_data);

bool dyibicc_update(DyibiccContext* context);

void dyibicc_free(DyibiccContext* context);
