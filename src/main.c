#include "dyibicc.h"

#if X64WIN
#include <direct.h>
#endif

#define C(x) compiler_state.main__##x
#define L(x) linker_state.main__##x

#if 0  // for -E call after preprocess().
static void print_tokens(Token* tok) {
  int line = 1;
  for (; tok->kind != TK_EOF; tok = tok->next) {
    if (line > 1 && tok->at_bol)
      logout("\n");
    if (tok->has_space && !tok->at_bol)
      logout(" ");
    logout("%.*s", tok->len, tok->loc);
    line++;
  }
  logout("\n");
}
#endif

static int default_output_fn(const char* fmt, va_list ap) {
  int ret = vfprintf(stdout, fmt, ap);
  return ret;
}

static bool default_load_file_fn(const char* path, char** contents, size_t* size) {
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

DyibiccContext* dyibicc_set_environment(DyibiccEnviromentData* env_data) {
  // Set this up with a temporary value early, mostly so we can ABORT() below
  // with output if necessary.
  UserContext tmp_user_context = {0};
  tmp_user_context.output_function = default_output_fn;
  tmp_user_context.load_file_contents = default_load_file_fn;
  user_context = &tmp_user_context;

  alloc_init(AL_Temp);

  // Clone env_data into allocated ctx

  size_t total_include_paths_len = 0;
  size_t num_include_paths = 0;
  for (const char** p = env_data->include_paths; *p; ++p) {
    total_include_paths_len += strlen(*p) + 1;
    ++num_include_paths;
  }

  StringArray sys_inc_paths = {0};
#if X64WIN
  strarray_push(&sys_inc_paths, format(AL_Temp, "%s/win", env_data->dyibicc_include_dir), AL_Temp);
  strarray_push(&sys_inc_paths, format(AL_Temp, "%s/all", env_data->dyibicc_include_dir), AL_Temp);

#define GET_ENV_VAR(x) \
  char* env_##x = getenv(#x); \
  if (!env_##x) { \
    ABORT("environment variable " #x " unset"); \
  }

  GET_ENV_VAR(WindowsSdkDir);
  GET_ENV_VAR(WindowsSdkLibVersion);
  GET_ENV_VAR(VcToolsInstallDir);
#undef GET_ENV_VAR

  strarray_push(&sys_inc_paths,
                format(AL_Temp, "%sInclude\\%sucrt", env_WindowsSdkDir, env_WindowsSdkLibVersion),
                AL_Temp);
  strarray_push(&sys_inc_paths,
                format(AL_Temp, "%sInclude\\%sum", env_WindowsSdkDir, env_WindowsSdkLibVersion),
                AL_Temp);
  strarray_push(&sys_inc_paths,
                format(AL_Temp, "%sInclude\\%sshared", env_WindowsSdkDir, env_WindowsSdkLibVersion),
                AL_Temp);
  strarray_push(&sys_inc_paths, format(AL_Temp, "%sinclude", env_VcToolsInstallDir), AL_Temp);

#else
  strarray_push(&sys_inc_paths, format(AL_Temp, "%s/linux", env_data->dyibicc_include_dir),
                AL_Temp);
  strarray_push(&sys_inc_paths, format(AL_Temp, "%s/all", env_data->dyibicc_include_dir), AL_Temp);

  strarray_push(&sys_inc_paths, "/usr/local/include", AL_Temp);
  strarray_push(&sys_inc_paths, "/usr/include/x86_64-linux-gnu", AL_Temp);
  strarray_push(&sys_inc_paths, "/usr/include", AL_Temp);
#endif

  for (int i = 0; i < sys_inc_paths.len; ++i) {
    total_include_paths_len += strlen(sys_inc_paths.data[i]) + 1;
    ++num_include_paths;
  }

  // Don't currently need dyibicc_include_dir once sys_inc_paths are added to.

  size_t total_source_files_len = 0;
  size_t num_files = 0;
  for (const char** p = env_data->files; *p; ++p) {
    total_source_files_len += strlen(*p) + 1;
    ++num_files;
  }

  size_t total_size =
      sizeof(UserContext) +                       // base structure
      (num_include_paths * sizeof(char*)) +       // array in base structure
      (num_files * sizeof(FileLinkData)) +        // array in base structure
      (total_include_paths_len * sizeof(char)) +  // pointed to by include_paths
      (total_source_files_len * sizeof(char)) +   // pointed to by FileLinkData.source_name
      ((num_files + 1) * sizeof(HashMap)) +       // +1 beyond num_files for fully global dataseg
      ((num_files + 1) * sizeof(HashMap))         // +1 beyond num_files for fully global exports
      ;

  UserContext* data = calloc(1, total_size);

  data->load_file_contents = env_data->load_file_contents;
  if (!data->load_file_contents) {
    data->load_file_contents = default_load_file_fn;
  }
  data->get_function_address = env_data->get_function_address;
  data->output_function = env_data->output_function;
  if (!data->output_function) {
    data->output_function = default_output_fn;
  }
  data->use_ansi_codes = env_data->use_ansi_codes;
  data->generate_debug_symbols = env_data->generate_debug_symbols;

  char* d = (char*)(&data[1]);

  data->num_include_paths = num_include_paths;
  data->include_paths = (char**)d;
  d += sizeof(char*) * num_include_paths;

  data->num_files = num_files;
  data->files = (FileLinkData*)d;
  d += sizeof(FileLinkData) * num_files;

  data->global_data = (HashMap*)d;
  d += sizeof(HashMap) * (num_files + 1);

  data->exports = (HashMap*)d;
  d += sizeof(HashMap) * (num_files + 1);

  int i = 0;
  for (const char** p = env_data->include_paths; *p; ++p) {
    data->include_paths[i++] = d;
    strcpy(d, *p);
    d += strlen(*p) + 1;
  }
  for (int j = 0; j < sys_inc_paths.len; ++j) {
    data->include_paths[i++] = d;
    strcpy(d, sys_inc_paths.data[j]);
    d += strlen(sys_inc_paths.data[j]) + 1;
  }

  i = 0;
  for (const char** p = env_data->files; *p; ++p) {
    FileLinkData* dld = &data->files[i++];
    dld->source_name = d;
    strcpy(dld->source_name, *p);
    d += strlen(*p) + 1;
  }

  // These maps store an arbitrary number of symbols, and they must persist
  // beyond AL_Link (to be saved for relink updates) so they must be manually
  // managed.
  for (size_t j = 0; j < num_files + 1; ++j) {
    data->global_data[j].alloc_lifetime = AL_Manual;
    data->exports[j].alloc_lifetime = AL_Manual;
  }
  data->reflect_types.alloc_lifetime = AL_UserContext;

  if ((size_t)(d - (char*)data) != total_size) {
    ABORT("incorrect size calculation");
  }

  user_context = data;
  alloc_reset(AL_Temp);
  alloc_init(AL_UserContext);
  return (DyibiccContext*)data;
}

void dyibicc_free(DyibiccContext* context) {
  UserContext* ctx = (UserContext*)context;
  assert(ctx == user_context && "only one context currently supported");
  for (size_t i = 0; i < ctx->num_files + 1; ++i) {
    hashmap_clear_manual_key_owned_value_owned_aligned(&ctx->global_data[i]);
    hashmap_clear_manual_key_owned_value_unowned(&ctx->exports[i]);
  }
  alloc_reset(AL_UserContext);

  for (size_t i = 0; i < ctx->num_files; ++i) {
    free_link_fixups(&ctx->files[i]);
  }
#if X64WIN
  unregister_and_free_function_table_data(ctx);
#endif
  free(ctx);
  user_context = NULL;
}

bool dyibicc_update(DyibiccContext* context, char* filename, char* contents) {
  if (setjmp(toplevel_update_jmpbuf) != 0) {
    codegen_free();
    alloc_reset(AL_Compile);
    alloc_reset(AL_Temp);
    alloc_reset(AL_Link);
    memset(&compiler_state, 0, sizeof(compiler_state));
    memset(&linker_state, 0, sizeof(linker_state));
    return false;
  }

  UserContext* ctx = (UserContext*)context;
  bool link_result = true;

  assert(ctx == user_context && "only one context currently supported");

  bool compiled_any = false;
  {
    for (size_t i = 0; i < ctx->num_files; ++i) {
      FileLinkData* dld = &ctx->files[i];

      if (filename && strcmp(dld->source_name, filename) != 0) {
        // If a specific update is provided, we only compile that one.
        continue;
      }

      {
        alloc_init(AL_Compile);

        init_macros();
        C(base_file) = dld->source_name;
        Token* tok;
        if (filename) {
          tok = tokenize_filecontents(filename, contents);
        } else {
          tok = tokenize_file(C(base_file));
        }
        if (!tok)
          error("%s: %s", C(base_file), strerror(errno));
        tok = preprocess(tok);

        codegen_init();  // Initializes dynasm so that parse() can assign labels.

        Obj* prog = parse(tok);
        codegen(prog, i);

        compiled_any = true;

        alloc_reset(AL_Compile);
      }
    }

    if (compiled_any) {
      alloc_init(AL_Link);

      link_result = link_all_files();

      alloc_reset(AL_Link);
    }
  }

  return link_result;
}

void* dyibicc_find_export(DyibiccContext* context, char* name) {
  UserContext* ctx = (UserContext*)context;
  return hashmap_get(&ctx->exports[ctx->num_files], name);
}
