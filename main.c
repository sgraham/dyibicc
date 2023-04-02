// Notes and todos
// ---------------
//
// Windows x64 .pdata generation:
//
//   Need to RtlAddFunctionTable() so that even minimal stackwalking in
//   Disassembly view is correct in VS. Required for SEH too. cl /Fa emits
//   without using any helper macros for samples.
//
// Break up into small symbol-sized 'sections':
//
//   In order to update code without losing global state, need to be able to
//   replace and relink. Right now, link_dyos() does all the allocation of
//   global data at the same time as mapping the code in to executable pages.
//
//   The simplest fix would be to keep the mappings of globals around and not
//   reallocate them on updates (one map for global symbols, plus one per
//   translation unit). The code updating could still be tossing all code, and
//   relinking everything, but using the old hashmaps for data addresses.
//
//   Alternatively, it might be a better direction to break everything up into
//   symbol-sized chunks (i.e. either a variable or a function indexed by symbol
//   name). Initial and update become more similar, in that if any symbol is
//   updated, the old one (if any) gets thrown away, the new one gets mapped in
//   (whether code or data), and then everything that refers to it is patched.
//
//   The main gotchas that come to mind on the second approach are:
//
//     - The parser (and DynASM to assign labels) need to be initialized before
//     processing the whole file; C is just never going to be able to compile a
//     single function in isolation. So emit_data() and emit_text() need to make
//     sure that each symbol blob can be ripped out of the generated block, and
//     any offsets have to be saved relative to the start of that symbol for
//     emitting fixups. Probably codegen_pclabel() a start/end for each for
//     rippage/re-offseting.
//
//     - Need figure out how to name things. If everything becomes a flat bag of
//     symbols, we need to make sure that statics from a.c are local to a.c, so
//     they'll need to be file prefixed.
//
//     - Probably wll need to switch to a new format (some kv store or
//     something), as symbol-per-dyo would be a spamming of files to deal with.
//
// Large allocas aren't _chkstk'ing on Windows:
//
//   Will STACK_OVERFLOW if it jumps past the guard page.
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
// rep stosb for local clear:
//
//   Especially on Windows where rdi is non-volatile, it seems like quite a lot
//   of instructions. At the very least we could only do one memset for all
//   locals to clear out a range.
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
// Consider merging some of the record types in dyo:
//
//     kTypeImport is (offset-to-fix, string-to-reference)
//     kTypeCodeReferenceToGlobal is (offset-to-fix, string-to-reference)
//     kTypeInitializerDataRelocation is (string-to-reference, addend)
//
//   The only difference between the first two is that one does GetProcAddress()
//   or similar, and the other looks in the export tables for other dyos. But we
//   might want data imported from host too.
//
//   The third is different in that the address to fix up is implicit because
//   it's in a sequence of data segment initializers, but just having all
//   imports be:
//
//      (offset-to-fix, string-to-reference, addend)
//
//   might be nicer.
//
//
#include "dyibicc.h"

StringArray include_paths;

char* base_file;

static StringArray input_paths;
static bool opt_E = false;

static void add_default_include_paths(char* argv0) {
#if X64WIN
  strarray_push(&include_paths, format("%s/include/win", dirname(bumpstrdup(argv0))));
  strarray_push(&include_paths, format("%s/include/all", dirname(bumpstrdup(argv0))));

  strarray_push(&include_paths,
                "C:\\Program Files (x86)\\Windows Kits\\10\\Include\\10.0.19041.0\\ucrt");
  strarray_push(&include_paths,
                "C:\\Program Files\\Microsoft Visual "
                "Studio\\2022\\Community\\VC\\Tools\\MSVC\\14.34.31933\\include");
#else
  strarray_push(&include_paths, format("%s/include/linux", dirname(bumpstrdup(argv0))));
  strarray_push(&include_paths, format("%s/include/all", dirname(bumpstrdup(argv0))));

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

  for (size_t i = 0; i < sizeof(x) / sizeof(*x); i++)
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
  char* filename = basename(bumpstrdup(tmpl));
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

void* compile_and_link(int argc, char** argv, HashMap* active_symbols) {
  bumpcalloc_init();
  parse_args(argc, argv);

  // bumpalloc is cleared after each input path, so this HashMap must globally
  // allocate to have them remaining during link.
  assert(active_symbols->global_alloc);

  for (int i = 0; i < input_paths.len; i++) {
    purge_and_reset_all(argc, argv);
    base_file = input_paths.data[i];

    Token* tok = must_tokenize_file(base_file);
    tok = preprocess(tok);

    // If -E is given, print out preprocessed C code as a result.
    if (opt_E) {
      print_tokens(tok);
      continue;
    }

    codegen_init();  // Initializes dynasm so that parse() can assign labels.

    Obj* prog = parse(tok);

    codegen(prog);
  }

  if (opt_E)
    return 0;

  return link_dyos(active_symbols);
}
