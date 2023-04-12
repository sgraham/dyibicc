#include "dyibicc.h"

#if X64WIN
#include <direct.h>
#include <io.h>
#include <malloc.h>
#include <math.h>
#include <process.h>
#include <windows.h>
#define alloca _alloca
#else
#include <dlfcn.h>
#include <unistd.h>
#endif

#define L(x) linker_state.link__##x

#include "khash.h"

KHASH_SET_INIT_INT64(voidp)

#if X64WIN
static void Unimplemented(void) {
  ABORT("unimplemented function");
}

extern void __chkstk();

static void Xstosb(PBYTE Destination, BYTE Value, SIZE_T Count) {
  (void)Destination;
  (void)Value;
  (void)Count;
  ABORT("unimplemented __stosb");
}

static void XReadWriteBarrier(void) {
  // I think this is probably a sufficient implementation in our compiler.
}

static void* get_standard_runtime_function(char* name) {
  if (L(runtime_function_map).capacity == 0) {
    L(runtime_function_map).alloc_lifetime = AL_Link;
#define X(func) hashmap_put(&L(runtime_function_map), #func, (void*)&func)
#define Y(name, func) hashmap_put(&L(runtime_function_map), name, (void*)&func)
    X(__acrt_iob_func);
    X(__chkstk);
    X(__pctype_func);
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
    X(_access);
    X(_chmod);
    X(_chmod);
    X(_ctime64);
    X(_ctime64_s);
    X(_difftime64);
    X(_errno);
    X(_fileno);
    X(_fileno);
    X(_fileno);
    X(_findclose);
    X(_findfirst64i32);
    X(_findnext64i32);
    X(_findnext64i32);
    X(_findnext64i32);
    X(_fstat64i32);
    X(_gmtime64);
    X(_gmtime64_s);
    X(_invalid_parameter_noinfo);
    X(_isatty);
    X(_isctype_l);
    X(_localtime64);
    X(_localtime64_s);
    X(_mkdir);
    X(_mkdir);
    X(_mkgmtime64);
    X(_mktime64);
    X(_pclose);
    X(_popen);
    X(_setmode);
    X(_setmode);
    X(_snprintf);
    X(_stat64i32);
    X(_stat64i32);
    X(_strdup);
    X(_time64);
    X(_timespec64_get);
    X(_unlink);
    X(_wassert);
    X(_wcsicmp);
    X(_wctime64);
    X(_wctime64_s);
    X(_wunlink);
    X(AreFileApisANSI);
    X(atoi);
    X(CharUpperW);
    X(CloseHandle);
    X(CreateFileA);
    X(CreateFileMappingW);
    X(CreateFileW);
    X(CreateMutexW);
    X(CreateThread);
    X(DebugBreak);
    X(DeleteCriticalSection);
    X(DeleteFileA);
    X(DeleteFileW);
    X(exit);
    X(fclose);
    X(fflush);
    X(fgetc);
    X(fgets);
    X(FindClose);
    X(FindFirstFileW);
    X(FlushFileBuffers);
    X(FlushViewOfFile);
    X(fopen);
    X(FormatMessageA);
    X(FormatMessageW);
    X(fprintf);
    X(fputc);
    X(fputs);
    X(fread);
    X(free);
    X(FreeLibrary);
    X(fseek);
    X(ftell);
    X(fwrite);
    X(GetConsoleScreenBufferInfo);
    X(GetCurrentProcess);
    X(GetCurrentProcessId);
    X(GetDiskFreeSpaceA);
    X(GetDiskFreeSpaceW);
    X(getenv);
    X(GetEnvironmentVariableA);
    X(GetFileAttributesA);
    X(GetFileAttributesExW);
    X(GetFileAttributesW);
    X(GetFileSize);
    X(GetFullPathNameA);
    X(GetFullPathNameW);
    X(GetLastError);
    X(GetProcAddress);
    X(GetProcessHeap);
    X(GetStdHandle);
    X(GetSystemInfo);
    X(GetSystemTime);
    X(GetSystemTimeAsFileTime);
    X(GetTempPathA);
    X(GetTempPathW);
    X(GetTickCount);
    X(HeapAlloc);
    X(HeapCompact);
    X(HeapCreate);
    X(HeapDestroy);
    X(HeapFree);
    X(HeapReAlloc);
    X(HeapSize);
    X(HeapValidate);
    X(isalnum);
    X(isalpha);
    X(isdigit);
    X(isprint);
    X(isspace);
    X(LoadLibraryA);
    X(LoadLibraryW);
    X(LocalFree);
    X(LockFile);
    X(LockFileEx);
    X(lstrcmpiW);
    X(lstrcmpW);
    X(lstrlenW);
    X(malloc);
    X(MapViewOfFile);
    X(MapViewOfFileNuma2);
    X(memcmp);
    X(memcpy);
    X(memmove);
    X(memset);
    X(MessageBoxA);
    X(MultiByteToWideChar);
    X(OutputDebugStringA);
    X(OutputDebugStringW);
    X(printf);
    X(putc);
    X(QueryPerformanceCounter);
    X(ReadFile);
    X(realloc);
    X(rewind);
    X(SetConsoleCtrlHandler);
    X(SetConsoleTextAttribute);
    X(SetCurrentDirectoryW);
    X(SetEndOfFile);
    X(SetFilePointer);
    X(SetFileTime);
    X(SetProcessDPIAware);
    X(setvbuf);
    X(Sleep);
    X(sprintf);
    X(sscanf);
    X(strchr);
    X(strcmp);
    X(strcpy);
    X(strcspn);
    X(strlen);
    X(strncmp);
    X(strncpy);
    X(strncpy);
    X(strncpy);
    X(strncpy);
    X(strnlen);
    X(strstr);
    X(strtol);
    X(system);
    X(SystemTimeToFileTime);
    X(SystemTimeToFileTime);
    X(tolower);
    X(uaw_lstrcmpiW);
    X(uaw_lstrcmpW);
    X(uaw_lstrlenW);
    X(uaw_wcschr);
    X(uaw_wcscpy);
    X(uaw_wcsicmp);
    X(uaw_wcslen);
    X(uaw_wcsrchr);
    X(UnlockFile);
    X(UnlockFileEx);
    X(UnmapViewOfFile);
    X(vfprintf);
    X(vsprintf);
    X(WaitForSingleObject);
    X(WaitForSingleObjectEx);
    X(wcschr);
    X(wcscpy);
    X(wcscpy_s);
    X(wcslen);
    X(wcsnlen);
    X(wcsrchr);
    X(wcstok);
    X(WideCharToMultiByte);
    X(WriteFile);
    X(EnterCriticalSection);
    X(GetCurrentThreadId);
    X(InitializeCriticalSection);
    X(LeaveCriticalSection);
    X(TryEnterCriticalSection);
    X(_beginthreadex);
    X(_byteswap_ulong);
    X(_byteswap_ushort);
    X(_chgsign);
    X(_copysign);
    X(_endthreadex);
    X(_hypot);
    X(_hypotf);
    X(_msize);
    X(acos);
    X(asin);
    X(atan);
    X(atan2);
    X(ceil);
    X(cos);
    X(cosh);
    X(exp);
    X(fabs);
    X(floor);
    X(fmod);
    X(frexp);
    X(ldexp);
    X(log);
    X(log10);
    X(modf);
    X(pow);
    X(sin);
    X(sinh);
    X(sqrt);
    X(strrchr);
    X(tan);
    X(tanh);

    Y("__stosb", Xstosb);
    Y("_ReadWriteBarrier", XReadWriteBarrier);
#undef X
  }

  void* ret = hashmap_get(&L(runtime_function_map), name);
  if (ret == NULL) {
    if (strcmp(name, "uaw_CharUpperW") == 0 ||             //
        strcmp(name, "__readgsqword") == 0 ||              //
        strcmp(name, "__readgsdword") == 0 ||              //
        strcmp(name, "__readgsword") == 0 ||               //
        strcmp(name, "__readgsbyte") == 0 ||               //
        strcmp(name, "__stosb") == 0 ||                    //
        strcmp(name, "_ReadWriteBarrier") == 0 ||          //
        strcmp(name, "_umul128") == 0 ||                   //
        strcmp(name, "_mul128") == 0 ||                    //
        strcmp(name, "__shiftright128") == 0 ||            //
        strcmp(name, "_InterlockedExchangeAdd64") == 0 ||  //
        strcmp(name, "_InterlockedExchangeAdd") == 0       //
    ) {
      return (void*)Unimplemented;
    }
  }
  return ret;
}
#endif

static void* symbol_lookup(char* name) {
  if (user_context->get_function_address) {
    void* f = user_context->get_function_address(name);
    if (f) {
      return f;
    }
  }

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

bool link_dyos(void) {
#define BUF_SIZE (16 << 20)
  void* read_buffer = malloc(BUF_SIZE);
  char* buf = read_buffer;

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
      if (!read_dyo_record(dyo, &record_index, buf, BUF_SIZE, &type, &size))
        goto fail;

      if (type == kTypeString) {
        strarray_push(&strings, bumpstrndup(buf, size, AL_Link), AL_Link);
      } else {
        strarray_push(&strings, NULL, AL_Link);

        if (type == kTypeEntryPoint) {
          entry_point_offset = *(unsigned int*)&buf[0];
        } else if (type == kTypeX64Code) {
          if (size == 0)
            size = 1;  // VirtualAlloc and mmap don't accept 0.
          unsigned int page_sized = (unsigned int)align_to_u(size, page_size);
          // outaf("code %d, allocating %d\n", size, page_sized);
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
          bool was_freed = false;

          // Don't recreate non-rodata if relinking. TBD what data should be
          // recreated vs. preserved, maybe some sort of annotations.
          if (relinking) {
            size_t idx = is_static ? num_dyos : uc->num_files;
            void* prev = hashmap_get(&uc->global_data[idx], strings.data[name_index]);
            if (prev) {
              if (is_rodata) {
                aligned_free(prev);
                was_freed = true;
              } else {
                continue;
              }
            }
          }

          void* global_data = aligned_allocate(data_size, align);
          memset(global_data, 0, data_size);

          // The keys need to be strdup'd to stick around for subsequent links.
          size_t idx = is_static ? num_dyos : uc->num_files;
          if (!was_freed) {
            void* prev = hashmap_get(&uc->global_data[idx], strings.data[name_index]);
            if (prev) {
              outaf("duplicated symbol: %s\n", strings.data[name_index]);
              goto fail;
            }
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
  HashMap global_exports = {NULL, 0, 0, AL_Link};
  HashMap* static_exports = alloca(sizeof(HashMap) * uc->num_files);
  memset(static_exports, 0, sizeof(HashMap) * uc->num_files);
  for (size_t i = 0; i < uc->num_files; ++i) {
    static_exports[i].alloc_lifetime = AL_Link;
  }
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
      if (!read_dyo_record(dyo, &record_index, buf, BUF_SIZE, &type, &size))
        goto fail;

      if (type == kTypeString) {
        strarray_push(&strings, bumpstrndup(buf, size, AL_Link), AL_Link);
      } else {
        strarray_push(&strings, NULL, AL_Link);

        if (type == kTypeFunctionExport) {
          unsigned int function_offset = *(unsigned int*)&buf[0];
          int is_static = *(unsigned int*)&buf[4];
          unsigned int string_record_index = *(unsigned int*)&buf[8];
          // printf("%d \"%s\" at %p\n", string_record_index, strings.data[string_record_index],
          //      base_address[num_dyos] + function_offset);
          if (is_static) {
            hashmap_put(&static_exports[num_dyos], strings.data[string_record_index],
                        dld->codeseg_base_address + function_offset);
          } else {
            hashmap_put(&global_exports, strings.data[string_record_index],
                        dld->codeseg_base_address + function_offset);
          }
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
      if (!read_dyo_record(dyo, &record_index, buf, BUF_SIZE, &type, &size))
        goto fail;

      if (type == kTypeString) {
        strarray_push(&strings, bumpstrndup(buf, size, AL_Link), AL_Link);
      } else {
        strarray_push(&strings, NULL, AL_Link);

        if (type == kTypeImport) {
          unsigned int fixup_offset = *(unsigned int*)&buf[0];
          unsigned int string_record_index = *(unsigned int*)&buf[4];
          void* fixup_address = dld->codeseg_base_address + fixup_offset;
          void* target_address = hashmap_get(&global_exports, strings.data[string_record_index]);
          if (target_address == NULL) {
            target_address = symbol_lookup(strings.data[string_record_index]);
            if (target_address == NULL) {
              outaf("undefined import symbol: %s\n", strings.data[string_record_index]);
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
            outaf("init data not allocated\n");
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
              outaf("undefined ref to symbol: %s\n", strings.data[string_record_index]);
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
            ABORT("initializer overrun bytes");
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
            ABORT("initializer overrun reloc");
          }
          unsigned int name_index = *(unsigned int*)&buf[0];
          int addend = *(int*)&buf[4];

          void* target_address = hashmap_get(&uc->global_data[num_dyos], strings.data[name_index]);
          if (!target_address) {
            target_address = hashmap_get(&uc->global_data[uc->num_files], strings.data[name_index]);
            if (!target_address) {
              target_address = hashmap_get(&static_exports[num_dyos], strings.data[name_index]);
              if (!target_address) {
                target_address = hashmap_get(&global_exports, strings.data[name_index]);
                if (!target_address) {
                  target_address = symbol_lookup(strings.data[name_index]);
                  if (!target_address) {
                    outaf("undefined data reloc symbol: %s\n", strings.data[name_index]);
                    goto fail;
                  }
                }
              }
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
            ABORT("initializer overrun reloc");
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
  free(read_buffer);
  return true;

fail:
  kh_destroy(voidp, created_this_update);
  free(read_buffer);
  return false;
}
