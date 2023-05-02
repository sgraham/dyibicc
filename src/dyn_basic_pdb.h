#ifndef INCLUDED_DYN_BASIC_PDB_H
#define INCLUDED_DYN_BASIC_PDB_H

// In exactly *one* C file:
//
//   #define DYN_BASIC_PDB_IMPLEMENTATION
//   #include "dyn_basic_pdb.h"
//
// then include and use dyn_basic_pdb.h in other files as usual.
//
// See dbp_example/dyn_basic_pdb_example.c for sample usage.
//
// This implementation only outputs function symbols and line mappings, not full
// type information, though it could be extended to do so with a bunch more
// futzing around.
//
// Only one module is supported (equivalent to one .obj file), because in my
// jit's implementation, all code is generated into a single code segment.
//
// Normally, a .pdb is referenced by another PE (exe/dll) or .dmp, and that's
// how VS locates and decides to load the PDB. Because there's no PE in the case
// of a JIT, in addition to writing a viable pdb, dbp_ready_to_execute() also
// does some goofy hacking to encourage the VS IDE to find and load the
// generated .pdb.

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

typedef struct DbpContext DbpContext;
typedef struct DbpFunctionSymbol DbpFunctionSymbol;
typedef struct DbpExceptionTables DbpExceptionTables;

// Allocates |image_size| bytes for JITing code into. |image_size| must be an
// even multiple of PAGE_SIZE (== 4096). |output_pdb_name| names the .pdb that
// will be generated, and the stub dll is based on the pdb name. The base
// address for generating code into can be retrieved by calling
// dbp_get_image_base().
DbpContext* dbp_create(size_t image_size, const char* output_pdb_name);

// Gets the base of the image, length is |image_size| as passed to dbp_create().
void* dbp_get_image_base(DbpContext* dbp);

// Create a global symbol |name| with the given |filename|. Visual Studio tends
// to work better if |filename| is an absolute path, but it's not required, and
// |filename| is used as-is. |address| should be relative to the base returned
// by dbp_get_image_base(). |length| is in bytes.
DbpFunctionSymbol* dbp_add_function_symbol(DbpContext* ctx,
                                           const char* name,
                                           const char* filename,
                                           unsigned int address,
                                           unsigned int length);

// Add a single debug line mapping to a function. |address| is the first of the
// instructions for the line of code, and should be relative to the base address
// as retrieved by dbp_get_image_base(). |line| is the one-based file line
// number in the source code.
void dbp_add_line_mapping(DbpContext* ctx,
                          DbpFunctionSymbol* fs,
                          unsigned int address,
                          unsigned int line);

// Called when all line information has been written to generate and load the
// .pdb.
//
// exception_tables can be NULL, but stack traces and exceptions will not work
// correctly (see RtlAddFunctionTable() on MSDN for more information). If
// provided, .pdata and UNWIND_INFO will be included in the synthetic DLL, and
// will be equivalent to calling RtlAddFunctionTable(). However, when the
// addresses for a dynamically provided table with RtlAddFunctionTable() overlap
// with the address space for a DLL, the static information in the DLL takes
// precedence and the dynamic information is ignored.
int dbp_ready_to_execute(DbpContext* ctx, DbpExceptionTables* exception_tables);

// Free |ctx| and all associated memory, including the stub dll and image.
void dbp_free(DbpContext* ctx);

// This is stored in CodeView records, default is "dyn_basic_pdb writer 1.0.0.0" if not set.
void dbp_set_compiler_information(DbpContext* ctx,
                                  const char* compiler_version_string,
                                  unsigned short major,
                                  unsigned short minor,
                                  unsigned short build,
                                  unsigned short qfe);


// Same as winnt.h RUNTIME_FUNCTION, we just want to avoid including windows.h
// in the interface header.
typedef struct DbpRUNTIME_FUNCTION {
  unsigned int begin_address;
  unsigned int end_address;
  unsigned int unwind_data;
} DbpRUNTIME_FUNCTION;

// pdata entries will be written to a .pdata section in the dll, with the RVA of
// .unwind_data fixed up to be relative to where unwind_info is stored.
// unwind_data==0 should correspond to &unwind_info[0].
struct DbpExceptionTables {
  DbpRUNTIME_FUNCTION* pdata;
  size_t num_pdata_entries;

  unsigned char* unwind_info;
  size_t unwind_info_byte_length;
};

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // INCLUDED_DYN_BASIC_PDB_H

#ifdef DYN_BASIC_PDB_IMPLEMENTATION

#define _CRT_SECURE_NO_WARNINGS
#pragma warning(disable : 4201)  // non-standard extension: nameless struct/union
#pragma warning(disable : 4668)  // 'X' is not defined as a preprocessor macro, replacing with '0'
                                 // for '#if/#elif'
#pragma warning(disable : 4820)  // 'X' bytes padding added after data member 'Y'
#pragma warning(disable : 5045)  // Compiler will insert Spectre mitigation for memory load if
                                 // /Qspectre switch specified
#pragma comment(lib, "rpcrt4")

#ifdef __clang__
#pragma clang diagnostic ignored "-Wcast-align"
#pragma clang diagnostic ignored "-Wdeclaration-after-statement"
#pragma clang diagnostic ignored "-Wimplicit-fallthrough"
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage"
#endif

#include <Windows.h>
#include <assert.h>
#include <malloc.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef unsigned int u32;
typedef signed int i32;
typedef unsigned short u16;
typedef unsigned long long u64;

typedef struct StreamData StreamData;
typedef struct SuperBlock SuperBlock;
typedef struct NmtAlikeHashTable NmtAlikeHashTable;

struct DbpContext {
  char* base_addr;    // This is the VirtualAlloc base
  void* image_addr;   // and this is the address returned to the user,
  size_t image_size;  // The user has this much accessible, and the allocation is 0x1000 larger.
  char* output_pdb_name;
  char* output_dll_name;
  HMODULE dll_module;

  DbpFunctionSymbol** func_syms;
  size_t func_syms_len;
  size_t func_syms_cap;

  UUID unique_id;

  NmtAlikeHashTable* names_nmt;

  char* compiler_version_string;
  u16 version_major;
  u16 version_minor;
  u16 version_build;
  u16 version_qfe;

  HANDLE file;
  char* data;
  size_t file_size;

  SuperBlock* superblock;

  StreamData** stream_data;
  size_t stream_data_len;
  size_t stream_data_cap;

  u32 next_potential_block;
  u32 padding;
};

typedef struct LineInfo {
  unsigned int address;
  unsigned int line;
} LineInfo;

struct DbpFunctionSymbol {
  char* name;
  char* filename;
  unsigned int address;             // Offset into image_addr where function is.
  unsigned int length;              // Number of bytes long.
  unsigned int module_info_offset;  // Location into modi where the full symbol info can be found.

  LineInfo* lines;
  size_t lines_len;
  size_t lines_cap;
};

#define PUSH_BACK(vec, item)                            \
  do {                                                  \
    if (!vec) {                                         \
      vec = calloc(8, sizeof(*vec));                    \
      vec##_len = 0;                                    \
      vec##_cap = 8;                                    \
    }                                                   \
                                                        \
    if (vec##_cap == vec##_len) {                       \
      vec = realloc(vec, sizeof(*vec) * vec##_cap * 2); \
      vec##_cap *= 2;                                   \
    }                                                   \
                                                        \
    vec[vec##_len++] = item;                            \
  } while (0)

DbpContext* dbp_create(size_t image_size, const char* output_pdb_name) {
  DbpContext* ctx = calloc(1, sizeof(DbpContext));
  // Allocate with an extra page for the DLL header.
  char* base_addr =
      VirtualAlloc(NULL, image_size + 0x1000, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
  ctx->base_addr = base_addr;
  ctx->image_addr = base_addr + 0x1000;
  ctx->image_size = image_size;
  char full_pdb_name[MAX_PATH];
  GetFullPathName(output_pdb_name, sizeof(full_pdb_name), full_pdb_name, NULL);
  ctx->output_pdb_name = _strdup(full_pdb_name);
  size_t len = strlen(full_pdb_name);
  static char suffix[] = ".synthetic.dll";
  ctx->output_dll_name = malloc(len + sizeof(suffix));
  memcpy(ctx->output_dll_name, full_pdb_name, len);
  memcpy(ctx->output_dll_name + len, suffix, sizeof(suffix));
  if (UuidCreate(&ctx->unique_id) != RPC_S_OK) {
    fprintf(stderr, "UuidCreate failed\n");
    return NULL;
  }
  dbp_set_compiler_information(ctx, "dyn_basic_pdb writer 1.0.0.0", 1, 0, 0, 0);
  return ctx;
}

void* dbp_get_image_base(DbpContext* ctx) {
  return ctx->image_addr;
}

DbpFunctionSymbol* dbp_add_function_symbol(DbpContext* ctx,
                                           const char* name,
                                           const char* filename,
                                           unsigned int address,
                                           unsigned int length) {
  DbpFunctionSymbol* fs = calloc(1, sizeof(*fs));
  fs->name = _strdup(name);
  fs->filename = _strdup(filename);
  fs->address = address;
  fs->length = length;
  PUSH_BACK(ctx->func_syms, fs);
  return fs;
}

void dbp_set_compiler_information(DbpContext* ctx,
                                  const char* compiler_version_string,
                                  unsigned short major,
                                  unsigned short minor,
                                  unsigned short build,
                                  unsigned short qfe) {
  if (ctx->compiler_version_string)
    free(ctx->compiler_version_string);
  ctx->compiler_version_string = _strdup(compiler_version_string);
  ctx->version_major = major;
  ctx->version_minor = minor;
  ctx->version_build = build;
  ctx->version_qfe = qfe;
}

void dbp_add_line_mapping(DbpContext* ctx,
                          DbpFunctionSymbol* sym,
                          unsigned int address,
                          unsigned int line) {
  (void)ctx;
  assert(address >= sym->address);
  LineInfo line_info = {.address = address, .line = line};
  PUSH_BACK(sym->lines, line_info);
}

static const char big_hdr_magic[0x1e] = "Microsoft C/C++ MSF 7.00\r\n\x1a\x44\x53";

#define ENSURE(x, want)                                                               \
  do {                                                                                \
    if (want != x) {                                                                  \
      fprintf(stderr, "%s:%d: failed %s wasn't %s\n", __FILE__, __LINE__, #x, #want); \
      return 0;                                                                       \
    }                                                                                 \
  } while (0)

#define ENSURE_NE(x, bad)                                                         \
  do {                                                                            \
    if (bad == x) {                                                               \
      fprintf(stderr, "%s:%d: failed %s was %s\n", __FILE__, __LINE__, #x, #bad); \
      return 0;                                                                   \
    }                                                                             \
  } while (0)

struct SuperBlock {
  char file_magic[0x1e];
  char padding[2];
  u32 block_size;
  u32 free_block_map_block;
  u32 num_blocks;
  u32 num_directory_bytes;
  u32 unknown;
  u32 block_map_addr;
};

#define STUB_RDATA_SIZE 4096

#define BLOCK_SIZE 4096
#define DEFAULT_NUM_BLOCKS 256
#define PAGE_TO_WORD(pn) (pn >> 6)
#define PAGE_MASK(pn) (1ULL << (pn & ((sizeof(u64) * CHAR_BIT) - 1)))
static const char synthetic_obj_name[] = "dyn_basic_pdb-synthetic-for-jit.obj";

static void mark_block_used(DbpContext* ctx, u32 pn) {
  u64* map2 = (u64*)&ctx->data[BLOCK_SIZE * 2];
  map2[PAGE_TO_WORD(pn)] &= ~PAGE_MASK(pn);
}

static int block_is_free(DbpContext* ctx, u32 pn) {
  u64* map2 = (u64*)&ctx->data[BLOCK_SIZE * 2];
  return !!(map2[PAGE_TO_WORD(pn)] & PAGE_MASK(pn));
}

static void* get_block_ptr(DbpContext* ctx, u32 i) {
  return &ctx->data[BLOCK_SIZE * i];
}

static u32 alloc_block(DbpContext* ctx) {
  for (;;) {
    if (block_is_free(ctx, ctx->next_potential_block)) {
      mark_block_used(ctx, ctx->next_potential_block);
      return ctx->next_potential_block++;
    }
    ctx->next_potential_block++;
  }
}

struct StreamData {
  u32 stream_index;

  u32 data_length;

  char* cur_write;

  u32* blocks;
  size_t blocks_len;
  size_t blocks_cap;
};

static void stream_write_block(DbpContext* ctx, StreamData* stream, const void* data, size_t len) {
  if (!stream->cur_write) {
    u32 block_id = alloc_block(ctx);
    PUSH_BACK(stream->blocks, block_id);
    stream->cur_write = get_block_ptr(ctx, block_id);
  }

  u32 cur_block_filled = stream->data_length % BLOCK_SIZE;
  u32 max_remaining_this_block = BLOCK_SIZE - cur_block_filled;
  if (max_remaining_this_block >= len) {
    memcpy(stream->cur_write, data, len);
    stream->cur_write += len;
    stream->data_length += (u32)len;
  } else {
    memcpy(stream->cur_write, data, max_remaining_this_block);
    stream->cur_write += max_remaining_this_block;
    stream->data_length += max_remaining_this_block;
    stream->cur_write = NULL;
    stream_write_block(ctx, stream, (const char*)data + max_remaining_this_block,
                       len - max_remaining_this_block);
  }
}

static void stream_ensure_init(DbpContext* ctx, StreamData* stream) {
  // Hack for fixup capture if the block pointer hasn't been allocated yet
  // before the macro wants to capture it.
  unsigned char none;
  stream_write_block(ctx, stream, &none, 0);
}

#define SW_BLOCK(x, len) stream_write_block(ctx, stream, x, len)
#define SW_U32(x)                                   \
  do {                                              \
    u32 _ = (x);                                    \
    stream_write_block(ctx, stream, &_, sizeof(_)); \
  } while (0)
#define SW_I32(x)                                   \
  do {                                              \
    i32 _ = (x);                                    \
    stream_write_block(ctx, stream, &_, sizeof(_)); \
  } while (0)
#define SW_U16(x)                                   \
  do {                                              \
    u16 _ = (x);                                    \
    stream_write_block(ctx, stream, &_, sizeof(_)); \
  } while (0)
#define SW_ALIGN(to)                        \
  do {                                      \
    while (stream->data_length % to != 0) { \
      SW_BLOCK("", 1);                      \
    }                                       \
  } while (0)

typedef char* SwFixup;
#define SW_CAPTURE_FIXUP(strukt, field) \
  (stream_ensure_init(ctx, stream), stream->cur_write + offsetof(strukt, field))

#define SW_WRITE_FIXUP_FOR_LOCATION_U32(swfixup) \
  do {                                           \
    *(u32*)swfixup = stream->data_length;        \
  } while (0)

typedef u32 SwDelta;
#define SW_CAPTURE_DELTA_START() (stream->data_length)

#define SW_WRITE_DELTA_FIXUP(swfixup, delta)      \
  do {                                            \
    *(u32*)swfixup = stream->data_length - delta; \
  } while (0)

static void write_superblock(DbpContext* ctx) {
  SuperBlock* sb = (SuperBlock*)ctx->data;
  ctx->superblock = sb;
  memcpy(sb->file_magic, big_hdr_magic, sizeof(big_hdr_magic));
  sb->padding[0] = '\0';
  sb->padding[1] = '\0';
  sb->block_size = BLOCK_SIZE;
  sb->free_block_map_block = 2;  // We never use map 1.
  sb->num_blocks = DEFAULT_NUM_BLOCKS;
  // num_directory_bytes filled in later once we've written everything else.
  sb->unknown = 0;
  sb->block_map_addr = 3;

  // Mark all pages as free, then mark the first four in use:
  // 0 is super block, 1 is FPM1, 2 is FPM2, 3 is the block map.
  memset(&ctx->data[BLOCK_SIZE], 0xff, BLOCK_SIZE * 2);
  for (u32 i = 0; i <= 3; ++i)
    mark_block_used(ctx, i);
}

static StreamData* add_stream(DbpContext* ctx) {
  StreamData* stream = calloc(1, sizeof(StreamData));
  stream->stream_index = (u32)ctx->stream_data_len;
  PUSH_BACK(ctx->stream_data, stream);
  return stream;
}

static u32 align_to(u32 val, u32 align) {
  return (val + align - 1) / align * align;
}

typedef struct PdbStreamHeader {
  u32 Version;
  u32 Signature;
  u32 Age;
  UUID UniqueId;
} PdbStreamHeader;

static int write_pdb_info_stream(DbpContext* ctx, StreamData* stream, u32 names_stream) {
  PdbStreamHeader psh = {
      .Version = 20000404, /* VC70 */
      .Signature = (u32)time(NULL),
      .Age = 1,
  };
  memcpy(&psh.UniqueId, &ctx->unique_id, sizeof(UUID));
  SW_BLOCK(&psh, sizeof(psh));

  // Named Stream Map.

  // The LLVM docs are something that would be nice to refer to here:
  //
  //   https://llvm.org/docs/PDB/HashTable.html
  //
  // But unfortunately, this specific page is quite misleading (unlike the rest
  // of the PDB docs which are quite helpful). The microsoft-pdb repo is,
  // uh, "dense", but has the benefit of being correct by definition:
  //
  // https://github.com/microsoft/microsoft-pdb/blob/082c5290e5aff028ae84e43affa8be717aa7af73/PDB/include/nmtni.h#L77-L95
  // https://github.com/microsoft/microsoft-pdb/blob/082c5290e5aff028ae84e43affa8be717aa7af73/PDB/include/map.h#L474-L508
  //
  // Someone naturally already figured this out, as LLVM writes the correct
  // data, just the docs are wrong. (LLVM's patch for docs setup seems a bit
  // convoluted which is why I'm whining in a buried comment instead of just
  // fixing it...)

  // Starts with the string buffer (which we pad to % 4, even though that's not
  // actually required). We don't bother with actually building and updating a
  // map as the only named stream we need is /names (TBD: possibly /LinkInfo?).
  static const char string_data[] = "/names\0";
  SW_U32(sizeof(string_data));
  SW_BLOCK(string_data, sizeof(string_data));

  // Then hash size, and capacity.
  SW_U32(1);  // Size
  SW_U32(1);  // Capacity
  // Then two bit vectors, first for "present":
  SW_U32(0x01);  // Present length (1 word follows)
  SW_U32(0x01);  // 0b0000`0001    (only bucket occupied)
  // Then for "deleted" (we don't write any).
  SW_U32(0);
  // Now, the maps: mapping "/names" at offset 0 above to given names stream.
  SW_U32(0);
  SW_U32(names_stream);
  // This is "niMac", which is the last index allocated. We don't need it.
  SW_U32(0);

  // Finally, feature codes, which indicate that we're somewhat modern.
  SW_U32(20140508); /* VC140 */

  return 1;
}

typedef struct TpiStreamHeader {
  u32 version;
  u32 header_size;
  u32 type_index_begin;
  u32 type_index_end;
  u32 type_record_bytes;

  u16 hash_stream_index;
  u16 hash_aux_stream_index;
  u32 hash_key_size;
  u32 num_hash_buckets;

  i32 hash_value_buffer_offset;
  u32 hash_value_buffer_length;

  i32 index_offset_buffer_offset;
  u32 index_offset_buffer_length;

  i32 hash_adj_buffer_offset;
  u32 hash_adj_buffer_length;
} TpiStreamHeader;

static int write_empty_tpi_ipi_stream(DbpContext* ctx, StreamData* stream) {
  // This is an "empty" TPI/IPI stream, we do not emit any user-defined types
  // currently.
  TpiStreamHeader tsh = {
      .version = 20040203, /* V80 */
      .header_size = sizeof(TpiStreamHeader),
      .type_index_begin = 0x1000,
      .type_index_end = 0x1000,
      .type_record_bytes = 0,
      .hash_stream_index = 0xffff,
      .hash_aux_stream_index = 0xffff,
      .hash_key_size = 4,
      .num_hash_buckets = 0x3ffff,
      .hash_value_buffer_offset = 0,
      .hash_value_buffer_length = 0,
      .index_offset_buffer_offset = 0,
      .index_offset_buffer_length = 0,
      .hash_adj_buffer_offset = 0,
      .hash_adj_buffer_length = 0,
  };
  SW_BLOCK(&tsh, sizeof(tsh));
  return 1;
}

// Copied from:
// https://github.com/microsoft/microsoft-pdb/blob/082c5290e5aff028ae84e43affa8be717aa7af73/PDB/include/misc.h#L15
// with minor type adaptations. It needs to match that implementation to make
// serialized hashes match up.
static u32 calc_hash(char* pb, size_t cb) {
  u32 ulHash = 0;

  // hash leading dwords using Duff's Device
  size_t cl = cb >> 2;
  u32* pul = (u32*)pb;
  u32* pulMac = pul + cl;
  size_t dcul = cl & 7;

  switch (dcul) {
    do {
      dcul = 8;
      ulHash ^= pul[7];
      case 7:
        ulHash ^= pul[6];
      case 6:
        ulHash ^= pul[5];
      case 5:
        ulHash ^= pul[4];
      case 4:
        ulHash ^= pul[3];
      case 3:
        ulHash ^= pul[2];
      case 2:
        ulHash ^= pul[1];
      case 1:
        ulHash ^= pul[0];
      case 0:;
    } while ((pul += dcul) < pulMac);
  }

  pb = (char*)(pul);

  // hash possible odd word
  if (cb & 2) {
    ulHash ^= *(unsigned short*)pb;
    pb = (char*)((unsigned short*)(pb) + 1);
  }

  // hash possible odd byte
  if (cb & 1) {
    ulHash ^= (u32) * (pb++);
  }

  const u32 toLowerMask = 0x20202020;
  ulHash |= toLowerMask;
  ulHash ^= (ulHash >> 11);

  return (ulHash ^ (ulHash >> 16));
}

// A hash table that emulates the microsoft-pdb nmt.h as required by the /names
// stream.
typedef struct NmtAlikeHashTable {
  char* strings;        // This is a "\0bunch\0of\0strings\0" always starting with \0,
                        // so that 0 is an invalid index.
  size_t strings_len;
  size_t strings_cap;

  u32* hash;            // hash[hashed_value % hash_len] = name_index, which is an index
                        // into strings to get the actual value.
  size_t hash_len;

  u32 num_names;
} NmtAlikeHashTable;

#define NMT_INVALID (0u)

static NmtAlikeHashTable* nmtalike_create(void) {
  NmtAlikeHashTable* ret = calloc(1, sizeof(NmtAlikeHashTable));
  PUSH_BACK(ret->strings, '\0');
  ret->hash = calloc(1, sizeof(u32));
  ret->hash_len = 1;
  return ret;
}

static char* nmtalike__string_for_name_index(NmtAlikeHashTable* nmt, u32 name_index) {
  if (name_index >= nmt->strings_len)
    return NULL;
  return &nmt->strings[name_index];
}

// If |str| already exists, return 1 with *out_name_index set to its name_index.
// Else, return 0 and *out_slot is where a new name_index should be stored for
// |str|.
static void nmtalike__find(NmtAlikeHashTable* nmt, char* str, u32* out_name_index, u32* out_slot) {
  assert(nmt->strings_len > 0);
  assert(nmt->hash_len > 0);

  size_t len = strlen(str);
  u32 slot = calc_hash(str, len) % nmt->hash_len;
  u32 name_index = NMT_INVALID;
  for (;;) {
    name_index = nmt->hash[slot];
    if (name_index == NMT_INVALID)
      break;

    if (strcmp(str, nmtalike__string_for_name_index(nmt, name_index)) == 0)
      break;

    ++slot;
    if (slot >= nmt->hash_len)
      slot = 0;
  }

  *out_slot = slot;
  *out_name_index = name_index;
}

// Returns new name index.
static u32 nmtalike__append_to_string_buffer(NmtAlikeHashTable* nmt, char* str) {
  size_t len = strlen(str) + 1;

  while (nmt->strings_cap < nmt->strings_len + len) {
    nmt->strings = realloc(nmt->strings, sizeof(*nmt->strings) * nmt->strings_cap * 2);
    nmt->strings_cap *= 2;
  }

  char* start = &nmt->strings[nmt->strings_len];
  memcpy(start, str, len);
  nmt->strings_len += len;
  return (u32)(start - nmt->strings);
}

static void nmtalike__rehash(NmtAlikeHashTable* nmt, u32 new_count) {
  size_t new_hash_byte_len = sizeof(u32) * new_count;
  u32* new_hash = malloc(new_hash_byte_len);
  size_t new_hash_len = new_count;

  memset(new_hash, 0, new_hash_byte_len);

  for (u32 i = 0; i < nmt->hash_len; ++i) {
    u32 name_index = nmt->hash[i];
    if (name_index != NMT_INVALID) {
      char* str = nmtalike__string_for_name_index(nmt, name_index);
      u32 j = calc_hash(str, strlen(str)) % new_count;
      for (;;) {
        if (new_hash[j] == NMT_INVALID)
          break;
        ++j;
        if (j == new_count)
          j = 0;
      }
      new_hash[j] = name_index;
    }
  }

  free(nmt->hash);
  nmt->hash = new_hash;
  nmt->hash_len = new_hash_len;
}

static void nmtalike__grow(NmtAlikeHashTable* nmt) {
  ++nmt->num_names;

  // These growth factors have to match so that the buckets line up as expected
  // when serialized.
  if (nmt->hash_len * 3 / 4 < nmt->num_names) {
    nmtalike__rehash(nmt, (u32)(nmt->hash_len * 3 / 2 + 1));
  }
}

static u32 nmtalike_add_string(NmtAlikeHashTable* nmt, char* str) {
  u32 name_index = NMT_INVALID;
  u32 insert_location;
  nmtalike__find(nmt, str, &name_index, &insert_location);
  if (name_index != NMT_INVALID)
    return name_index;

  name_index = nmtalike__append_to_string_buffer(nmt, str);
  nmt->hash[insert_location] = name_index;
  nmtalike__grow(nmt);
  return name_index;
}

static u32 nmtalike_name_index_for_string(NmtAlikeHashTable* nmt, char* str) {
  u32 name_index = NMT_INVALID;
  u32 slot_unused;
  nmtalike__find(nmt, str, &name_index, &slot_unused);
  return name_index; // either NMT_INVALID or the slot
}

typedef struct NmtAlikeEnum {
  NmtAlikeHashTable* nmt;
  u32 i;
} NmtAlikeEnum;

static NmtAlikeEnum nmtalike_enum_begin(NmtAlikeHashTable* nmt) {
  return (NmtAlikeEnum){.nmt = nmt, .i = (u32)-1};
}

static int nmtalike_enum_next(NmtAlikeEnum* it) {
  while (++it->i < it->nmt->hash_len) {
    if (it->nmt->hash[it->i] != NMT_INVALID)
      return 1;
  }
  return 0;
}

static void nmtalike_enum_get(NmtAlikeEnum* it, u32* name_index, char** str) {
  *name_index = it->nmt->hash[it->i];
  *str = nmtalike__string_for_name_index(it->nmt, *name_index);
}

static int write_names_stream(DbpContext* ctx, StreamData* stream) {
  NmtAlikeHashTable* nmt = ctx->names_nmt = nmtalike_create();

  for (size_t i = 0; i < ctx->func_syms_len; ++i) {
    nmtalike_add_string(nmt, ctx->func_syms[i]->filename);
  }

  // "/names" is:
  //
  // header
  // string bufer
  // hash table
  // number of names in the table
  //
  // Most of the 'magic' is in NmtAlikeHashTable, specifically in how it's
  // grown.

  SW_U32(0xeffeeffe);               // Header
  SW_U32(1);                        // verLongHash
  SW_U32((u32)(nmt->strings_len));  // Size of string buffer
  SW_BLOCK(nmt->strings, nmt->strings_len);

  SW_U32((u32)(nmt->hash_len));                      // Number of buckets
  SW_BLOCK(nmt->hash, nmt->hash_len * sizeof(u32));  // Hash buckets

  SW_U32(nmt->num_names);  // Number of names in the hash

  return 1;
}

typedef struct DbiStreamHeader {
  i32 version_signature;
  u32 version_header;
  u32 age;
  u16 global_stream_index;
  u16 build_number;
  u16 public_stream_index;
  u16 pdb_dll_version;
  u16 sym_record_stream;
  u16 pdb_dll_rbld;
  i32 mod_info_size;
  i32 section_contribution_size;
  i32 section_map_size;
  i32 source_info_size;
  i32 type_server_map_size;
  u32 mfc_type_server_index;
  i32 optional_dbg_header_size;
  i32 ec_substream_size;
  u16 flags;
  u16 machine;
  u32 padding;
} DbiStreamHeader;

// Part of ModInfo
typedef struct SectionContribEntry {
  u16 section;
  char padding1[2];
  i32 offset;
  i32 size;
  u32 characteristics;
  u16 module_index;
  char padding2[2];
  u32 data_crc;
  u32 reloc_crc;
} SectionContribEntry;

typedef struct ModInfo {
  u32 unused1;
  SectionContribEntry section_contr;
  u16 flags;
  u16 module_sym_stream;
  u32 sym_byte_size;
  u32 c11_byte_size;
  u32 c13_byte_size;
  u16 source_file_count;
  char padding[2];
  u32 unused2;
  u32 source_file_name_index;
  u32 pdb_file_path_name_index;
  // char module_name[];
  // char obj_file_name[];
} ModInfo;

typedef struct SectionMapHeader {
  u16 count;      // Number of segment descriptors
  u16 log_count;  // Number of logical segment descriptors
} SectionMapHeader;

typedef struct SectionMapEntry {
  u16 flags;  // See the SectionMapEntryFlags enum below.
  u16 ovl;    // Logical overlay number
  u16 group;  // Group index into descriptor array.
  u16 frame;
  u16 section_name;  // Byte index of segment / group name in string table, or 0xFFFF.
  u16 class_name;    // Byte index of class in string table, or 0xFFFF.
  u32 offset;        // Byte offset of the logical segment within physical segment. If group is set
                     // in flags, this is the offset of the group.
  u32 section_length;  // Byte count of the segment or group.
} SectionMapEntry;

enum SectionMapEntryFlags {
  SMEF_Read = 1 << 0,               // Segment is readable.
  SMEF_Write = 1 << 1,              // Segment is writable.
  SMEF_Execute = 1 << 2,            // Segment is executable.
  SMEF_AddressIs32Bit = 1 << 3,     // Descriptor describes a 32-bit linear address.
  SMEF_IsSelector = 1 << 8,         // Frame represents a selector.
  SMEF_IsAbsoluteAddress = 1 << 9,  // Frame represents an absolute address.
  SMEF_IsGroup = 1 << 10            // If set, descriptor represents a group.
};

typedef struct FileInfoSubstreamHeader {
  u16 num_modules;
  u16 num_source_files;

  // u16 mod_indices[num_modules];
  // u16 mod_file_counts[num_modules];
  // u32 file_name_offsets[num_source_files];
  // char names_buffer[][num_source_files];
} FileInfoSubstreamHeader;

typedef struct GsiData {
  u32 global_symbol_stream;
  u32 public_symbol_stream;
  u32 sym_record_stream;
} GsiData;

typedef struct DbiWriteData {
  GsiData gsi_data;
  u32 section_header_stream;
  u32 module_sym_stream;
  u32 module_symbols_byte_size;
  u32 module_c13_byte_size;
  u32 num_source_files;

  SwFixup fixup_mod_info_size;
  SwFixup fixup_section_contribution_size;
  SwFixup fixup_section_map_size;
  SwFixup fixup_source_info_size;
  SwFixup fixup_optional_dbg_header_size;
  SwFixup fixup_ec_substream_size;
} DbiWriteData;

static void write_dbi_stream_header(DbpContext* ctx, StreamData* stream, DbiWriteData* dwd) {
  DbiStreamHeader dsh = {
      .version_signature = -1,
      .version_header = 19990903, /* V70 */
      .age = 1,
      .global_stream_index = (u16)dwd->gsi_data.global_symbol_stream,
      .build_number = 0x8eb, /* 14.11 "new format" */
      .public_stream_index = (u16)dwd->gsi_data.public_symbol_stream,
      .pdb_dll_version = 0,
      .sym_record_stream = (u16)dwd->gsi_data.sym_record_stream,
      .pdb_dll_rbld = 0,
      .type_server_map_size = 0,
      .mfc_type_server_index = 0,
      .flags = 0,
      .machine = 0x8664, /* x64 */
      .padding = 0,
  };

  dwd->fixup_mod_info_size = SW_CAPTURE_FIXUP(DbiStreamHeader, mod_info_size);
  dwd->fixup_section_contribution_size =
      SW_CAPTURE_FIXUP(DbiStreamHeader, section_contribution_size);
  dwd->fixup_section_map_size = SW_CAPTURE_FIXUP(DbiStreamHeader, section_map_size);
  dwd->fixup_source_info_size = SW_CAPTURE_FIXUP(DbiStreamHeader, source_info_size);
  dwd->fixup_optional_dbg_header_size = SW_CAPTURE_FIXUP(DbiStreamHeader, optional_dbg_header_size);
  dwd->fixup_ec_substream_size = SW_CAPTURE_FIXUP(DbiStreamHeader, ec_substream_size);
  SW_BLOCK(&dsh, sizeof(dsh));
}

static void write_dbi_stream_modinfo(DbpContext* ctx, StreamData* stream, DbiWriteData* dwd) {
  SwDelta block_start = SW_CAPTURE_DELTA_START();

  // Module Info Substream. We output a single module with a single section for
  // the whole jit blob.
  ModInfo mod = {
      .unused1 = 0,
      .section_contr =
          {
              .section = 1,
              .padding1 = {0, 0},
              .offset = 0,
              .size = (i32)ctx->image_size,
              .characteristics = IMAGE_SCN_CNT_CODE | IMAGE_SCN_ALIGN_16BYTES |
                                 IMAGE_SCN_MEM_EXECUTE | IMAGE_SCN_MEM_READ,
              .module_index = 0,
              .padding2 = {0, 0},
              .data_crc = 0,
              .reloc_crc = 0,
          },
      .flags = 0,
      .module_sym_stream = (u16)dwd->module_sym_stream,
      .sym_byte_size = dwd->module_symbols_byte_size,
      .c11_byte_size = 0,
      .c13_byte_size = dwd->module_c13_byte_size,
      .source_file_count = (u16)dwd->num_source_files,
      .padding = {0, 0},
      .unused2 = 0,
      .source_file_name_index = 0,
      .pdb_file_path_name_index = 0,
  };
  SW_BLOCK(&mod, sizeof(mod));
  // Intentionally twice for two index fields.
  SW_BLOCK(synthetic_obj_name, sizeof(synthetic_obj_name));
  SW_BLOCK(synthetic_obj_name, sizeof(synthetic_obj_name));
  SW_ALIGN(4);
  SW_WRITE_DELTA_FIXUP(dwd->fixup_mod_info_size, block_start);
}

static void write_dbi_stream_section_contribution(DbpContext* ctx,
                                                  StreamData* stream,
                                                  DbiWriteData* dwd) {
  SwDelta block_start = SW_CAPTURE_DELTA_START();

  // We only write a single section, one big for .text.
  SW_U32(0xf12eba2d);  // Ver60
  SectionContribEntry text_section = {
      .section = 1,
      .padding1 = {0, 0},
      .offset = 0,
      .size = (i32)ctx->image_size,
      .characteristics =
          IMAGE_SCN_CNT_CODE | IMAGE_SCN_ALIGN_16BYTES | IMAGE_SCN_MEM_EXECUTE | IMAGE_SCN_MEM_READ,
      .module_index = 0,
      .padding2 = {0, 0},
      .data_crc = 0,
      .reloc_crc = 0,
  };
  SW_BLOCK(&text_section, sizeof(text_section));
  SW_WRITE_DELTA_FIXUP(dwd->fixup_section_contribution_size, block_start);
}

static void write_dbi_stream_section_map(DbpContext* ctx, StreamData* stream, DbiWriteData* dwd) {
  // This pretends to look sensible, but it doesn't make a lot of sense to me at
  // the moment. A single SectionMapEntry for just .text that maps causes all
  // functions to not have an RVA, and so line numbers don't get found. (Don't)
  // ask me how many hours of trial-and-error that took to figure out!
  //
  // Making a second section seems to make it work. We make it .rdata and one
  // page large, which matches the fake dll that we write later.

  SwDelta block_start = SW_CAPTURE_DELTA_START();

  SectionMapHeader header = {.count = 2, .log_count = 2};
  SW_BLOCK(&header, sizeof(header));

  SectionMapEntry text = {
      .flags = SMEF_Read | SMEF_Execute | SMEF_AddressIs32Bit | SMEF_IsSelector,
      .ovl = 0,               // ?
      .group = 0,             // ?
      .frame = 1,             // 1-based section number
      .section_name = 0xfff,  // ?
      .class_name = 0xffff,   // ?
      .offset = 0,            // This seems to be added to the RVA, but defaults to 0x1000.
      .section_length = (u32)ctx->image_size,
  };
  SW_BLOCK(&text, sizeof(text));

  SectionMapEntry rdata = {
      .flags = SMEF_Read | SMEF_AddressIs32Bit,
      .ovl = 0,               // ?
      .group = 0,             // ?
      .frame = 2,             // 1-based section number
      .section_name = 0xfff,  // ?
      .class_name = 0xffff,   // ?
      .offset = 0,            // This seems to be added to the RVA.
      .section_length = STUB_RDATA_SIZE,
  };
  SW_BLOCK(&rdata, sizeof(rdata));

  SW_WRITE_DELTA_FIXUP(dwd->fixup_section_map_size, block_start);
}

static void write_dbi_stream_file_info(DbpContext* ctx, StreamData* stream, DbiWriteData* dwd) {
  SwDelta block_start = SW_CAPTURE_DELTA_START();

  // File Info Substream
  FileInfoSubstreamHeader fish = {
      .num_modules = 1,       // This is always 1 for us.
      .num_source_files = 0,  // This is unused.
  };
  SW_BLOCK(&fish, sizeof(fish));

  SW_U16(0);                               // [mod_indices], unused.
  SW_U16((u16)ctx->names_nmt->num_names);  // [num_source_files]

  // First, write array of offset to files.
  NmtAlikeEnum it = nmtalike_enum_begin(ctx->names_nmt);
  while (nmtalike_enum_next(&it)) {
    u32 name_index;
    char* str;
    nmtalike_enum_get(&it, &name_index, &str);
    SW_U32(name_index);
  }

  // Then the strings buffer.
  SW_BLOCK(ctx->names_nmt->strings, ctx->names_nmt->strings_len);

  SW_ALIGN(4);
  SW_WRITE_DELTA_FIXUP(dwd->fixup_source_info_size, block_start);
}

static void write_dbi_stream_ec_substream(DbpContext* ctx, StreamData* stream, DbiWriteData* dwd) {
  SwDelta block_start = SW_CAPTURE_DELTA_START();

  // llvm-pdbutil tries to load a pdb name from the ECSubstream. Emit a single
  // nul byte, as we only refer to index 0. (This is an NMT if it needs to be
  // fully written with more data.)
  static unsigned char empty_nmt[] = {
      0xfe, 0xef, 0xfe, 0xef,  // Header
      0x01, 0x00, 0x00, 0x00,  // verLongHash
      0x01, 0x00, 0x00, 0x00,  // Size
      0x00,                    // Single \0 string.
      0x01, 0x00, 0x00, 0x00,  // One element in array
      0x00, 0x00, 0x00, 0x00,  // Entry 0 which is ""
      0x00, 0x00, 0x00, 0x00,  // Number of names in hash table
                               // Doesn't include initial nul which is always in the table.
  };
  SW_BLOCK(&empty_nmt, sizeof(empty_nmt));

  // I don't think this one's supposed to be aligned /shruggie.

  SW_WRITE_DELTA_FIXUP(dwd->fixup_ec_substream_size, block_start);
}

static void write_dbi_stream_optional_dbg_header(DbpContext* ctx,
                                                 StreamData* stream,
                                                 DbiWriteData* dwd) {
  SwDelta block_start = SW_CAPTURE_DELTA_START();

  // Index 5 points to the section header stream, which is theoretically
  // optional, but llvm-pdbutil doesn't like it if it's not there, so I'm
  // guessing that various microsoft things don't either. The stream it points
  // at is empty, but that seems to be sufficient.
  for (int i = 0; i < 5; ++i)
    SW_U16(0xffff);
  SW_U16((u16)dwd->section_header_stream);
  for (int i = 0; i < 5; ++i)
    SW_U16(0xffff);

  SW_WRITE_DELTA_FIXUP(dwd->fixup_optional_dbg_header_size, block_start);
}

static int write_dbi_stream(DbpContext* ctx, StreamData* stream, DbiWriteData* dwd) {
  write_dbi_stream_header(ctx, stream, dwd);
  write_dbi_stream_modinfo(ctx, stream, dwd);
  write_dbi_stream_section_contribution(ctx, stream, dwd);
  write_dbi_stream_section_map(ctx, stream, dwd);
  write_dbi_stream_file_info(ctx, stream, dwd);
  // No type server map.
  // No MFC type server map.
  write_dbi_stream_ec_substream(ctx, stream, dwd);
  write_dbi_stream_optional_dbg_header(ctx, stream, dwd);

  return 1;
}

#define IPHR_HASH 4096

typedef struct HashSym {
  char* name;
  u32 offset;
  u32 hash_bucket;  // Must be % IPHR_HASH
} HashSym;

typedef struct HRFile {
  u32 off;
  u32 cref;
} HRFile;

typedef struct GsiHashBuilder {
  HashSym* sym;
  size_t sym_len;
  size_t sym_cap;

  HRFile* hash_records;
  size_t hash_records_len;

  u32* hash_buckets;
  size_t hash_buckets_len;
  size_t hash_buckets_cap;

  u32 hash_bitmap[(IPHR_HASH + 32) / 32];
} GsiHashBuilder;

typedef struct GsiBuilder {
  StreamData* public_hash_stream;
  StreamData* global_hash_stream;
  StreamData* sym_record_stream;

  GsiHashBuilder publics;
  GsiHashBuilder globals;
} GsiBuilder;

// The CodeView structs are all smooshed.
#pragma pack(push, 1)

#define CV_SYM_HEADER \
  u16 record_len;     \
  u16 record_type

typedef enum CV_S_PUB32_FLAGS {
  CVSPF_None = 0x00,
  CVSPF_Code = 0x01,
  CVSPF_Function = 0x02,
  CVSPF_Managed = 0x04,
  CVSPF_MSIL = 0x08,
} CV_S_PUB32_FLAGS;

typedef struct CV_S_PUB32 {
  CV_SYM_HEADER;
  u32 flags;
  u32 offset_into_codeseg;
  u16 segment;
  // unsigned char name[];
} CV_S_PUB32;

typedef struct CV_S_PROCREF {
  CV_SYM_HEADER;
  u32 sum_name;
  u32 offset_into_module_data;
  u16 segment;
  // unsigned char name[];
} CV_S_PROCREF;

typedef struct CV_S_OBJNAME {
  CV_SYM_HEADER;
  u32 signature;
  // unsigned char name[];
} CV_S_OBJNAME;

typedef struct CV_S_COMPILE3 {
  CV_SYM_HEADER;

  struct {
    u32 language : 8;         // language index
    u32 ec : 1;               // compiled for E/C
    u32 no_dbg_info : 1;      // not compiled with debug info
    u32 ltcg : 1;             // compiled with LTCG
    u32 no_data_align : 1;    // compiled with -Bzalign
    u32 managed_present : 1;  // managed code/data present
    u32 security_checks : 1;  // compiled with /GS
    u32 hot_patch : 1;        // compiled with /hotpatch
    u32 cvtcil : 1;           // converted with CVTCIL
    u32 msil_module : 1;      // MSIL netmodule
    u32 sdl : 1;              // compiled with /sdl
    u32 pgo : 1;              // compiled with /ltcg:pgo or pgu
    u32 exp : 1;              // .exp module
    u32 pad : 12;             // reserved, must be 0
  } flags;
  u16 machine;
  u16 ver_fe_major;
  u16 ver_fe_minor;
  u16 ver_fe_build;
  u16 ver_fe_qfe;
  u16 ver_be_major;
  u16 ver_be_minor;
  u16 ver_be_build;
  u16 ver_be_qfe;

  // unsigned char version[];
} CV_S_COMPILE3;

typedef struct CV_S_PROCFLAGS {
  union {
    unsigned char all;
    struct {
      unsigned char CV_PFLAG_NOFPO : 1;       // frame pointer present
      unsigned char CV_PFLAG_INT : 1;         // interrupt return
      unsigned char CV_PFLAG_FAR : 1;         // far return
      unsigned char CV_PFLAG_NEVER : 1;       // function does not return
      unsigned char CV_PFLAG_NOTREACHED : 1;  // label isn't fallen into
      unsigned char CV_PFLAG_CUST_CALL : 1;   // custom calling convention
      unsigned char CV_PFLAG_NOINLINE : 1;    // function marked as noinline
      unsigned char CV_PFLAG_OPTDBGINFO : 1;  // function has debug information for optimized code
    };
  };
} CV_S_PROCFLAGS;

typedef struct CV_S_GPROC32 {
  CV_SYM_HEADER;

  u32 parent;
  u32 end;
  u32 next;
  u32 len;
  u32 dbg_start;
  u32 dbg_end;
  u32 type_index;
  u32 offset;
  u16 seg;
  CV_S_PROCFLAGS flags;  // Proc flags

  // unsigned char name[];
} CV_S_GPROC32;

typedef enum CV_LineFlags {
  CF_LF_None = 0,
  CF_LF_HaveColumns = 1,
} CV_LineFlags;

typedef struct CV_LineFragmentHeader {
  u32 reloc_offset;
  u16 reloc_segment;
  u16 flags;  // CV_LineFlags
  u32 code_size;
} CV_LineFragmentHeader;

typedef struct CV_LineBlockFragmentHeader {
  u32 checksum_block_offset;  // Offset of file_checksum entry in file checksums buffer. The
                              // checksum entry then contains another offset into the string table
                              // of the actual name.
  u32 num_lines;
  u32 block_size;
  // CV_LineNumberEntry lines[num_lines];
  // Columns array goes here too, but we don't currently support that.
} CV_LineBlockFragmentHeader;

typedef struct CV_LineNumberEntry {
  u32 offset;               // Offset to start of code bytes for line number.
  u32 line_num_start : 24;  // Line where statement/expression starts.
  u32 delta_line_end : 7;   // Delta to line where statement ends (optional).
  u32 is_statement : 1;     // true if statement, false if expression.
} CV_LineNumberEntry;

typedef struct CV_S_NODATA {
  CV_SYM_HEADER;
} CV_S_NODATA;

typedef enum CV_FileChecksumKind {
  CV_FCSK_None,
  CV_FCSK_MD5,
  CV_FCSK_SHA1,
  CV_FCSK_SHA256,
} CV_FileChecksumKind;

typedef struct CV_FileChecksumEntryHeader {
  u32 filename_offset;
  unsigned char checksum_size;
  unsigned char checksum_kind;
} CV_FileChecksumEntryHeader;

typedef enum CV_DebugSubsectionKind {
  CV_DSF_Symbols = 0xf1,
  CV_DSF_Lines = 0xf2,
  CV_DSF_StringTable = 0xf3,
  CV_DSF_FileChecksums = 0xf4,
  // There are also others that we don't need.
} CV_DebugSubsectionKind;

typedef struct CV_DebugSubsectionHeader {
  u32 kind;    // CV_DebugSubsectionKind enum
  u32 length;  // includes data after, but not this struct.
} CV_DebugSubsectionHeader;

#define SW_CV_SYM_TRAILING_NAME(sym, name)                                                    \
  do {                                                                                        \
    u16 name_len = (u16)strlen(name) + 1; /* trailing \0 seems required in (most?) records */ \
    u16 record_len = (u16)align_to((u32)name_len + sizeof(sym), 4) -                          \
                     sizeof(u16) /* length field not included in length count */;             \
    sym.record_len = record_len;                                                              \
    assert(sym.record_type);                                                                  \
    SW_BLOCK(&sym, sizeof(sym));                                                              \
    SW_BLOCK(name, name_len);                                                                 \
    SW_ALIGN(4);                                                                              \
  } while (0)

#define SW_CV_SYM(sym)                                            \
  do {                                                            \
    u16 record_len = (u16)align_to(sizeof(sym), 4) - sizeof(u16); \
    sym.record_len = record_len;                                  \
    assert(sym.record_type);                                      \
    SW_BLOCK(&sym, sizeof(sym));                                  \
    /* No need to align because we should already be. */          \
  } while (0)

#pragma pack(pop)

static void gsi_builder_add_public(DbpContext* ctx,
                                   GsiBuilder* builder,
                                   CV_S_PUB32_FLAGS flags,
                                   u32 offset_into_codeseg,
                                   char* name) {
  StreamData* stream = builder->sym_record_stream;

  HashSym sym = {_strdup(name), stream->data_length, calc_hash(name, strlen(name)) % IPHR_HASH};
  PUSH_BACK(builder->publics.sym, sym);

  CV_S_PUB32 pub = {
      .record_type = 0x110e,
      .flags = (u32)flags,
      .offset_into_codeseg = offset_into_codeseg,
      .segment = 1  // segment is always 1 for us
  };
  SW_CV_SYM_TRAILING_NAME(pub, name);
}

static void gsi_builder_add_procref(DbpContext* ctx,
                                    GsiBuilder* builder,
                                    u32 offset_into_module_data,
                                    char* name) {
  StreamData* stream = builder->sym_record_stream;

  HashSym sym = {_strdup(name), stream->data_length, calc_hash(name, strlen(name)) % IPHR_HASH};
  PUSH_BACK(builder->globals.sym, sym);

  CV_S_PROCREF procref = {
      .record_type = 0x1125,
      .sum_name = 0,
      .offset_into_module_data = offset_into_module_data,
      .segment = 1,  // segment is always 1 for us
  };
  SW_CV_SYM_TRAILING_NAME(procref, name);
}

static int is_ascii_string(char* s) {
  for (unsigned char* p = (unsigned char*)s; *p; ++p) {
    if (*p >= 0x80)
      return 0;
  }
  return 1;
}

static int gsi_record_cmp(char* s1, char* s2) {
  // Not-at-all-Accidentally Quadratic, but rather Wantonly. :/
  size_t ls = strlen(s1);
  size_t rs = strlen(s2);
  if (ls != rs) {
    return (ls > rs) - (ls < rs);
  }

  // Non-ascii: memcmp.
  if (!is_ascii_string(s1) || !is_ascii_string(s2)) {
    return memcmp(s1, s2, ls);
  }

  // Otherwise case-insensitive (so random!).
  return _memicmp(s1, s2, ls);
}

// TODO: use a better sort impl
static HashSym* g_cur_hash_bucket_sort_syms = NULL;

// See caseInsensitiveComparePchPchCchCch() in microsoft-pdb gsi.cpp.
static int gsi_bucket_cmp(const void* a, const void* b) {
  const HRFile* hra = (const HRFile*)a;
  const HRFile* hrb = (const HRFile*)b;
  HashSym* left = &g_cur_hash_bucket_sort_syms[hra->off];
  HashSym* right = &g_cur_hash_bucket_sort_syms[hrb->off];
  assert(left->hash_bucket == right->hash_bucket);
  int cmp = gsi_record_cmp(left->name, right->name);
  if (cmp != 0) {
    return cmp < 0;
  }
  return left->offset < right->offset;
}

static void gsi_hash_builder_finish(GsiHashBuilder* hb) {
  // Figure out the exact bucket layout in the very arbitrary way that somebody
  // happened to decide on 30 years ago. The number of buckets in the
  // microsoft-pdb implementation is constant at IPHR_HASH afaict.

  // Figure out where each bucket starts.
  u32 bucket_starts[IPHR_HASH] = {0};
  {
    u32 num_mapped_to_bucket[IPHR_HASH] = {0};
    for (size_t i = 0; i < hb->sym_len; ++i) {
      ++num_mapped_to_bucket[hb->sym[i].hash_bucket];
    }

    u32 total = 0;
    for (size_t i = 0; i < IPHR_HASH; ++i) {
      bucket_starts[i] = total;
      total += num_mapped_to_bucket[i];
    }
  }

  // Put symbols into the table in bucket order, updating the bucket starts as
  // we go.
  u32 bucket_cursors[IPHR_HASH];
  memcpy(bucket_cursors, bucket_starts, sizeof(bucket_cursors));

  size_t num_syms = hb->sym_len;

  hb->hash_records = calloc(num_syms, sizeof(HRFile));
  hb->hash_records_len = num_syms;

  for (size_t i = 0; i < num_syms; ++i) {
    u32 hash_idx = bucket_cursors[hb->sym[i].hash_bucket]++;
    hb->hash_records[hash_idx].off = (u32)i;
    hb->hash_records[hash_idx].cref = 1;
  }

  g_cur_hash_bucket_sort_syms = hb->sym;
  // Sort each *bucket* (approximately) by the memcmp of the symbol's name. This
  // has to match microsoft-pdb, and it's bonkers. LLVM's implementation was
  // more helpful than microsoft-pdb's gsi.cpp for this one, and these hashes
  // aren't documented at all (in English) as of this writing as far as I know.
  for (size_t i = 0; i < IPHR_HASH; ++i) {
    size_t count = bucket_cursors[i] - bucket_starts[i];
    if (count > 0) {
      HRFile* begin = hb->hash_records + bucket_starts[i];
      qsort(begin, count, sizeof(HRFile), gsi_bucket_cmp);

      // Replace the indices with the stream offsets of each global, biased by 1
      // because 0 is treated specially.
      for (size_t j = 0; j < count; ++j) {
        begin[j].off = hb->sym[begin[j].off].offset + 1;
      }
    }
  }
  g_cur_hash_bucket_sort_syms = NULL;

  // Update the hash bitmap for each used bucket.
  for (u32 i = 0; i < sizeof(hb->hash_bitmap) / sizeof(hb->hash_bitmap[0]); ++i) {
    u32 word = 0;
    for (u32 j = 0; j < 32; ++j) {
      u32 bucket_idx = i * 32 + j;
      if (bucket_idx >= IPHR_HASH || bucket_starts[bucket_idx] == bucket_cursors[bucket_idx]) {
        continue;
      }
      word |= 1u << j;

      // Calculate what the offset of the first hash record int he chain would
      // be if it contained 32bit pointers: HROffsetCalc in microsoft-pdb gsi.h.
      u32 size_of_hr_offset_calc = 12;
      u32 chain_start_off = bucket_starts[bucket_idx] * size_of_hr_offset_calc;
      PUSH_BACK(hb->hash_buckets, chain_start_off);
    }
    hb->hash_bitmap[i] = word;
  }
}

static void gsi_hash_builder_write(DbpContext* ctx, GsiHashBuilder* hb, StreamData* stream) {
  SW_U32(0xffffffff);             // HdrSignature
  SW_U32(0xeffe0000 + 19990810);  // GSIHashSCImpv70
  SW_U32((u32)(hb->hash_records_len * sizeof(HRFile)));
  SW_U32((u32)(sizeof(hb->hash_bitmap) + hb->hash_buckets_len * sizeof(u32)));

  SW_BLOCK(hb->hash_records, hb->hash_records_len * sizeof(HRFile));
  SW_BLOCK(hb->hash_bitmap, sizeof(hb->hash_bitmap));
  SW_BLOCK(hb->hash_buckets, hb->hash_buckets_len * sizeof(u32));
}

static HashSym* g_cur_addr_map_sort_syms = NULL;
static int addr_map_cmp(const void* a, const void* b) {
  const u32* left_idx = (const u32*)a;
  const u32* right_idx = (const u32*)b;
  HashSym* left = &g_cur_addr_map_sort_syms[*left_idx];
  HashSym* right = &g_cur_addr_map_sort_syms[*right_idx];
  // Compare segment first, if we had one, but it's always 1.
  if (left->offset != right->offset)
    return left->offset < right->offset;
  return strcmp(left->name, right->name);
}

static void gsi_write_publics_stream(DbpContext* ctx, GsiHashBuilder* hb, StreamData* stream) {
  // microsoft-pdb PSGSIHDR first, then the hash table in the same format as
  // "globals" (gsi_hash_builder_write).
  u32 size_of_hash = (u32)(16 + (hb->hash_records_len * sizeof(HRFile)) + sizeof(hb->hash_bitmap) +
                           (hb->hash_buckets_len * sizeof(u32)));
  SW_U32(size_of_hash);                      // cbSymHash
  SW_U32((u32)(hb->sym_len * sizeof(u32)));  // cbAddrMap
  SW_U32(0);                                 // nThunks
  SW_U32(0);                                 // cbSizeOfThunk
  SW_U16(0);                                 // isectTunkTable
  SW_U16(0);                                 // padding
  SW_U32(0);                                 // offThunkTable
  SW_U32(0);                                 // nSects

  size_t before_hash_len = stream->data_length;

  gsi_hash_builder_write(ctx, hb, stream);

  size_t after_hash_len = stream->data_length;
  assert(after_hash_len - before_hash_len == size_of_hash &&
         "hash size calc doesn't match gsi_hash_builder_write");

  u32* addr_map = _alloca(sizeof(u32) * hb->sym_len);
  for (u32 i = 0; i < hb->sym_len; ++i)
    addr_map[i] = i;
  g_cur_addr_map_sort_syms = hb->sym;
  qsort(addr_map, hb->sym_len, sizeof(u32), addr_map_cmp);
  g_cur_addr_map_sort_syms = NULL;

  // Rewrite public symbol indices into symbol offsets.
  for (size_t i = 0; i < hb->sym_len; ++i) {
    addr_map[i] = hb->sym[addr_map[i]].offset;
  }

  SW_BLOCK(addr_map, hb->sym_len * sizeof(u32));
}

static void free_gsi_hash_builder(GsiHashBuilder* hb) {
  for (size_t i = 0; i < hb->sym_len; ++i) {
    free(hb->sym[i].name);
  }
  free(hb->sym);
  free(hb->hash_records);
  free(hb->hash_buckets);
}

static GsiData gsi_builder_finish(DbpContext* ctx, GsiBuilder* gsi) {
  gsi_hash_builder_finish(&gsi->publics);
  gsi_hash_builder_finish(&gsi->globals);

  gsi->global_hash_stream = add_stream(ctx);
  gsi_hash_builder_write(ctx, &gsi->globals, gsi->global_hash_stream);

  gsi->public_hash_stream = add_stream(ctx);
  gsi_write_publics_stream(ctx, &gsi->publics, gsi->public_hash_stream);

  GsiData result = {.global_symbol_stream = gsi->global_hash_stream->stream_index,
                    .public_symbol_stream = gsi->public_hash_stream->stream_index,
                    .sym_record_stream = gsi->sym_record_stream->stream_index};

  free_gsi_hash_builder(&gsi->publics);
  free_gsi_hash_builder(&gsi->globals);
  free(gsi);

  return result;
}

static GsiData build_gsi_data(DbpContext* ctx) {
  GsiBuilder* gsi = calloc(1, sizeof(GsiBuilder));
  gsi->sym_record_stream = add_stream(ctx);

  for (size_t i = 0; i < ctx->func_syms_len; ++i) {
    DbpFunctionSymbol* fs = ctx->func_syms[i];

    assert(fs->module_info_offset > 0 && "didn't write modi yet?");
    gsi_builder_add_procref(ctx, gsi, fs->module_info_offset, fs->name);

    gsi_builder_add_public(ctx, gsi, CVSPF_Function, fs->address, fs->name);
  }

  return gsi_builder_finish(ctx, gsi);
}

typedef struct ModuleData {
  StreamData* stream;
  u32 symbols_byte_size;
  u32 c13_byte_size;
} ModuleData;

typedef struct DebugLinesBlock {
  u32 checksum_buffer_offset;
  CV_LineNumberEntry* lines;
  size_t lines_len;
  size_t lines_cap;
} DebugLinesBlock;

typedef struct DebugLines {
  DebugLinesBlock* blocks;
  size_t blocks_len;
  size_t blocks_cap;

  u32 reloc_offset;
  u32 code_size;
} DebugLines;

static ModuleData write_module_stream(DbpContext* ctx) {
  StreamData* stream = add_stream(ctx);
  ModuleData module_data = {stream, 0, 0};

  u32 symbol_start = stream->data_length;

  SW_U32(4);  // Signature

  //
  // Symbols
  //
  CV_S_OBJNAME objname = {.record_type = 0x1101, .signature = 0};
  SW_CV_SYM_TRAILING_NAME(objname, synthetic_obj_name);

  CV_S_COMPILE3 compile3 = {
      .record_type = 0x113c,
      .flags = {.language = 0x00 /* CV_CFL_C */},
      .machine = 0xd0,  // x64
      .ver_fe_major = ctx->version_major,
      .ver_fe_minor = ctx->version_minor,
      .ver_fe_build = ctx->version_build,
      .ver_fe_qfe = ctx->version_qfe,
      .ver_be_major = ctx->version_major,
      .ver_be_minor = ctx->version_minor,
      .ver_be_build = ctx->version_build,
      .ver_be_qfe = ctx->version_qfe,
  };
  SW_CV_SYM_TRAILING_NAME(compile3, ctx->compiler_version_string);

  for (size_t i = 0; i < ctx->func_syms_len; ++i) {
    DbpFunctionSymbol* fs = ctx->func_syms[i];

    fs->module_info_offset = stream->data_length;

    CV_S_GPROC32 gproc32 = {
        .record_type = 0x1110,
        .parent = 0,
        .end = ~0U,
        .next = 0,
        .len = fs->length,
        .dbg_start = fs->address,   // not sure about these fields
        .dbg_end = fs->length - 1,  // not sure about these fields
        .type_index = 0x1001,       // hrm, first UDT, undefined but we're not writing types.
        .offset = fs->address,      /* address of proc */
        .seg = 1,
        .flags = {0},
    };
    SwFixup end_fixup = SW_CAPTURE_FIXUP(CV_S_GPROC32, end);
    SW_CV_SYM_TRAILING_NAME(gproc32, fs->name);

    CV_S_NODATA end = {.record_type = 0x0006};
    SW_WRITE_FIXUP_FOR_LOCATION_U32(end_fixup);
    SW_CV_SYM(end);
  }

  // TODO: could add all global data here too, but without types it's probably
  // pretty pointless.

  module_data.symbols_byte_size = stream->data_length - symbol_start;

  //
  // C13LineInfo
  //
  u32 c13_start = stream->data_length;

  // Need filename to offset-into-checksums block map. Record the name_index for
  // each string here, and find the index of name_index in this array. Then the
  // location where it's found in this array is the offset (times a constant
  // sizeof).
  u32* name_index_to_checksum_offset = _alloca(sizeof(u32) * ctx->names_nmt->num_names);

  // Checksums
  {
    // Write a block of checksums, except we actually write "None" checksums, so
    // it's just a table of indices pointed to by name_index above, that in turn
    // points to the offset into the /names NMT for the actual file name.
    size_t len = align_to((u32)sizeof(CV_FileChecksumEntryHeader), 4) * ctx->names_nmt->num_names;
    CV_DebugSubsectionHeader checksums_subsection_header = {.kind = CV_DSF_FileChecksums,
                                                            .length = align_to((u32)len, 4)};
    SW_BLOCK(&checksums_subsection_header, sizeof(checksums_subsection_header));

    // The layout of this section has to match how name_index_to_checksum_offset
    // is used above.
    NmtAlikeEnum it = nmtalike_enum_begin(ctx->names_nmt);
    size_t i = 0;
    while (nmtalike_enum_next(&it)) {
      u32 name_index;
      char* str;
      nmtalike_enum_get(&it, &name_index, &str);

      name_index_to_checksum_offset[i++] = name_index;

      CV_FileChecksumEntryHeader header = {
          .filename_offset = name_index, .checksum_size = 0, .checksum_kind = CV_FCSK_None};
      SW_BLOCK(&header, sizeof(header));
      SW_ALIGN(4);
    }
  }

  // Lines
  {
    size_t len = sizeof(CV_LineFragmentHeader);
    for (size_t i = 0; i < ctx->func_syms_len; ++i) {
      DbpFunctionSymbol* fs = ctx->func_syms[i];
      len += sizeof(CV_LineBlockFragmentHeader);
      len += fs->lines_len * sizeof(CV_LineNumberEntry);
    }

    CV_DebugSubsectionHeader lines_subsection_header = {.kind = CV_DSF_Lines,
                                                        .length = (u32)len};
    SW_BLOCK(&lines_subsection_header, sizeof(lines_subsection_header));

    CV_LineFragmentHeader header = {.code_size = (u32)ctx->image_size,
                                    .flags = CF_LF_None,
                                    .reloc_segment = 1,
                                    .reloc_offset = 0};
    SW_BLOCK(&header, sizeof(header));

    for (size_t i = 0; i < ctx->func_syms_len; ++i) {
      DbpFunctionSymbol* fs = ctx->func_syms[i];

      CV_LineBlockFragmentHeader block_header;
      block_header.num_lines = (u32)fs->lines_len;
      block_header.block_size = sizeof(CV_LineBlockFragmentHeader);
      block_header.block_size += block_header.num_lines * sizeof(CV_LineNumberEntry);
      u32 name_index = nmtalike_name_index_for_string(ctx->names_nmt, fs->filename);
      u32 offset = ~0u;
      for (size_t j = 0; j < ctx->names_nmt->num_names; ++j) {
        if (name_index_to_checksum_offset[j] == name_index) {
          offset = (u32)(align_to((u32)sizeof(CV_FileChecksumEntryHeader), 4) * j);
          break;
        }
      }
      assert(offset != ~0u && "didn't find filename");
      block_header.checksum_block_offset = offset;
      SW_BLOCK(&block_header, sizeof(block_header));

      for (size_t j = 0; j < fs->lines_len; ++j) {
        CV_LineNumberEntry line_entry = {.offset = fs->lines[j].address,
                                         .line_num_start = fs->lines[j].line,
                                         .delta_line_end = 0,
                                         .is_statement = 1};
        SW_BLOCK(&line_entry, sizeof(line_entry));
      }
    }

  }

  module_data.c13_byte_size = stream->data_length - c13_start;

  //
  // GlobalRefs, don't know, don't write it.
  //
  SW_U32(0);  // GlobalRefsSize

  return module_data;
}

static int write_directory(DbpContext* ctx) {
  u32 directory_page = alloc_block(ctx);

  u32* block_map = get_block_ptr(ctx, ctx->superblock->block_map_addr);
  *block_map = directory_page;

  u32* start = get_block_ptr(ctx, directory_page);
  u32* dir = start;

  // Starts with number of streams.
  *dir++ = (u32)ctx->stream_data_len;

  // Then, the number of blocks in each stream.
  for (size_t i = 0; i < ctx->stream_data_len; ++i) {
    *dir++ = ctx->stream_data[i]->data_length;
    ENSURE((ctx->stream_data[i]->data_length + BLOCK_SIZE - 1) / BLOCK_SIZE,
           ctx->stream_data[i]->blocks_len);
  }

  // Then the list of blocks for each stream.
  for (size_t i = 0; i < ctx->stream_data_len; ++i) {
    for (size_t j = 0; j < ctx->stream_data[i]->blocks_len; ++j) {
      *dir++ = ctx->stream_data[i]->blocks[j];
    }
  }

  // And finally, update the super block with the number of bytes in the
  // directory.
  ctx->superblock->num_directory_bytes = (u32)((u32)(dir - start) * sizeof(u32));

  // This can't easily use StreamData because it's the directory of streams. It
  // would take a larger pdb that we expect to be writing here to overflow the
  // first block (especially since we don't write types), so just assert that we
  // didn't grow too large for now.
  if (ctx->superblock->num_directory_bytes > BLOCK_SIZE) {
    fprintf(stderr, "%s:%d: directory grew beyond BLOCK_SIZE\n", __FILE__, __LINE__);
    return 0;
  }

  return 1;
}

static int create_file_map(DbpContext* ctx) {
  ctx->file = CreateFile(ctx->output_pdb_name, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                         FILE_ATTRIBUTE_NORMAL, NULL);
  ENSURE_NE(ctx->file, INVALID_HANDLE_VALUE);

  ctx->file_size = BLOCK_SIZE * DEFAULT_NUM_BLOCKS;  // TODO: grow mapping as necessary
  ENSURE(SetFilePointer(ctx->file, (LONG)ctx->file_size, NULL, FILE_BEGIN), ctx->file_size);

  ENSURE(1, SetEndOfFile(ctx->file));

  HANDLE map_object = CreateFileMapping(ctx->file, NULL, PAGE_READWRITE, 0, 0, NULL);
  ENSURE_NE(map_object, NULL);

  ctx->data = MapViewOfFileEx(map_object, FILE_MAP_ALL_ACCESS, 0, 0, 0, NULL);
  ENSURE_NE(ctx->data, NULL);

  ENSURE(CloseHandle(map_object), 1);

  return 1;
}

typedef struct RsdsDataHeader {
  unsigned char magic[4];
  UUID unique_id;
  u32 age;
  // unsigned char name[]
} RsdsDataHeader;

static void file_fill_to_next_page(FILE* f, unsigned char with) {
  long long align_count = ftell(f);
  while (align_count % 0x200 != 0) {
    fwrite(&with, sizeof(with), 1, f);
    ++align_count;
  }
}

static int write_stub_dll(DbpContext* ctx, DbpExceptionTables* exception_tables) {
  FILE* f;
  if (fopen_s(&f, ctx->output_dll_name, "wb") != 0) {
    fprintf(stderr, "couldn't open %s\n", ctx->output_dll_name);
    return 0;
  }
  DWORD timedate = (DWORD)time(NULL);
  static unsigned char dos_stub_and_pe_magic[172] = {
      0x4d, 0x5a, 0x90, 0x00, 0x03, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0xff, 0xff, 0x00,
      0x00, 0xb8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0xa8, 0x00, 0x00, 0x00, 0x0e, 0x1f, 0xba, 0x0e, 0x00, 0xb4, 0x09, 0xcd, 0x21, 0xb8, 0x01,
      0x4c, 0xcd, 0x21, 0x54, 0x68, 0x69, 0x73, 0x20, 0x70, 0x72, 0x6f, 0x67, 0x72, 0x61, 0x6d,
      0x20, 0x63, 0x61, 0x6e, 0x6e, 0x6f, 0x74, 0x20, 0x62, 0x65, 0x20, 0x72, 0x75, 0x6e, 0x20,
      0x69, 0x6e, 0x20, 0x44, 0x4f, 0x53, 0x20, 0x6d, 0x6f, 0x64, 0x65, 0x2e, 0x0d, 0x0d, 0x0a,
      0x24, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x61, 0x46, 0x33, 0xdf, 0x25, 0x27, 0x5d,
      0x8c, 0x25, 0x27, 0x5d, 0x8c, 0x25, 0x27, 0x5d, 0x8c, 0xe4, 0x5b, 0x59, 0x8d, 0x24, 0x27,
      0x5d, 0x8c, 0xe4, 0x5b, 0x5f, 0x8d, 0x24, 0x27, 0x5d, 0x8c, 0x52, 0x69, 0x63, 0x68, 0x25,
      0x27, 0x5d, 0x8c, 'P',  'E',  '\0', '\0',
  };
  // PE pointer is a 0x3c, points at 0xa8, IMAGE_FILE_HEADER starts there.
  fwrite(dos_stub_and_pe_magic, sizeof(dos_stub_and_pe_magic), 1, f);
  IMAGE_FILE_HEADER image_file_header = {
      .Machine = IMAGE_FILE_MACHINE_AMD64,
      .NumberOfSections = 2 + (exception_tables ? 2 : 0),
      .TimeDateStamp = timedate,
      .PointerToSymbolTable = 0,
      .NumberOfSymbols = 0,
      .SizeOfOptionalHeader = sizeof(IMAGE_OPTIONAL_HEADER64),
      .Characteristics = IMAGE_FILE_RELOCS_STRIPPED | IMAGE_FILE_EXECUTABLE_IMAGE |
                         IMAGE_FILE_LARGE_ADDRESS_AWARE | IMAGE_FILE_DLL,
  };

  DWORD pdata_length =
      exception_tables ? (DWORD)(exception_tables->num_pdata_entries * sizeof(DbpRUNTIME_FUNCTION))
                       : 0;
  DWORD pdata_length_page_aligned = align_to(pdata_length, 0x1000);
  DWORD pdata_length_file_aligned = align_to(pdata_length, 0x200);

  DWORD xdata_length = exception_tables ? (DWORD)exception_tables->unwind_info_byte_length : 0;
  DWORD xdata_length_page_aligned = align_to(xdata_length, 0x1000);
  DWORD xdata_length_file_aligned = align_to(xdata_length, 0x200);

  DWORD code_start = 0x1000;
  DWORD rdata_virtual_start = (DWORD)(code_start + ctx->image_size);
  DWORD pdata_virtual_start = rdata_virtual_start + STUB_RDATA_SIZE;
  DWORD xdata_virtual_start = pdata_virtual_start + pdata_length_page_aligned;
  fwrite(&image_file_header, sizeof(image_file_header), 1, f);
  IMAGE_OPTIONAL_HEADER64 opt_header = {
      .Magic = IMAGE_NT_OPTIONAL_HDR64_MAGIC,
      .MajorLinkerVersion = 14,  // Matches DBI stream.
      .MinorLinkerVersion = 11,
      .SizeOfCode = 0x200,
      .SizeOfInitializedData = 0x200,
      .SizeOfUninitializedData = 0,
      .AddressOfEntryPoint = 0,
      .BaseOfCode = 0x1000,
      .ImageBase = (ULONGLONG)ctx->base_addr,
      .SectionAlignment = 0x1000,
      .FileAlignment = 0x200,
      .MajorOperatingSystemVersion = 6,
      .MinorOperatingSystemVersion = 0,
      .MajorImageVersion = 6,
      .MinorImageVersion = 0,
      .MajorSubsystemVersion = 6,
      .MinorSubsystemVersion = 0,
      .Win32VersionValue = 0,
      // The documentation makes this seem like the size of the file, but I
      // think it's actually the virtual space occupied to the end of the
      // sections when loaded.
      .SizeOfImage = xdata_virtual_start + xdata_length_page_aligned, // xdata is the last section.
      .SizeOfHeaders = 0x400,  // Address of where the section data starts.
      .CheckSum = 0,
      .Subsystem = IMAGE_SUBSYSTEM_WINDOWS_GUI,
      .DllCharacteristics =
          IMAGE_DLLCHARACTERISTICS_NX_COMPAT | IMAGE_DLLCHARACTERISTICS_HIGH_ENTROPY_VA,
      .SizeOfStackReserve = 0x100000,
      .SizeOfStackCommit = 0x1000,
      .SizeOfHeapReserve = 0x100000,
      .SizeOfHeapCommit = 0x1000,
      .LoaderFlags = 0,
      .NumberOfRvaAndSizes = 0x10,
      .DataDirectory =
          {
              {0, 0},
              {0, 0},
              {0, 0},
              {exception_tables ? pdata_virtual_start : 0, (DWORD)pdata_length},
              {0, 0},
              {0, 0},
              {rdata_virtual_start, sizeof(IMAGE_DEBUG_DIRECTORY)},
              {0, 0},
              {0, 0},
              {0, 0},
              {0, 0},
              {0, 0},
              {0, 0},
              {0, 0},
              {0, 0},
              {0, 0},
          },
  };
  fwrite(&opt_header, sizeof(opt_header), 1, f);

  //
  // .text header
  //
  IMAGE_SECTION_HEADER text = {
      .Name = ".text\0\0",
      .Misc = {.VirtualSize = (DWORD)ctx->image_size},
      .VirtualAddress = code_start,
      .SizeOfRawData = 0x200,     // This is the size of the block of .text in the dll.
      .PointerToRawData = 0x400,  // Aligned address in file.
      .PointerToRelocations = 0,
      .PointerToLinenumbers = 0,
      .NumberOfRelocations = 0,
      .NumberOfLinenumbers = 0,
      .Characteristics = IMAGE_SCN_CNT_CODE | IMAGE_SCN_MEM_EXECUTE | IMAGE_SCN_MEM_READ,
  };
  fwrite(&text, sizeof(text), 1, f);

  //
  // .rdata header
  //
  RsdsDataHeader rsds_header = {
      .magic =
          {
              'R',
              'S',
              'D',
              'S',
          },
      .age = 1,
  };
  memcpy(&rsds_header.unique_id, &ctx->unique_id, sizeof(UUID));
  size_t name_len = strlen(ctx->output_pdb_name);
  DWORD rsds_len = (DWORD)(sizeof(rsds_header) + name_len + 1);

  IMAGE_SECTION_HEADER rdata = {
      .Name = ".rdata\0",
      .Misc = {.VirtualSize = (DWORD)(rsds_len + sizeof(IMAGE_DEBUG_DIRECTORY))},
      .VirtualAddress = rdata_virtual_start,
      .SizeOfRawData = 0x200,
      .PointerToRawData = 0x600,
      .PointerToRelocations = 0,
      .PointerToLinenumbers = 0,
      .NumberOfRelocations = 0,
      .NumberOfLinenumbers = 0,
      .Characteristics = IMAGE_SCN_CNT_INITIALIZED_DATA | IMAGE_SCN_MEM_READ,
  };
  fwrite(&rdata, sizeof(rdata), 1, f);

  if (exception_tables) {
    //
    // .pdata header
    //
    IMAGE_SECTION_HEADER pdata = {
        .Name = ".pdata\0",
        .Misc = {.VirtualSize = (DWORD)pdata_length},
        .VirtualAddress = pdata_virtual_start,
        .SizeOfRawData = pdata_length_file_aligned,
        .PointerToRawData = 0x800,  // Aligned address in file.
        .PointerToRelocations = 0,
        .PointerToLinenumbers = 0,
        .NumberOfRelocations = 0,
        .NumberOfLinenumbers = 0,
        .Characteristics = IMAGE_SCN_CNT_INITIALIZED_DATA | IMAGE_SCN_MEM_READ,
    };
    fwrite(&pdata, sizeof(pdata), 1, f);

    //
    // .xdata header
    //
    IMAGE_SECTION_HEADER xdata = {
        .Name = ".xdata\0",
        .Misc = {.VirtualSize = (DWORD)xdata_length},
        .VirtualAddress = xdata_virtual_start,
        .SizeOfRawData = xdata_length_file_aligned,
        .PointerToRawData = 0xa00,  // Aligned address in file.
        .PointerToRelocations = 0,
        .PointerToLinenumbers = 0,
        .NumberOfRelocations = 0,
        .NumberOfLinenumbers = 0,
        .Characteristics = IMAGE_SCN_CNT_INITIALIZED_DATA | IMAGE_SCN_MEM_READ,
    };
    fwrite(&xdata, sizeof(xdata), 1, f);
  } else {
    unsigned char zero = 0;
    fwrite(&zero, 1, 1, f);
  }

  file_fill_to_next_page(f, 0);
  assert(ftell(f) == 0x400);

  //
  // contents of .text
  //

  unsigned char int3 = 0xcc;
  fwrite(&int3, sizeof(int3), 1, f);
  file_fill_to_next_page(f, int3);
  assert(ftell(f) == 0x600);

  //
  // contents of .rdata
  //
  long long rdata_file_start = ftell(f);

  // Now the .rdata data which points to the pdb.
  IMAGE_DEBUG_DIRECTORY debug_dir = {
      .Characteristics = 0,
      .TimeDateStamp = timedate,
      .MajorVersion = 0,
      .MinorVersion = 0,
      .Type = IMAGE_DEBUG_TYPE_CODEVIEW,
      .SizeOfData = rsds_len,
      .AddressOfRawData = rdata_virtual_start + 0x1c,
      .PointerToRawData = (DWORD)(rdata_file_start + (long)sizeof(IMAGE_DEBUG_DIRECTORY)),
  };
  fwrite(&debug_dir, sizeof(debug_dir), 1, f);
  fwrite(&rsds_header, sizeof(rsds_header), 1, f);
  fwrite(ctx->output_pdb_name, 1, name_len + 1, f);

  file_fill_to_next_page(f, 0);
  assert(ftell(f) == 0x800);

  if (exception_tables) {
    //
    // contents of .pdata
    //
    for (size_t i = 0; i < exception_tables->num_pdata_entries; ++i) {
      exception_tables->pdata[i].begin_address += code_start;  // Fixup to .text RVA.
      exception_tables->pdata[i].end_address += code_start;    // Fixup to .text RVA.
      exception_tables->pdata[i].unwind_data += xdata_virtual_start;  // Fixup to .xdata RVA.
    }
    fwrite(exception_tables->pdata, sizeof(DbpRUNTIME_FUNCTION), exception_tables->num_pdata_entries,
        f);
    file_fill_to_next_page(f, 0);

    //
    // contents of .xdata
    //
    fwrite(exception_tables->unwind_info, 1, exception_tables->unwind_info_byte_length, f);
    file_fill_to_next_page(f, 0);
  }

  fclose(f);

  return 1;
}

static int force_symbol_load(DbpContext* ctx, DbpExceptionTables* exception_tables) {
  // Write stub dll with target address/size and fixed base address.
  ENSURE(write_stub_dll(ctx, exception_tables), 1);

  // Save current code block and then VirtualFree() it.
  void* tmp = malloc(ctx->image_size);
  memcpy(tmp, ctx->image_addr, ctx->image_size);
  VirtualFree(ctx->base_addr, 0, MEM_RELEASE);

  // There's a race here with other threads, but... I think that's the least of
  // our problems.
  ctx->dll_module = LoadLibraryEx(ctx->output_dll_name, NULL, DONT_RESOLVE_DLL_REFERENCES);

  // Make DLL writable and slam jitted code back into same location.
  DWORD old_protect;
  ENSURE(VirtualProtect(ctx->image_addr, ctx->image_size, PAGE_READWRITE, &old_protect), 1);

  memcpy(ctx->image_addr, tmp, ctx->image_size);
  free(tmp);

  ENSURE(VirtualProtect(ctx->image_addr, ctx->image_size, PAGE_EXECUTE_READ, &old_protect), 1);

  return 1;
}

int dbp_ready_to_execute(DbpContext* ctx, DbpExceptionTables* exception_tables) {
  if (!create_file_map(ctx))
    return 0;

  write_superblock(ctx);

  // Stream 0: "Old MSF Directory", empty.
  add_stream(ctx);

  // Stream 1: PDB Info Stream.
  StreamData* stream1 = add_stream(ctx);

  // Stream 2: TPI Stream.
  StreamData* stream2 = add_stream(ctx);
  ENSURE(1, write_empty_tpi_ipi_stream(ctx, stream2));

  // Stream 3: DBI Stream.
  StreamData* stream3 = add_stream(ctx);

  // Stream 4: IPI Stream.
  StreamData* stream4 = add_stream(ctx);
  ENSURE(write_empty_tpi_ipi_stream(ctx, stream4), 1);

  // "/names": named, so stream index doesn't matter.
  StreamData* names_stream = add_stream(ctx);
  ENSURE(write_names_stream(ctx, names_stream), 1);

  // Names must be written before module, because the line info refers to the
  // source files names (by offset into /names).
  ModuleData module_data = write_module_stream(ctx);

  // And the module stream must be written before the GSI stream because the
  // global procrefs contain the offset into the module data to locate the
  // actual function symbol.
  GsiData gsi_data = build_gsi_data(ctx);

  // Section Headers; empty. Referred to by DBI in 'optional' dbg headers, and
  // llvm-pdbutil wants it to exist, but handles an empty stream reasonably.
  StreamData* section_headers = add_stream(ctx);

  DbiWriteData dwd = {
      .gsi_data = gsi_data,
      .section_header_stream = section_headers->stream_index,
      .module_sym_stream = module_data.stream->stream_index,
      .module_symbols_byte_size = module_data.symbols_byte_size,
      .module_c13_byte_size = module_data.c13_byte_size,
      .num_source_files = 1,
  };
  ENSURE(write_dbi_stream(ctx, stream3, &dwd), 1);
  ENSURE(write_pdb_info_stream(ctx, stream1, names_stream->stream_index), 1);

  ENSURE(write_directory(ctx), 1);

  ENSURE(FlushViewOfFile(ctx->data, ctx->file_size), 1);
  ENSURE(UnmapViewOfFile(ctx->data), 1);
  CloseHandle(ctx->file);

  ENSURE(force_symbol_load(ctx, exception_tables), 1);

  return 1;
}

void dbp_free(DbpContext* ctx) {
  FreeLibrary(ctx->dll_module);

  for (size_t i = 0; i < ctx->func_syms_len; ++i) {
    free(ctx->func_syms[i]->name);
    free(ctx->func_syms[i]->filename);
    free(ctx->func_syms[i]->lines);
    free(ctx->func_syms[i]);
  }
  free(ctx->func_syms);
  free(ctx->output_pdb_name);
  free(ctx->output_dll_name);

  for (size_t i = 0; i < ctx->stream_data_len; ++i) {
    free(ctx->stream_data[i]->blocks);
    free(ctx->stream_data[i]);
  }
  free(ctx->stream_data);
  free(ctx->compiler_version_string);

  free(ctx->names_nmt->strings);
  free(ctx->names_nmt->hash);
  free(ctx->names_nmt);

  free(ctx);
}

#endif
