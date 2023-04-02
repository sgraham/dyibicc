#include "dyibicc.h"

#ifdef _MSC_VER
#include <direct.h>
#endif

static char* current_symbol_name;
static int current_generation;
static HashMap* current_symbols;

void dyostore_init_new_generation(HashMap* active_symbols) {
  assert(active_symbols->global_alloc);

  current_symbols = active_symbols;

  ++current_generation;

  // Ensure dyostore/ exists.
#if _MSC_VER
  _mkdir("dyostore/");
#else
#error
#endif
}

FILE* dyostore_write_begin(char* filename, char* symbol_name, bool is_private) {
  assert(!current_symbol_name);
  assert(filename);
  assert(symbol_name);

  char* idname;
  if (is_private) {
    idname = format("dyostore/%s@@@%s.dyo", filename, symbol_name);
  } else {
    idname = format("dyostore/%s.dyo", symbol_name);
  }
  for (char* p = strchr(idname, '/') + 1; *p; ++p) {
    if (*p == ':')
      *p = '_';
    if (*p == '\\')
      *p = '_';
    if (*p == '/')
      *p = '_';
  }

  FILE* f = fopen(idname, "wb");
  if (!f) {
    fprintf(stderr, "couldn't open '%s'\n", idname);
    abort();
  }

  current_symbol_name = idname;

  return f;
}

void dyostore_write_finalize(void) {
#ifdef _MSC_VER
#define strdup _strdup
#endif
  hashmap_put(current_symbols, strdup(current_symbol_name), (void*)(intptr_t)current_generation);

  current_symbol_name = NULL;
}

int dyostore_current_generation(void) {
  return current_generation;
}

FILE* dyostore_read_open(char* name) {
  FILE* f = fopen(name, "rb");
  if (!f) {
    fprintf(stderr, "couldn't open '%s'\n", name);
    abort();
  }

  return f;
}

void dyostore_read_close(FILE* f) {
  assert(f);
  fclose(f);
}
