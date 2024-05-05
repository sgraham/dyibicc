#include "dyibicc.h"

static void usage(int status) {
  printf("dyibicc [-e symbolname] [-I <path>] [-c] [-g] <file0> [<file1>...]\n");
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
                       bool* compile_only,
                       bool* debug_symbols,
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

    if (!strncmp(argv[i], "-c", 2)) {
      *compile_only = true;
      continue;
    }

    if (!strncmp(argv[i], "-g", 2)) {
      *debug_symbols = true;
      continue;
    }

    if (!strcmp(argv[i], "--help"))
      usage(0);

    if (argv[i][0] == '-' && argv[i][1] != '\0') {
      printf("unknown argument: %s\n", argv[i]);
      usage(1);
    }

    strarray_push(input_paths, argv[i], AL_Link);
  }

  if (input_paths->len == 0) {
    printf("no input files\n");
    usage(1);
  }
}

#if X64WIN
extern _Bool __stdcall SetConsoleMode(void*, int);
extern void* __stdcall GetStdHandle(int);
#endif

int main(int argc, char** argv) {
#if X64WIN
  SetConsoleMode(GetStdHandle(-11), 7);
#endif

  alloc_init(AL_Link);

  StringArray include_paths = {0};
  StringArray input_paths = {0};
  char* entry_point_override = "main";
  bool compile_only = false;
  bool debug_symbols = false;
  parse_args(argc, argv, &entry_point_override, &compile_only, &debug_symbols, &include_paths,
             &input_paths);
  strarray_push(&include_paths, NULL, AL_Link);
  strarray_push(&input_paths, NULL, AL_Link);

  DyibiccEnviromentData env_data = {
      .include_paths = (const char**)include_paths.data,
      .files = (const char**)input_paths.data,
      .load_file_contents = read_file,
      .get_function_address = NULL,
      .output_function = NULL,
      .use_ansi_codes = isatty(fileno(stdout)),
      .generate_debug_symbols = debug_symbols,
  };

  DyibiccContext* ctx = dyibicc_set_environment(&env_data);

  alloc_reset(AL_Link);

  int result = 0;

  if (dyibicc_update(ctx, NULL, NULL)) {
    void* entry_point = dyibicc_find_export(ctx, entry_point_override);
    if (entry_point) {
      if (compile_only) {
        // Only doing a syntax check, just return 0 without running if we made it this far.
        result = 0;
      } else {
        int myargc = 1;
        char* myargv[] = {"prog", NULL};
        result = ((int (*)(int, char**))entry_point)(myargc, myargv);
      }
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
