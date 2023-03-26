// Notes and todos
// ---------------
//
// Windows x64 .pdata generation:
//
//   Need to RtlAddFunctionTable() so that even minimal stackwalking in
//   Disassembly view is correct in VS. Required for SEH too. cl /Fa emits
//   without using any helper macros for samples.
//
// Parsing windows.h:
//
//   We make it through CRT, but windows.h will have lots more wild stuff.
//   __declspecs and packing pragmas are currently ignored and will be necessary
//   for passing structures to winapi.
//
// Debugger:
//
//   Picking either ELF/DWARF or PE/COFF (and dropping .dyo) would probably be
//   the more practical way to get a better debugging experience, but then,
//   clang-win would also be a lot better. Tomorrow Corp demo for inspiration of
//   what needs to be implemented/included. Possibly still go with debug adapter
//   json thing (with extension messages?) so that an existing well-written UI
//   can be used.
//
// Improved codegen:
//
//   Bit of a black hole of effort and probably doesn't matter for a dev-focused
//   tool. But it would be easier to trace through asm if the data flow was less
//   hidden. Possibly basic use of otherwise-unused gp registers, possibly some
//   peephole, or higher level amalgamated instructions for codegen to use that
//   avoid the common cases of load/push, push/something/pop.
//
// Various "C+" language extensions:
//
//   Some possibilities:
//     - an import instead of #include that can be used when not touching system
//     stuff
//     - string type with syntax integration
//     - basic polymophic containers (dict, list, slice, sizedarray)
//     - range-based for loop (to go with containers)
//     - range notation
//
// Don't emit __func__, __FUNCTION__ unless used:
//
//   Doesn't affect anything other than dyo size, but it bothers me seeing them
//   in there.
//
// Improve dumpdyo:
//
//   - Cross-reference the name to which fixups will be bound in disasm
//   - include dump as string for initializer bytes
//
// Implement TLS:
//
//   If needed.
//
// Implement inline ASM:
//
//   If needed.
//
// .dyo cache:
//
//   Based on compiler binary, "environment", and the contents of the .c file,
//   make a hash-based cache of dyos so that recompile can only build the
//   required files and relink while passing the whole module/program still.
//   Since there's no -D or other flags, "enviroment" could either be a hash of
//   all the files in the include search path, or alternatively hash after
//   preprocessing, or probably track all files include and include all of the
//   includes in the hash. Not overly important if total compile/link times
//   remain fast.
//
// In-memory dyo:
//
//   Alternatively to caching, maybe just save to a memory structure. Might be a
//   little faster for direct use, could still have a dump-dyo-from-mem for
//   debugging purposes. Goes more with an always-live compiler host hooked to
//   target.
//
#include "dyibicc.h"

StringArray include_paths;

char* base_file;

static StringArray input_paths;
static bool opt_E = false;

static void add_default_include_paths(char* argv0) {
#if X64WIN
  strarray_push(&include_paths, format("%s/include/win", dirname(strdup(argv0))));
  strarray_push(&include_paths, format("%s/include/all", dirname(strdup(argv0))));

  strarray_push(&include_paths,
                "C:\\Program Files (x86)\\Windows Kits\\10\\Include\\10.0.19041.0\\ucrt");
  strarray_push(&include_paths,
                "C:\\Program Files\\Microsoft Visual "
                "Studio\\2022\\Community\\VC\\Tools\\MSVC\\14.34.31933\\include");
#else
  strarray_push(&include_paths, format("%s/include/linux", dirname(strdup(argv0))));
  strarray_push(&include_paths, format("%s/include/all", dirname(strdup(argv0))));

  // Add standard include paths.
  strarray_push(&include_paths, "/usr/local/include");
  strarray_push(&include_paths, "/usr/include/x86_64-linux-gnu");
  strarray_push(&include_paths, "/usr/include");
#endif
}

static void usage(int status) {
  fprintf(stderr, "dyibicc [-E] [-I <path>] <file0> [<file1>...]\n");
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

    if (!strcmp(argv[i], "-E")) {
      opt_E = true;
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
  bumpcalloc_reset();
  codegen_reset();
  link_reset();
  parse_reset();
  preprocess_reset();
  tokenize_reset();
  input_paths = (StringArray){NULL, 0, 0};
  include_paths = (StringArray){NULL, 0, 0};
  opt_E = false;
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

static void print_tokens(Token* tok) {
  int line = 1;
  for (; tok->kind != TK_EOF; tok = tok->next) {
    if (line > 1 && tok->at_bol)
      fprintf(stdout, "\n");
    if (tok->has_space && !tok->at_bol)
      fprintf(stdout, " ");
    fprintf(stdout, "%.*s", tok->len, tok->loc);
    line++;
  }
  fprintf(stdout, "\n");
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

    // If -E is given, print out preprocessed C code as a result.
    if (opt_E) {
      print_tokens(tok);
      continue;
    }

    codegen_init();  // Initializes dynasm so that parse() can assign labels.

    Obj* prog = parse(tok);

    FILE* dyo_out = open_file(dyo_output_file);
    codegen(prog, dyo_out);
    fclose(dyo_out);

    dyo_files[num_dyo_files++] = fopen(dyo_output_file, "rb");
  }

  if (opt_E)
    return 0;

  void* entry_point = link_dyos(dyo_files);
  if (entry_point) {
    int (*p)() = (int (*)())entry_point;
    int result = p();
    printf("main returned: %d\n", result);
    return result;
  } else {
    fprintf(stderr, "link failed or no entry point found\n");
    return 1;
  }
}
