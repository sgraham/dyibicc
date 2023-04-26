#ifndef INCLUDED_DYN_BASIC_PDB_H
#define INCLUDED_DYN_BASIC_PDB_H

// In exactly *one* C file:
//
// #define DYN_BASIC_PDB_IMPLEMENTATION
// #include "dyn_basic_pdb.h"

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

typedef struct DbpContext DbpContext;
typedef struct DbpSourceFile DbpSourceFile;

DbpContext* dbp_create(void* image_addr, size_t image_size, const char* output_pdb_name);
DbpSourceFile* dbp_add_source_file(DbpContext* ctx, const char* name);
int dbp_add_line_mapping(DbpSourceFile* src,
                          unsigned int line_number,
                          unsigned int begin_addr,
                          unsigned int end_addr);
int dbp_finish(DbpContext* ctx);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // INCLUDED_DYN_BASIC_PDB_H

#ifdef DYN_BASIC_PDB_IMPLEMENTATION

#define _CRT_SECURE_NO_WARNINGS
#pragma warning(disable: 4668) // 'X' is not defined as a preprocessor macro, replacing with '0' for '#if/#elif'
#pragma warning(disable: 5045) // Compiler will insert Spectre mitigation for memory load if /Qspectre switch specified
#pragma comment(lib, "rpcrt4")

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <windows.h>

#include "str3.h"
#include "str4.h"
#include "str6.h"
#include "str7.h"
#include "str8.h"
#include "str9.h"
#include "str10.h"
#include "str11.h"
#include "str12.h"
#include "str13.h"
#include "str14.h"

typedef unsigned int u32;
typedef signed int i32;
typedef unsigned short u16;
typedef unsigned __int64 u64;

typedef struct SuperBlock SuperBlock;

typedef struct StreamData {
  u32 data_length;

  u32* blocks;
  size_t blocks_len;
  size_t blocks_cap;
} StreamData;

struct DbpContext {
  void* image_addr;
  size_t image_size;
  char* output_pdb_name;
  DbpSourceFile** source_files;
  size_t source_files_len;
  size_t source_files_cap;

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

struct DbpSourceFile {
  char* name;
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

DbpContext* dbp_create(void* image_addr, size_t image_size, const char* output_pdb_name) {
  DbpContext* ctx = calloc(1, sizeof(DbpContext));
  ctx->image_addr = image_addr;
  ctx->image_size = image_size;
  ctx->output_pdb_name = _strdup(output_pdb_name);
  return ctx;
}

DbpSourceFile* dbp_add_source_file(DbpContext* ctx, const char* name) {
  DbpSourceFile* ret = calloc(1, sizeof(DbpSourceFile));
  ret->name = _strdup(name);
  PUSH_BACK(ctx->source_files, ret);
  return ret;
}

int dbp_add_line_mapping(DbpSourceFile* src,
                          unsigned int line_number,
                          unsigned int begin_addr,
                          unsigned int end_addr) {
  (void)src;
  (void)line_number;
  (void)begin_addr;
  (void)end_addr;
  return 1;
}

static void free_ctx(DbpContext* ctx) {
  for (size_t i = 0; i < ctx->source_files_len; ++i) {
    free(ctx->source_files[i]->name);
  }
  free(ctx->source_files);
  free(ctx->output_pdb_name);
}

static const char BigHdrMagic[0x1e] = "Microsoft C/C++ MSF 7.00\r\n\x1a\x44\x53";

#define ENSURE(x, want)                                                             \
  if (want != x) {                                                                  \
    fprintf(stderr, "%s:%d: failed %s wasn't %s\n", __FILE__, __LINE__, #x, #want); \
    return 0;                                                                       \
  }

#define ENSURE_NE(x, bad)                                                        \
  if (bad == x) {                                                                \
    fprintf(stderr, "%s:%d: failed %s was %s\n", __FILE__, __LINE__, #x, #bad); \
    return 0;                                                                    \
  }

struct SuperBlock {
  char FileMagic[0x1e];
  char padding[2];
  u32 BlockSize;
  u32 FreeBlockMapBlock;
  u32 NumBlocks;
  u32 NumDirectoryBytes;
  u32 Unknown;
  u32 BlockMapAddr;
};

#define BLOCK_SIZE 4096
#define CTX DbpContext* ctx
#define PAGE_TO_WORD(pn) (pn >> 6)
#define PAGE_MASK(pn) (1ULL << (pn & ((sizeof(u64) * CHAR_BIT) - 1)))

static void mark_block_used(CTX, u32 pn) {
  u64* map2 = (u64*)&ctx->data[BLOCK_SIZE * 2];
  map2[PAGE_TO_WORD(pn)] &= ~PAGE_MASK(pn);
}

static int block_is_free(CTX, u32 pn) {
  u64* map2 = (u64*)&ctx->data[BLOCK_SIZE * 2];
  return !!(map2[PAGE_TO_WORD(pn)] & PAGE_MASK(pn));
}

static void* get_block_ptr(CTX, u32 i) {
  return &ctx->data[BLOCK_SIZE * i];
}

static u32 alloc_block(CTX) {
  for (;;) {
    if (block_is_free(ctx, ctx->next_potential_block)) {
      mark_block_used(ctx, ctx->next_potential_block);
      return ctx->next_potential_block++;
    }
    ctx->next_potential_block++;
  }
}

static void write_superblock(CTX) {
  SuperBlock* sb = (SuperBlock*)ctx->data;
  ctx->superblock = sb;
  memcpy(sb->FileMagic, BigHdrMagic, sizeof(BigHdrMagic));
  sb->padding[0] = '\0';
  sb->padding[1] = '\0';
  sb->BlockSize = BLOCK_SIZE;
  sb->FreeBlockMapBlock = 2;  // We never use map 1.
  sb->NumBlocks = 128;
  // NumDirectoryBytes filled in later once we've written everything else.
  sb->Unknown = 0;
  sb->BlockMapAddr = 3;

  // Mark all pages as free, then mark the first four in use:
  // 0 is super block, 1 is FPM1, 2 is FPM2, 3 is the block map.
  memset(&ctx->data[BLOCK_SIZE], 0xff, BLOCK_SIZE * 2);
  for (u32 i = 0; i <= 3; ++i)
    mark_block_used(ctx, i);
}

static StreamData* add_stream(CTX) {
  StreamData* stream = calloc(1, sizeof(StreamData));
  PUSH_BACK(ctx->stream_data, stream);
  return stream;
}

typedef struct PdbStreamHeader {
  u32 Version;
  u32 Signature;
  u32 Age;
  UUID UniqueId;
} PdbStreamHeader;

static int write_pdb_info_stream(CTX, StreamData* stream, u32 names_stream) {
  u32 block_id = alloc_block(ctx);
  PUSH_BACK(stream->blocks, block_id);
  char* start = get_block_ptr(ctx, block_id);
  char* block_ptr = start;
  PdbStreamHeader* psh = (PdbStreamHeader*)block_ptr;
  psh->Version = 20000404; /* VC70 */
  psh->Signature = (u32)time(NULL);
  psh->Age = 1;
  ENSURE(UuidCreate(&psh->UniqueId), RPC_S_OK);

  block_ptr += sizeof(PdbStreamHeader);

  // Named Stream Map.

  // The LLVM docs are something that would be nice to refer to here:
  //
  //   https://llvm.org/docs/PDB/HashTable.html
  //
  // But unfortunately, they're quite misleading. The microsoft-pdb repo is,
  // uh, "dense", but (obviously) correct:
  //
  // https://github.com/microsoft/microsoft-pdb/blob/082c5290e5aff028ae84e43affa8be717aa7af73/PDB/include/nmtni.h#L77-L95
  // https://github.com/microsoft/microsoft-pdb/blob/082c5290e5aff028ae84e43affa8be717aa7af73/PDB/include/map.h#L474-L508
  //
  // Someone naturally already figured this out, as LLVM writes the correct
  // data, just the docs are wrong. (I didn't see a way to fix the docs without
  // cloning the repo and figuring out their patch system, which is why I'm
  // whining here instead of just fixing it...)

  // Starts with the string buffer (which we pad to % 4, even though that's not
  // actually required). We don't bother with actually building and updating a
  // map as the only two named streams we need are these two (/LinkInfo and
  // /names).
  static const char string_data[] = "/LinkInfo\0/names\0\0\0";
  *(u32*)block_ptr = sizeof(string_data);
  block_ptr += sizeof(u32);
  memcpy(block_ptr, string_data, sizeof(string_data));
  block_ptr += sizeof(string_data);

  // Then hash size, and capacity (capacity is seemingly irrelevant).
  u32* hash = (u32*)block_ptr;
  *hash++ = 2; // Size
  *hash++ = 4; // Capacity
  // Then two bit vectors, first for "present":
  *hash++ = 0x01; // Present length (1 word follows)
  *hash++ = 0x06; // 0b0000`0110    (second and third buckets occupied)
  // then for "deleted" (we don't write any).
  *hash++ = 0;    // Deleted length.
  // Now, the maps: mapping "/names" at offset 0xa above to given names stream.
  *hash++ = 0xa;
  *hash++ = names_stream;
  // And mapping "/LinkInfo" at (offset 0) to Stream 5 (hardcoded since we just
  // leave it empty anyway.)
  *hash++ = 0;
  *hash++ = 5;
  // This is "niMac", which is the last index allocated. We don't need it.
  *hash++ = 0;

  // Finally, feature codes, which indicate that we're somewhat modern.
  // TODO: potentially don't declare this in order to drop TPI/IPI?
  *hash++ = 20140508; /* VC140 */

  block_ptr = (char*)hash;
  stream->data_length = block_ptr - start;
  return 1;
}

typedef struct TpiStreamHeader {
  u32 Version;
  u32 HeaderSize;
  u32 TypeIndexBegin;
  u32 TypeIndexEnd;
  u32 TypeRecordBytes;

  u16 HashStreamIndex;
  u16 HashAuxStreamIndex;
  u32 HashKeySize;
  u32 NumHashBuckets;

  i32 HashValueBufferOffset;
  u32 HashValueBufferLength;

  i32 IndexOffsetBufferOffset;
  u32 IndexOffsetBufferLength;

  i32 HashAdjBufferOffset;
  u32 HashAdjBufferLength;
} TpiStreamHeader;

static int write_empty_tpi_ipi_stream(CTX, StreamData* stream) {
  u32 block_id = alloc_block(ctx);
  PUSH_BACK(stream->blocks, block_id);

  // This is an "empty" TPI/IPI stream, we do not emit any user-defined types
  // currently.
  char* start = get_block_ptr(ctx, block_id);
  TpiStreamHeader* tsh = (TpiStreamHeader*)start;
  tsh->Version = 20040203; /* V80 */
  tsh->HeaderSize = sizeof(TpiStreamHeader);
  tsh->TypeIndexBegin = 0x1000;
  tsh->TypeIndexEnd = 0x1000;
  tsh->TypeRecordBytes = 0;
  tsh->HashStreamIndex = -1;
  tsh->HashAuxStreamIndex = -1;
  tsh->HashKeySize = 4;
  tsh->NumHashBuckets = 0x3ffff;
  tsh->HashValueBufferOffset = 0;
  tsh->HashValueBufferLength = 0;
  tsh->IndexOffsetBufferOffset = 0;
  tsh->IndexOffsetBufferLength = 0;
  tsh->HashAdjBufferOffset = 0;
  tsh->HashAdjBufferLength = 0;

  stream->data_length = sizeof(TpiStreamHeader);
  return 1;
}

static int write_directory(CTX) {
  // TODO: Handle overflow past BLOCK_SIZE.
  u32 directory_page = alloc_block(ctx);

  u32* block_map = get_block_ptr(ctx, ctx->superblock->BlockMapAddr);
  *block_map = directory_page;

  u32* start = get_block_ptr(ctx, directory_page);
  u32* dir = start;

  // Starts with number of streams.
  *dir++ = ctx->stream_data_len;

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
  ctx->superblock->NumDirectoryBytes = (dir - start) * sizeof(u32);

  return 1;
}

static int create_file_map(CTX) {
  ctx->file = CreateFile(ctx->output_pdb_name, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                         FILE_ATTRIBUTE_NORMAL, NULL);
  ENSURE_NE(ctx->file, INVALID_HANDLE_VALUE);

  ctx->file_size = BLOCK_SIZE * 128;  // TODO: 128 pages to start (see superblock)
  ENSURE(SetFilePointer(ctx->file, (LONG)ctx->file_size, NULL, FILE_BEGIN), ctx->file_size);

  ENSURE(1, SetEndOfFile(ctx->file));

  HANDLE map_object = CreateFileMapping(ctx->file, NULL, PAGE_READWRITE, 0, 0, NULL);
  ENSURE_NE(map_object, NULL);

  ctx->data = MapViewOfFileEx(map_object, FILE_MAP_ALL_ACCESS, 0, 0, 0, NULL);
  ENSURE_NE(ctx->data, NULL);

  ENSURE(CloseHandle(map_object), 1);

  return 1;
}

int dbp_finish(DbpContext* ctx) {
  if (!create_file_map(ctx))
    return 0;

  write_superblock(ctx);

  // Stream 0: "Old MSF Directory", empty.
  add_stream(ctx);

  // Stream 1: PDB Info Stream.
  StreamData* stream1 = add_stream(ctx);

  // Stream 2: TPI Stream.
  StreamData* stream2 = add_stream(ctx);

#define HACK(stream, block)                                                      \
  {                                                                              \
    StreamData* sd##stream = calloc(1, sizeof(StreamData));                      \
    sd##stream->data_length = str##stream##_raw_len;                             \
    PUSH_BACK(sd##stream->blocks, block);                                        \
    PUSH_BACK(ctx->stream_data, sd##stream);                                     \
    mark_block_used(ctx, block);                                                 \
    memcpy(get_block_ptr(ctx, block), str##stream##_raw, str##stream##_raw_len); \
  }

  HACK(3, 12);

  // Stream 4: IPI Stream.
  StreamData* stream4 = add_stream(ctx);

  // Stream 5: "/LinkInfo", empty.
  add_stream(ctx);

  HACK(6, 4);
  HACK(7, 5);
  HACK(8, 6);
  HACK(9, 8);
  HACK(10, 9);
  HACK(11, 10);
  HACK(12, 11);
  HACK(13, 13);
  HACK(14, 15);

  ENSURE(1, write_empty_tpi_ipi_stream(ctx, stream2));
  ENSURE(1, write_empty_tpi_ipi_stream(ctx, stream4));
  ENSURE(1, write_pdb_info_stream(ctx, stream1, /*names_stream=*/13));

  ENSURE(1, write_directory(ctx));

  ENSURE(FlushViewOfFile(ctx->data, ctx->file_size), 1);
  ENSURE(UnmapViewOfFile(ctx->data), 1);
  CloseHandle(ctx->file);

  free_ctx(ctx);
  return 1;
}

#endif
