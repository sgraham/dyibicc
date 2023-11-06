#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "libdyibicc.h"

static bool read_file(const char* path, char** contents, size_t* size) {
  FILE* fp = fopen(path, "rb");
  if (!fp) {
    return false;
  }

  fseek(fp, 0, SEEK_END);
  *size = ftell(fp);
  rewind(fp);
  *contents = malloc(*size);
  fread(*contents, 1, *size, fp);
  fclose(fp);
  return true;
}

int LLVMFuzzerTestOneInput(const uint8_t* Data, size_t Size) {
  const char* no_include_paths[] = {NULL};
  const char* one_input_path[] = {"fuzz.c", NULL};
  DyibiccEnviromentData env_data = {
      .include_paths = no_include_paths,
      .files = one_input_path,
      .load_file_contents = read_file,
      .get_function_address = NULL,
      .output_function = NULL,
      .use_ansi_codes = false,
  };

  DyibiccContext* ctx = dyibicc_set_environment(&env_data);

  char* data_copy = malloc(Size + 1);
  memcpy(data_copy, Data, Size);
  data_copy[Size] = 0;
  dyibicc_update(ctx, "fuzz.c", data_copy);
  dyibicc_free(ctx);

  return 0;
}
