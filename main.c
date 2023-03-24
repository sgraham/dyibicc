#include "dyibicc.h"

StringArray include_paths;

char* base_file;

static StringArray input_paths;

static void add_default_include_paths(char* argv0) {
  // We expect that dyibicc-specific include files are installed
  // to ./include relative to argv[0].
  strarray_push(&include_paths, format("%s/include", dirname(strdup(argv0))));

#ifdef _MSC_VER
  // Can't easily use
  // "C:\\Program Files (x86)\\Windows Kits\\10\\Include\\10.0.19041.0\\ucrt"
  // or
  // "C:\\Program Files\\Microsoft Visual
  // Studio\\2022\\Community\\VC\\Tools\\MSVC\\14.34.31933\\include" because things like va_arg
  // assumes Windows ABI.
#else
  // Add standard include paths.
  strarray_push(&include_paths, "/usr/local/include");
  strarray_push(&include_paths, "/usr/include/x86_64-linux-gnu");
  strarray_push(&include_paths, "/usr/include");
#endif
}

static void usage(int status) {
  fprintf(stderr, "dyibicc [-I <path>] <file0> [<file1>...]\n");
  exit(status);
}

static bool take_arg(char* arg) {
  char* x[] = {
      "-I",
  };

  for (int i = 0; i < sizeof(x) / sizeof(*x); i++)
    if (!strcmp(arg, x[i]))
      return true;
  return false;
}

static void parse_args(int argc, char** argv) {
  for (int i = 1; i < argc; i++)
    if (take_arg(argv[i]))
      if (!argv[++i])
        usage(1);

  for (int i = 1; i < argc; i++) {
    if (!strncmp(argv[i], "-I", 2)) {
      strarray_push(&include_paths, argv[i] + 2);
      continue;
    }

    if (!strcmp(argv[i], "--help"))
      usage(0);

    if (argv[i][0] == '-' && argv[i][1] != '\0')
      error("unknown argument: %s", argv[i]);

    strarray_push(&input_paths, argv[i]);
  }

  if (input_paths.len == 0)
    error("no input files");
}

// This attempts to blast all static variables back to zero-initialized and
// clears all memory that was calloc'd in a previous iteration of the compiler.
// All previously allocated pointers become invalidated. Command line arguments
// are reparsed because of this, and will be identical to the last time.
void purge_and_reset_all(int argc, char* argv[]) {
  tokenize_reset();
  preprocess_reset();
  parse_reset();
  codegen_reset();
  bumpcalloc_reset();
  input_paths = (StringArray){NULL, 0, 0};
  include_paths = (StringArray){NULL, 0, 0};
  base_file = NULL;

  bumpcalloc_init();
  init_macros();

  parse_args(argc, argv);
  add_default_include_paths(argv[0]);
}

static Token* must_tokenize_file(char* path) {
  Token* tok = tokenize_file(path);
  if (!tok)
    error("%s: %s", path, strerror(errno));
  return tok;
}

static FILE* open_file(char* path) {
  if (!path || strcmp(path, "-") == 0)
    return stdout;

  FILE* out = fopen(path, "wb");
  if (!out)
    error("cannot open output file: %s: %s", path, strerror(errno));
  return out;
}

// Replace file extension
static char* replace_extn(char* tmpl, char* extn) {
  char* filename = basename(strdup(tmpl));
  char* dot = strrchr(filename, '.');
  if (dot)
    *dot = '\0';
  return format("%s%s", filename, extn);
}

int main(int argc, char** argv) {
  bumpcalloc_init();
  parse_args(argc, argv);

  // TODO: Can't use a strarray because it'll get purged.
  FILE* dyo_files[32] = {0};
  int num_dyo_files = 0;

  for (int i = 0; i < input_paths.len; i++) {
    purge_and_reset_all(argc, argv);
    base_file = input_paths.data[i];
    char* dyo_output_file = replace_extn(base_file, ".dyo");

    Token* tok = must_tokenize_file(base_file);
    tok = preprocess(tok);

    codegen_init();  // Initializes dynasm so that parse() can assign labels.

    Obj* prog = parse(tok);

    FILE* dyo_out = open_file(dyo_output_file);
    codegen(prog, dyo_out);
    fclose(dyo_out);

    dyo_files[num_dyo_files++] = fopen(dyo_output_file, "rb");
  }

  void* entry_point = link_dyos(dyo_files);
  if (entry_point) {
    int (*p)() = (int (*)())entry_point;
    int result = p();
    printf("main returned: %d\n", result);
    return result;
  } else {
    fprintf(stderr, "no entry point found\n");
    return 1;
  }
}
