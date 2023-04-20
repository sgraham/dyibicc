#include "dyibicc.h"

static void usage(int status) {
  printf("dyibicc [-E] [-e symbolname] [-I <path>] <file0> [<file1>...]\n");
  exit(status);
}

static bool take_arg(char* arg) {
  char* x[] = {
      "-I",
      "-e",
  };

  for (size_t i = 0; i < sizeof(x) / sizeof(*x); i++)
    if (!strcmp(arg, x[i]))
      return true;
  return false;
}

// Returns the contents of a given file. Doesn't support '-' for reading from
// stdin.
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

static void parse_args(int argc,
                       char** argv,
                       char** entry_point_override,
                       StringArray* include_paths,
                       StringArray* input_paths) {
  for (int i = 1; i < argc; i++)
    if (take_arg(argv[i]))
      if (!argv[++i])
        usage(1);

  for (int i = 1; i < argc; i++) {
    if (!strncmp(argv[i], "-I", 2)) {
      strarray_push(include_paths, argv[i] + 2, AL_Link);
      continue;
    }

    if (!strncmp(argv[i], "-e", 2)) {
      *entry_point_override = argv[i] + 2;
      continue;
    }

    if (!strcmp(argv[i], "--help"))
      usage(0);

    if (argv[i][0] == '-' && argv[i][1] != '\0')
      error("unknown argument: %s", argv[i]);

    strarray_push(input_paths, argv[i], AL_Link);
  }

  if (input_paths->len == 0)
    error("no input files");
}

int main(int argc, char** argv) {
#if X64WIN
  // All dyo files are open during link, so we hack this (the maximum)
  // to support some tests with a lot of files. dyo files should
  // probably go away later.
  _setmaxstdio(8192);
#endif

  alloc_init(AL_Link);

  StringArray include_paths = {0};
  StringArray input_paths = {0};
  char* entry_point_override = "main";
  parse_args(argc, argv, &entry_point_override, &include_paths, &input_paths);
  strarray_push(&include_paths, NULL, AL_Link);
  strarray_push(&input_paths, NULL, AL_Link);

  DyibiccEnviromentData env_data = {
      .include_paths = (const char**)include_paths.data,
      .files = (const char**)input_paths.data,
      .dyibicc_include_dir = "./include",
      .load_file_contents = read_file,
      .get_function_address = NULL,
      .output_function = NULL,
      .use_ansi_codes = isatty(fileno(stdout)),
  };

  DyibiccContext* ctx = dyibicc_set_environment(&env_data);

  alloc_reset(AL_Link);

  int result = 0;

  if (dyibicc_update(ctx, NULL, NULL)) {
    void* entry_point = dyibicc_find_export(ctx, entry_point_override);
    if (entry_point) {
      int myargc = 1;
      char* myargv[] = {"prog", NULL};
      result = ((int (*)(int, char**))entry_point)(myargc, myargv);
    } else {
      printf("no entry point found\n");
      result = 254;
    }
  } else {
    result = 255;
  }

  dyibicc_free(ctx);

  return result;
}
