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

extern int __chkstk(void);

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

IMPLSTATIC bool link_all_files(void) {
  // This is a hack to avoid disabling -Wunused-function, since these are in
  // khash.h and aren't instantiated.
  (void)__ac_X31_hash_string;
  (void)__ac_Wang_hash;

  UserContext* uc = user_context;

  if (uc->num_files == 0)
    return false;

  // Process fixups.
  for (size_t i = 0; i < uc->num_files; ++i) {
    FileLinkData* fld = &uc->files[i];
    for (int j = 0; j < fld->flen; ++j) {
      void* fixup_address = fld->fixups[j].at;
      char* name = fld->fixups[j].name;
      int addend = fld->fixups[j].addend;

      void* target_address = hashmap_get(&uc->global_data[i], name);
      if (!target_address) {
        target_address = hashmap_get(&uc->exports[i], name);
        if (!target_address) {
          target_address = hashmap_get(&uc->global_data[uc->num_files], name);
          if (!target_address) {
            target_address = hashmap_get(&uc->exports[uc->num_files], name);
            if (!target_address) {
              target_address = symbol_lookup(name);
              if (!target_address) {
                outaf("undefined symbol: %s\n", name);
                return false;
              }
            }
          }
        }
      }

      *((uintptr_t*)fixup_address) = (uintptr_t)target_address + addend;
    }
  }

  for (size_t i = 0; i < uc->num_files; ++i) {
    FileLinkData* fld = &uc->files[i];
    if (!make_memory_executable(fld->codeseg_base_address, fld->codeseg_size)) {
      outaf("failed to make %p size %zu executable\n", fld->codeseg_base_address,
            fld->codeseg_size);
      return false;
    }
  }

  return true;
}
