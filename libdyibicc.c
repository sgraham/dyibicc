#include "libdyibicc.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN64
#include <windows.h>
#else
#include <errno.h>
#include <sys/stat.h>
#endif

typedef struct Context {
  DyibiccEntryPointFn entry_point;

  // ^^^ Public definition matches above here. ^^^

  DyibiccFunctionLookupFn get_function_address;
  DyibiccOutputFn output_function;

  size_t num_include_paths;
  char** include_paths;

  size_t num_files;
  char** files;
  int64_t* last_file_timestamps;
  bool* up_to_date;

  const char* entry_point_name;
  const char* cache_directory;

  // TODO: maps to data between links
} Context;

#ifdef _WIN64
// From ninja.
int64_t timestamp_from_filetime(const FILETIME* filetime) {
  // FILETIME is in 100-nanosecond increments since the Windows epoch.
  // We don't much care about epoch correctness but we do want the
  // resulting value to fit in a 64-bit integer.
  uint64_t mtime = ((uint64_t)filetime->dwHighDateTime << 32) | ((uint64_t)filetime->dwLowDateTime);
  // 1600 epoch -> 2000 epoch (subtract 400 years).
  return (int64_t)mtime - 12622770400LL * (1000000000LL / 100);
}

int64_t stat_single_file(const char* path) {
  WIN32_FILE_ATTRIBUTE_DATA attrs;
  if (!GetFileAttributesExA(path, GetFileExInfoStandard, &attrs)) {
    DWORD win_err = GetLastError();
    if (win_err == ERROR_FILE_NOT_FOUND || win_err == ERROR_PATH_NOT_FOUND)
      return 0;
    return -1;
  }
  return timestamp_from_filetime(&attrs.ftLastWriteTime);
}
#else

int64_t stat_single_file(const char* path) {
  struct stat st;
  if (stat(path, &st) < 0) {
    if (errno == ENOENT || errno == ENOTDIR)
      return 0;
    return -1;
  }
#if defined(st_mtime) // A macro, so we're likely on modern POSIX.
  return (int64_t)st.st_mtim.tv_sec * 1000000000LL + st.st_mtim.tv_nsec;
#else
  return (int64_t)st.st_mtime * 1000000000LL + st.st_mtimensec;
#endif
}

#endif

DyibiccContext* dyibicc_set_environment(DyibiccEnviromentData* env_data) {
  // Clone env_data into allocated ctx

  size_t total_include_paths_len = 0;
  size_t num_include_paths = 0;
  for (const char** p = env_data->include_paths; *p; ++p) {
    total_include_paths_len += strlen(*p) + 1;
    ++num_include_paths;
  }
  size_t total_files_len = 0;
  size_t num_files = 0;
  for (const char** p = env_data->files; *p; ++p) {
    total_files_len += strlen(*p) + 1;
    ++num_files;
  }

  size_t total_size = sizeof(Context) + (num_include_paths * sizeof(char*)) +
                      (total_include_paths_len * sizeof(char)) + (num_files * sizeof(char*)) +
                      (sizeof(int64_t) * num_files) + (sizeof(bool) * num_files) + total_files_len +
                      (strlen(env_data->entry_point_name) + 1) +
                      (strlen(env_data->cache_directory) + 1);
  Context* data = malloc(total_size);
  memset(data, 0, total_size);

  data->entry_point = NULL;
  data->get_function_address = env_data->get_function_address;
  data->output_function = env_data->output_function;

  char* d = (char*)(&data[1]);

  data->num_include_paths = num_include_paths;
  data->include_paths = (char**)d;
  d += sizeof(char*) * num_include_paths;

  data->num_files = num_files;
  data->last_file_timestamps = (int64_t*)d;
  d += sizeof(int64_t) * num_files;
  data->files = (char**)d;
  d += sizeof(char*) * num_files;
  data->up_to_date = (bool*)d;
  d += sizeof(bool) * num_files;

  data->entry_point_name = d;
  strcpy(d, env_data->entry_point_name);
  d += strlen(env_data->entry_point_name) + 1;

  data->cache_directory = d;
  strcpy(d, env_data->cache_directory);
  d += strlen(env_data->cache_directory) + 1;

  int i = 0;
  for (const char** p = env_data->include_paths; *p; ++p) {
    data->include_paths[i++] = d;
    strcpy(d, *p);
    d += strlen(*p) + 1;
  }

  i = 0;
  for (const char** p = env_data->files; *p; ++p) {
    data->files[i++] = d;
    strcpy(d, *p);
    d += strlen(*p) + 1;
  }

  assert((size_t)(d - (char*)data) == total_size);

  return (DyibiccContext*)data;
}

static bool do_subprocess_compile(Context* ctx, size_t file_index) {
  (void)ctx;
  (void)file_index;

  // Shell to self-compile each .c by doing:
  //   --run-as-dyibicc $cache_directory/x.c.subinvoke
  //
  // args.txt:
  // -ename
  // -Iblah
  // -Izippy
  // -L$cache_directory/x.c.log
  // -o$cache_directory/x.dyo
  // x.c

  // read x.c.log and use output_function to display it

  return true;
}

bool dyibicc_update(DyibiccContext* context) {
  Context* ctx = (Context*)context;

  // Check timestamp of all .c for now. Not really enough, but workable for now.
  for (size_t i = 0; i < ctx->num_files; ++i) {
    int64_t ts = stat_single_file(ctx->files[i]);
    if (ctx->last_file_timestamps[i] <= 0 || ts != ctx->last_file_timestamps[i]) {
      ctx->up_to_date[i] = do_subprocess_compile(ctx, i);
    }
  }

  // If any .c failed, bail now.
  for (size_t i = 0; i < ctx->num_files; ++i) {
    if (!ctx->up_to_date[i]) {
      return false;
      break;
    }
  }

  // Otherwise, carefully relink in process.
  // TODO: need maps of global and per-dyo data segment to not be replaced.

  return false;
}

void dyibicc_free(DyibiccContext* ctx) {
  free(ctx);
}

int main(void) {
  const char* include_paths[] = {
      "c:\\src\\rideau\\src\\shared",
      "c:\\src\\rideau\\src\\sim",
      NULL,
  };
  const char* files[] = {
      "c:\\src\\rideau\\src\\sim\\entry.c",
      NULL,
  };
  DyibiccEnviromentData cc_env_data = {.include_paths = include_paths,
                                       .files = files,
                                       .entry_point_name = "EntryPoint_RunSim",
                                       .cache_directory = "dyibicc_tmp",
                                       .get_function_address = NULL,
                                       .output_function = NULL};
  DyibiccContext* cc_ctx = dyibicc_set_environment(&cc_env_data);
  printf("%p\n", cc_ctx);
}
