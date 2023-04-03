#pragma once

#include <stdarg.h>
#include <stdbool.h>

// level == 0 debug log
// level == 1 regular user message
// level == 2 error
typedef int (*DyibiccOutputFn)(int level, const char* fmt, va_list ap);
void dyibicc_set_output_function(DyibiccOutputFn f);

typedef void* (*DyibiccFunctionLookupFn)(const char*);
void dyibicc_set_user_runtime_function_callback(DyibiccFunctionLookupFn f);

typedef struct DyibiccLinkInfo {
  void* entry_point;
  char private_data[4088];
} DyibiccLinkInfo;

bool dyibicc_compile_and_link(int argc, char** argv, DyibiccLinkInfo* link_info);

void dyibicc_free_link_info_resources(DyibiccLinkInfo* link_info);
