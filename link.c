#include "dyibicc.h"

#if X64WIN
#include <windows.h>
#else
#include <dlfcn.h>
#include <unistd.h>
#endif

#define MAX_DYOS 32

#if X64WIN
static HashMap runtime_function_map;

static void* get_standard_runtime_function(char* name) {
  if (runtime_function_map.capacity == 0) {
#define X(func) hashmap_put(&runtime_function_map, #func, (void*)&func)
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

  return hashmap_get(&runtime_function_map, name);
}
#endif

static void* symbol_lookup(char* name) {
#if X64WIN
  void* f = get_standard_runtime_function(name);
  if (f) {
    return f;
  }
  return (void*)GetProcAddress(GetModuleHandle(NULL), name);
#else
  return dlsym(NULL, name);
#endif
}

void* link_dyos(FILE** dyo_files) {
  char buf[1 << 16];

#if X64WIN
  SYSTEM_INFO system_info;
  GetSystemInfo(&system_info);
  unsigned int page_size = system_info.dwPageSize;
#else
  unsigned int page_size = sysconf(_SC_PAGESIZE);
#endif

  char* base_address[MAX_DYOS];
  size_t code_size[MAX_DYOS];
  int num_dyos = 0;

  void* entry_point = NULL;

  HashMap exported_global_data = {0};
  HashMap per_dyo_global[MAX_DYOS] = {0};

  // Map code blocks from each and save base address.
  // Allocate and global data and save address/size by name.
  for (FILE** dyo = dyo_files; *dyo; ++dyo) {
    if (!ensure_dyo_header(*dyo))
      return NULL;

    unsigned int entry_point_offset = 0xffffffff;
    int record_index = 0;

    StringArray strings = {NULL, 0, 0};
    strarray_push(&strings, NULL);  // 1-based

    for (;;) {
      unsigned int type;
      unsigned int size;
      if (!read_dyo_record(*dyo, &record_index, buf, sizeof(buf), &type, &size))
        return false;

      if (type == kTypeString) {
        strarray_push(&strings, bumpstrndup(buf, size));
      } else {
        strarray_push(&strings, NULL);

        if (type == kTypeEntryPoint) {
          entry_point_offset = *(unsigned int*)&buf[0];
        } else if (type == kTypeX64Code) {
          unsigned int page_sized = (unsigned int)align_to_u(size, page_size);
          // fprintf(stderr, "code %d, allocating %d\n", size, page_sized);
          assert(num_dyos < MAX_DYOS);
          base_address[num_dyos] = allocate_writable_memory(page_sized);
          code_size[num_dyos] = page_sized;
          memcpy(base_address[num_dyos], buf, size);
          // fprintf(stderr, "base address is %p\n", base_address[num_dyos]);
          if (entry_point_offset != 0xffffffff) {
            entry_point = base_address[num_dyos] + entry_point_offset;
          }
          ++num_dyos;
          break;
        } else if (type == kTypeInitializedData) {
          unsigned int data_size = *(unsigned int*)&buf[0];
          unsigned int align = *(unsigned int*)&buf[4];
          unsigned int is_static = *(unsigned int*)&buf[8];
          unsigned int name_index = *(unsigned int*)&buf[12];

          void* global_data = bumpaligned_allocate(data_size, align);
          memset(global_data, 0, data_size);

          if (is_static) {
            hashmap_put(&per_dyo_global[num_dyos], strings.data[name_index], global_data);
          } else {
            hashmap_put(&exported_global_data, strings.data[name_index], global_data);
          }
        }
      }
    }
  }

  for (FILE** dyo = dyo_files; *dyo; ++dyo) {
    rewind(*dyo);
  }

  // Get all exported symbols as hashmap of name => real address.
  HashMap exports = {NULL, 0, 0};
  num_dyos = 0;
  for (FILE** dyo = dyo_files; *dyo; ++dyo) {
    if (!ensure_dyo_header(*dyo))
      return NULL;

    int record_index = 0;

    StringArray strings = {NULL, 0, 0};
    strarray_push(&strings, NULL);  // 1-based

    for (;;) {
      unsigned int type;
      unsigned int size;
      if (!read_dyo_record(*dyo, &record_index, buf, sizeof(buf), &type, &size))
        return false;

      if (type == kTypeString) {
        strarray_push(&strings, bumpstrndup(buf, size));
      } else {
        strarray_push(&strings, NULL);

        if (type == kTypeFunctionExport) {
          unsigned int function_offset = *(unsigned int*)&buf[0];
          unsigned int string_record_index = *(unsigned int*)&buf[4];
          // printf("%d \"%s\" at %p\n", string_record_index, strings.data[string_record_index],
          //      base_address[num_dyos] + function_offset);
          hashmap_put(&exports, strings.data[string_record_index],
                      base_address[num_dyos] + function_offset);
        } else if (type == kTypeX64Code) {
          ++num_dyos;
          break;
        }
      }
    }
  }

  for (FILE** dyo = dyo_files; *dyo; ++dyo) {
    rewind(*dyo);
  }

  // Run through all imports and data relocs and fix up the addresses.
  num_dyos = 0;
  for (FILE** dyo = dyo_files; *dyo; ++dyo) {
    if (!ensure_dyo_header(*dyo))
      return NULL;

    int record_index = 0;

    StringArray strings = {NULL, 0, 0};
    strarray_push(&strings, NULL);  // 1-based

    char* current_data_base = NULL;
    char* current_data_pointer = NULL;
    char* current_data_end = NULL;

    for (;;) {
      unsigned int type;
      unsigned int size;
      if (!read_dyo_record(*dyo, &record_index, buf, sizeof(buf), &type, &size))
        return false;

      if (type == kTypeString) {
        strarray_push(&strings, bumpstrndup(buf, size));
      } else {
        strarray_push(&strings, NULL);

        if (type == kTypeImport) {
          unsigned int fixup_offset = *(unsigned int*)&buf[0];
          unsigned int string_record_index = *(unsigned int*)&buf[4];
          void* fixup_address = base_address[num_dyos] + fixup_offset;
          void* target_address = hashmap_get(&exports, strings.data[string_record_index]);
          if (target_address == NULL) {
            target_address = symbol_lookup(strings.data[string_record_index]);
            if (target_address == NULL) {
              fprintf(stderr, "undefined symbol: %s\n", strings.data[string_record_index]);
              return false;
            }
          }
          *((uintptr_t*)fixup_address) = (uintptr_t)target_address;
          // printf("fixed up import %p to point at %p (%s)\n", fixup_address, target_address,
          // strings.data[string_record_index]);
        } else if (type == kTypeInitializedData) {
          unsigned int data_size = *(unsigned int*)&buf[0];
          unsigned int is_static = *(unsigned int*)&buf[8];
          unsigned int name_index = *(unsigned int*)&buf[12];

          if (is_static) {
            current_data_base = hashmap_get(&per_dyo_global[num_dyos], strings.data[name_index]);
          } else {
            current_data_base = hashmap_get(&exported_global_data, strings.data[name_index]);
          }
          if (!current_data_base) {
            fprintf(stderr, "init data not allocated\n");
            return NULL;
          }
          current_data_pointer = current_data_base;
          current_data_end = current_data_base + data_size;
        } else if (type == kTypeCodeReferenceToGlobal) {
          unsigned int fixup_offset = *(unsigned int*)&buf[0];
          unsigned int string_record_index = *(unsigned int*)&buf[4];
          void* fixup_address = base_address[num_dyos] + fixup_offset;
          void* target_address =
              hashmap_get(&per_dyo_global[num_dyos], strings.data[string_record_index]);
          if (!target_address) {
            target_address = hashmap_get(&exported_global_data, strings.data[string_record_index]);
            if (!target_address) {
              fprintf(stderr, "undefined symbol: %s\n", strings.data[string_record_index]);
              return NULL;
            }
          }
          *((uintptr_t*)fixup_address) = (uintptr_t)target_address;
          // printf("fixed up data %p to point at %p (%s)\n", fixup_address, target_address,
          // strings.data[string_record_index]);
        } else if (type == kTypeInitializerEnd) {
          assert(current_data_base);
          current_data_base = current_data_pointer = current_data_end = NULL;
        } else if (type == kTypeInitializerBytes) {
          assert(current_data_base);
          if (current_data_pointer + size > current_data_end) {
            fprintf(stderr, "initializer overrun bytes\n");
            abort();
          }
          memcpy(current_data_pointer, buf, size);
          current_data_pointer += size;
        } else if (type == kTypeInitializerDataRelocation) {
          assert(current_data_base);
          if (current_data_pointer + 8 > current_data_end) {
            fprintf(stderr, "initializer overrun reloc\n");
            abort();
          }
          unsigned int name_index = *(unsigned int*)&buf[0];
          int addend = *(int*)&buf[4];

          void* target_address = hashmap_get(&per_dyo_global[num_dyos], strings.data[name_index]);
          if (!target_address) {
            target_address = hashmap_get(&exported_global_data, strings.data[name_index]);
            if (!target_address) {
              fprintf(stderr, "undefined symbol: %s\n", strings.data[name_index]);
              return NULL;
            }
          }
          *((uintptr_t*)current_data_pointer) = (uintptr_t)target_address + addend;
          // printf("fixed up data reloc %p to point at %p (%s)\n", current_data_pointer,
          // target_address, strings.data[name_index]);
          current_data_pointer += 8;
        } else if (type == kTypeInitializerCodeRelocation) {
          assert(current_data_base);
          if (current_data_pointer + 8 > current_data_end) {
            fprintf(stderr, "initializer overrun reloc\n");
            abort();
          }
          int offset = *(unsigned int*)&buf[0];
          int addend = *(int*)&buf[4];

          void* target_address = base_address[num_dyos] + offset + addend;
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

  for (FILE** dyo = dyo_files; *dyo; ++dyo) {
    fclose(*dyo);
  }

  for (int i = 0; i < num_dyos; ++i) {
    if (!make_memory_executable(base_address[i], code_size[i])) {
      return NULL;
    }
  }

  // Return entry point.
  return entry_point;
}

void link_reset(void) {
#if X64WIN
  runtime_function_map = (HashMap){NULL, 0, 0};
#endif
}
