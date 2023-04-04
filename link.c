#include "dyibicc.h"

#if X64WIN
#include <malloc.h>
#include <windows.h>
#define alloca _alloca
#else
#include <dlfcn.h>
#include <unistd.h>
#endif

#define L(x) linker_state.link__##x

#include "khash.h"

KHASH_SET_INIT_INT64(voidp)

static DyibiccFunctionLookupFn user_runtime_function_callback = NULL;

void dyibicc_set_user_runtime_function_callback(DyibiccFunctionLookupFn f) {
  user_runtime_function_callback = f;
}

static void* get_standard_runtime_function(char* name) {
  if (L(runtime_function_map).capacity == 0) {
    L(runtime_function_map).alloc_lifetime = AL_Link;
#define X(func) hashmap_put(&L(runtime_function_map), #func, (void*)&func)
#if X64WIN
    X(CloseHandle);
    X(CreateThread);
    X(WaitForSingleObject);
    X(__acrt_iob_func);
    X(__stdio_common_vfprintf);
    X(__stdio_common_vfprintf_p);
    X(__stdio_common_vfprintf_s);
    X(__stdio_common_vfscanf);
    X(__stdio_common_vfwprintf);
    X(__stdio_common_vfwprintf_p);
    X(__stdio_common_vfwprintf_s);
    X(__stdio_common_vfwscanf);
    X(__stdio_common_vsnprintf_s);
    X(__stdio_common_vsnwprintf_s);
    X(__stdio_common_vsprintf);
    X(__stdio_common_vsprintf_p);
    X(__stdio_common_vsprintf_s);
    X(__stdio_common_vsscanf);
    X(__stdio_common_vswprintf);
    X(__stdio_common_vswprintf_p);
    X(__stdio_common_vswprintf_s);
    X(__stdio_common_vswscanf);
#endif
    X(exit);
    X(memcmp);
    X(memcpy);
    X(printf);
    X(sprintf);
    X(strcmp);
    X(strlen);
    X(strncmp);
    X(vsprintf);
#undef X
  }

  return hashmap_get(&L(runtime_function_map), name);
}

static void* symbol_lookup(char* name) {
  if (user_runtime_function_callback) {
    void* f = user_runtime_function_callback(name);
    if (f) {
      return f;
    }
  }

  void* f = get_standard_runtime_function(name);
  if (f) {
    return f;
  }
#if X64WIN
  return (void*)GetProcAddress(GetModuleHandle(NULL), name);
#else
  return dlsym(NULL, name);
#endif
}

bool link_dyos(void) {
  char buf[64 << 10];

  UserContext* uc = user_context;

#if X64WIN
  SYSTEM_INFO system_info;
  GetSystemInfo(&system_info);
  unsigned int page_size = system_info.dwPageSize;
#else
  unsigned int page_size = sysconf(_SC_PAGESIZE);
#endif

  if (uc->num_files == 0)
    return false;

  bool relinking = uc->files[0].codeseg_base_address != 0;

  if (relinking) {
    for (size_t i = 0; i < uc->num_files; ++i) {
      DyoLinkData* dld = &uc->files[i];
      free_executable_memory(dld->codeseg_base_address, dld->codeseg_size);
    }
  }

  // Tracks which data segment objects were created this update to know whether
  // to set them to their static initializers.
  khash_t(voidp)* created_this_update = kh_init(voidp);

  FILE** dyo_files = alloca(sizeof(FILE*) * uc->num_files);
  for (size_t i = 0; i < uc->num_files; ++i) {
    DyoLinkData* dld = &uc->files[i];
    dyo_files[i] = fopen(dld->output_dyo_name, "rb");
    if (!dyo_files[i]) {
      error("couldn't open '%s'", dld->output_dyo_name);
    }
  }

  int num_dyos = 0;
  // Map code blocks from each and save base address.
  // Allocate and global data and save address/size by name.
  for (size_t i = 0; i < uc->num_files; ++i) {
    FILE* dyo = dyo_files[i];
    if (!ensure_dyo_header(dyo))
      goto fail;

    unsigned int entry_point_offset = 0xffffffff;
    int record_index = 0;

    StringArray strings = {NULL, 0, 0};
    strarray_push(&strings, NULL, AL_Link);  // 1-based

    DyoLinkData* dld = &uc->files[num_dyos];
    for (;;) {
      unsigned int type;
      unsigned int size;
      if (!read_dyo_record(dyo, &record_index, buf, sizeof(buf), &type, &size))
        goto fail;

      if (type == kTypeString) {
        strarray_push(&strings, bumpstrndup(buf, size, AL_Link), AL_Link);
      } else {
        strarray_push(&strings, NULL, AL_Link);

        if (type == kTypeEntryPoint) {
          entry_point_offset = *(unsigned int*)&buf[0];
        } else if (type == kTypeX64Code) {
          unsigned int page_sized = (unsigned int)align_to_u(size, page_size);
          // logdbg("code %d, allocating %d\n", size, page_sized);
          dld->codeseg_base_address = allocate_writable_memory(page_sized);
          dld->codeseg_size = page_sized;
          memcpy(dld->codeseg_base_address, buf, size);
          if (entry_point_offset != 0xffffffff) {
            uc->entry_point = (DyibiccEntryPointFn)(dld->codeseg_base_address + entry_point_offset);
          }
          ++num_dyos;
          break;
        } else if (type == kTypeInitializedData) {
          unsigned int data_size = *(unsigned int*)&buf[0];
          unsigned int align = *(unsigned int*)&buf[4];
          unsigned int flags = *(unsigned int*)&buf[8];
          unsigned int name_index = *(unsigned int*)&buf[12];
          bool is_static = flags & 0x01;
          bool is_rodata = flags & 0x02;

          // Don't recreate non-rodata if relinking. TBD what data should be
          // recreated vs. preserved, maybe some sort of annotations.
          if (relinking) {
            size_t idx = is_static ? num_dyos : uc->num_files;
            void* prev = hashmap_get(&uc->global_data[idx], strings.data[name_index]);
            if (prev) {
              if (is_rodata) {
                aligned_free(prev);
              } else {
                continue;
              }
            }
          }

          void* global_data = aligned_allocate(data_size, align);
          memset(global_data, 0, data_size);

          // The keys need to be strdup'd to stick around for subsequent links.
          size_t idx = is_static ? num_dyos : uc->num_files;
          void* prev = hashmap_get(&uc->global_data[idx], strings.data[name_index]);
          if (prev) {
            logerr("duplicated symbol: %s\n", strings.data[name_index]);
            goto fail;
          }
          hashmap_put(&uc->global_data[idx], bumpstrdup(strings.data[name_index], AL_Manual),
                      global_data);

          int ret;
          kh_put(voidp, created_this_update, (khint64_t)global_data, &ret);
        }
      }
    }
  }

  for (size_t i = 0; i < uc->num_files; ++i) {
    FILE* dyo = dyo_files[i];
    rewind(dyo);
  }

  // Get all exported symbols as hashmap of name => real address.
  HashMap exports = {NULL, 0, 0, AL_Link};
  num_dyos = 0;
  for (size_t i = 0; i < uc->num_files; ++i) {
    FILE* dyo = dyo_files[i];
    if (!ensure_dyo_header(dyo))
      goto fail;

    int record_index = 0;

    StringArray strings = {NULL, 0, 0};
    strarray_push(&strings, NULL, AL_Link);  // 1-based

    DyoLinkData* dld = &uc->files[num_dyos];
    for (;;) {
      unsigned int type;
      unsigned int size;
      if (!read_dyo_record(dyo, &record_index, buf, sizeof(buf), &type, &size))
        goto fail;

      if (type == kTypeString) {
        strarray_push(&strings, bumpstrndup(buf, size, AL_Link), AL_Link);
      } else {
        strarray_push(&strings, NULL, AL_Link);

        if (type == kTypeFunctionExport) {
          unsigned int function_offset = *(unsigned int*)&buf[0];
          unsigned int string_record_index = *(unsigned int*)&buf[4];
          // printf("%d \"%s\" at %p\n", string_record_index, strings.data[string_record_index],
          //      base_address[num_dyos] + function_offset);
          hashmap_put(&exports, strings.data[string_record_index],
                      dld->codeseg_base_address + function_offset);
        } else if (type == kTypeX64Code) {
          ++num_dyos;
          break;
        }
      }
    }
  }

  for (size_t i = 0; i < uc->num_files; ++i) {
    FILE* dyo = dyo_files[i];
    rewind(dyo);
  }

  // Run through all imports and data relocs and fix up the addresses.
  num_dyos = 0;
  for (size_t i = 0; i < uc->num_files; ++i) {
    FILE* dyo = dyo_files[i];
    if (!ensure_dyo_header(dyo))
      goto fail;

    int record_index = 0;

    StringArray strings = {NULL, 0, 0};
    strarray_push(&strings, NULL, AL_Link);  // 1-based

    char* current_data_base = NULL;
    char* current_data_pointer = NULL;
    char* current_data_end = NULL;

    DyoLinkData* dld = &uc->files[num_dyos];
    for (;;) {
      unsigned int type;
      unsigned int size;
      if (!read_dyo_record(dyo, &record_index, buf, sizeof(buf), &type, &size))
        goto fail;

      if (type == kTypeString) {
        strarray_push(&strings, bumpstrndup(buf, size, AL_Link), AL_Link);
      } else {
        strarray_push(&strings, NULL, AL_Link);

        if (type == kTypeImport) {
          unsigned int fixup_offset = *(unsigned int*)&buf[0];
          unsigned int string_record_index = *(unsigned int*)&buf[4];
          void* fixup_address = dld->codeseg_base_address + fixup_offset;
          void* target_address = hashmap_get(&exports, strings.data[string_record_index]);
          if (target_address == NULL) {
            target_address = symbol_lookup(strings.data[string_record_index]);
            if (target_address == NULL) {
              logerr("undefined symbol: %s\n", strings.data[string_record_index]);
              goto fail;
            }
          }
          *((uintptr_t*)fixup_address) = (uintptr_t)target_address;
          // printf("fixed up import %p to point at %p (%s)\n", fixup_address, target_address,
          // strings.data[string_record_index]);
        } else if (type == kTypeInitializedData) {
          unsigned int data_size = *(unsigned int*)&buf[0];
          unsigned int flags = *(unsigned int*)&buf[8];
          bool is_static = flags & 0x01;
          // bool is_rodata = flags & 0x02;
          unsigned int name_index = *(unsigned int*)&buf[12];

          size_t idx = is_static ? num_dyos : uc->num_files;
          current_data_base = hashmap_get(&uc->global_data[idx], strings.data[name_index]);

          // Don't reinitialize data from previous links.
          khiter_t it = kh_get(voidp, created_this_update, (khint64_t)current_data_base);
          if (relinking && it == kh_end(created_this_update))
            continue;

          if (!current_data_base) {
            logerr("init data not allocated\n");
            goto fail;
          }
          current_data_pointer = current_data_base;
          current_data_end = current_data_base + data_size;
        } else if (type == kTypeCodeReferenceToGlobal) {
          unsigned int fixup_offset = *(unsigned int*)&buf[0];
          unsigned int string_record_index = *(unsigned int*)&buf[4];
          void* fixup_address = dld->codeseg_base_address + fixup_offset;
          void* target_address =
              hashmap_get(&uc->global_data[num_dyos], strings.data[string_record_index]);
          if (!target_address) {
            target_address =
                hashmap_get(&uc->global_data[uc->num_files], strings.data[string_record_index]);
            if (!target_address) {
              logerr("undefined symbol: %s\n", strings.data[string_record_index]);
              goto fail;
            }
          }
          *((uintptr_t*)fixup_address) = (uintptr_t)target_address;
          // printf("fixed up data %p to point at %p (%s)\n", fixup_address, target_address,
          // strings.data[string_record_index]);
        } else if (type == kTypeInitializerEnd) {
          assert(current_data_base);
          current_data_base = current_data_pointer = current_data_end = NULL;
        } else if (type == kTypeInitializerBytes) {
          // Don't reinitialize data from previous links.
          khiter_t it = kh_get(voidp, created_this_update, (khint64_t)current_data_base);
          if (relinking && it == kh_end(created_this_update))
            continue;

          assert(current_data_base);
          if (current_data_pointer + size > current_data_end) {
            logerr("initializer overrun bytes\n");
            abort();
          }
          memcpy(current_data_pointer, buf, size);
          current_data_pointer += size;
        } else if (type == kTypeInitializerDataRelocation) {
          // Don't reinitialize data from previous links.
          khiter_t it = kh_get(voidp, created_this_update, (khint64_t)current_data_base);
          if (relinking && it == kh_end(created_this_update))
            continue;

          // This is the same as kTypeCodeReferenceToGlobal, except that a)
          // there's an additional addend added to the target location; and b)
          // the location to fixup is implicit (the current init location)
          // rather than specified as an offset.
          assert(current_data_base);
          if (current_data_pointer + 8 > current_data_end) {
            logerr("initializer overrun reloc\n");
            abort();
          }
          unsigned int name_index = *(unsigned int*)&buf[0];
          int addend = *(int*)&buf[4];

          void* target_address = hashmap_get(&uc->global_data[num_dyos], strings.data[name_index]);
          if (!target_address) {
            target_address = hashmap_get(&uc->global_data[uc->num_files], strings.data[name_index]);
            if (!target_address) {
              logerr("undefined symbol: %s\n", strings.data[name_index]);
              goto fail;
            }
          }
          *((uintptr_t*)current_data_pointer) = (uintptr_t)target_address + addend;
          // printf("fixed up data reloc %p to point at %p (%s)\n", current_data_pointer,
          // target_address, strings.data[name_index]);
          current_data_pointer += 8;
        } else if (type == kTypeInitializerCodeRelocation) {
          // Don't reinitialize data from previous links.
          khiter_t it = kh_get(voidp, created_this_update, (khint64_t)current_data_base);
          if (relinking && it == kh_end(created_this_update))
            continue;

          assert(current_data_base);
          if (current_data_pointer + 8 > current_data_end) {
            logerr("initializer overrun reloc\n");
            abort();
          }
          int offset = *(unsigned int*)&buf[0];
          int addend = *(int*)&buf[4];

          void* target_address = dld->codeseg_base_address + offset + addend;
          *((uintptr_t*)current_data_pointer) = (uintptr_t)target_address + addend;
          // printf("fixed up code reloc %p to point at %p\n", current_data_pointer,
          // target_address);
          current_data_pointer += 8;
        } else if (type == kTypeX64Code) {
          ++num_dyos;
          break;
        }
      }
    }
  }

  for (size_t i = 0; i < uc->num_files; ++i) {
    FILE* dyo = dyo_files[i];
    fclose(dyo);
  }

  for (size_t i = 0; i < uc->num_files; ++i) {
    DyoLinkData* dld = &uc->files[i];
    if (!make_memory_executable(dld->codeseg_base_address, dld->codeseg_size)) {
      goto fail;
    }
  }

  kh_destroy(voidp, created_this_update);
  return true;

fail:
  kh_destroy(voidp, created_this_update);
  return false;
}
