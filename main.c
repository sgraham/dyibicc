#ifdef _MSC_VER
__declspec(dllexport) int __cdecl printf(const char *_Format,...);
__declspec(dllexport) void __cdecl exit(int _Code);
#endif

#include "chibicc.h"

StringArray include_paths;

char *base_file;

static StringArray input_paths;

static void add_default_include_paths(char *argv0) {
  // We expect that chibicc-specific include files are installed
  // to ./include relative to argv[0].
  strarray_push(&include_paths, format("%s/include", dirname(strdup(argv0))));

#ifdef _MSC_VER
  // Can't easily use
  // "C:\\Program Files (x86)\\Windows Kits\\10\\Include\\10.0.19041.0\\ucrt" 
  // or
  // "C:\\Program Files\\Microsoft Visual Studio\\2022\\Community\\VC\\Tools\\MSVC\\14.34.31933\\include"
  // because things like va_arg assumes Windows ABI.
#else
  // Add standard include paths.
  strarray_push(&include_paths, "/usr/local/include");
  strarray_push(&include_paths, "/usr/include/x86_64-linux-gnu");
  strarray_push(&include_paths, "/usr/include");
#endif
}

static void usage(int status) {
  fprintf(stderr, "chibicc [-I <path>] <file0> [<file1>...]\n");
  exit(status);
}

static bool take_arg(char *arg) {
  char* x[] = {
      "-I",
  };

  for (int i = 0; i < sizeof(x) / sizeof(*x); i++)
    if (!strcmp(arg, x[i]))
      return true;
  return false;
}

static void parse_args(int argc, char **argv) {
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

static Token *must_tokenize_file(char *path) {
  Token *tok = tokenize_file(path);
  if (!tok)
    error("%s: %s", path, strerror(errno));
  return tok;
}

static FILE *open_file(char *path) {
  if (!path || strcmp(path, "-") == 0)
    return stdout;

  FILE *out = fopen(path, "wb");
  if (!out)
    error("cannot open output file: %s: %s", path, strerror(errno));
  return out;
}

// Replace file extension
static char *replace_extn(char *tmpl, char *extn) {
  char *filename = basename(strdup(tmpl));
  char *dot = strrchr(filename, '.');
  if (dot)
    *dot = '\0';
  return format("%s%s", filename, extn);
}

int main(int argc, char **argv) {
  bumpcalloc_init();
  parse_args(argc, argv);

  // TODO: Can't use a strarray because it'll get purged.
  FILE* dyo_files[32] = {0};
  int num_dyo_files = 0;

  for (int i = 0; i < input_paths.len; i++) {
    purge_and_reset_all(argc, argv);
    base_file = input_paths.data[i];
    char* output_file = replace_extn(base_file, ".s");
    char* dyo_output_file = replace_extn(base_file, ".dyo");

    //char *input = input_paths.data[i];
    //fprintf(stderr, "DYNASM: %s\n", input);

    Token *tok = must_tokenize_file(base_file);
    tok = preprocess(tok);

    codegen_init();  // Initializes dynasm so that parse() can assign labels.

    Obj *prog = parse(tok);

    //printf("opening %s\n", output_file);
    //printf("opening %s\n", dyo_output_file);
    FILE* out = open_file(output_file);
    FILE* dyo_out = open_file(dyo_output_file);
    codegen(prog, out, dyo_out);
    fclose(out);
    fclose(dyo_out);

    dyo_files[num_dyo_files++] = fopen(dyo_output_file, "rb");
    //fprintf(stderr, "%s = %p\n", dyo_output_file, dyo_files[num_dyo_files - 1]);

    // XXX
    // - need a subprocess_run for dynasm to reset compiler state (maybe manual
    // reset is better for figuring out what's actually required)
    // - but then need to figure out globals allocated pointed to by
    // jitted code, as well as inter-module references which will be gone
    // - can't just concat all files (though it might work for tests?) as there
    // will be various duplication
    // - partially serialize jitted state?
    // - or probably:
    //  - jitted code for each module
    //  - map of "required name" to fixup address (maybe all mov64?) so that
    //  the "link" can rip through each module and write in the correct address
    //  to the mov64
    //  - need to deal with the "Relocation" .data entries
    //    - is it multiple pass or is one sufficient?
    //    - i think it's single because each module can allocate a data segment
    //    and Relocation is just a pointer to another global (possibly
    //    in a different module)
    //  - need to deal with bss (probably easy)
    //  - i think the jitted code is position-independent other than the various
    //  globals we generate, so in theory it could jit to a plain block of
    //  memory and then the janky .obj format could just be:
    //  - raw x64 bytes to be mmap'd +x
    //  - string => list<void*>
    //
    //  iwbn to add some simple structure that allowed easy function replacement
    //  with just a cache and relink, e.g.:
    //  updatable map:
    //  - function name -> code bytes
    //                  -> sha1 of code
    //                  -> string => list<void*> fixups required
    //  - data name -> initial value
    //
    //  - initial boot loads all code/data
    //  - add entry to import table for each function (need anything for data?)
    //  - update means:
    //    - here's a new function data block
    //    - if the name exists and hash is different:
    //      - get it into executable mem
    //      - patch import table
    //      - discard old block
    //      - free +x mem for old block
    //    - could also just do full relink of references instead of import table
    //  - this should be an entry in the saved inputs fiie, so start with
    //  the compiled code, update "bloop" at tick N, pressed Left and N+4, etc.
  }

  // XXX the dyo files currently contain resolved addresses to locals so they
  // have to be run here. function references should be
  // relocatable/serializable.

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
