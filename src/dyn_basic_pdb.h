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

typedef unsigned int u32;
typedef unsigned __int64 u64;

struct DbpContext {
  void* image_addr;
  size_t image_size;
  char* output_pdb_name;
  DbpSourceFile** source_files;
  size_t source_files_len;
  size_t source_files_cap;

  // During write.
  HANDLE file;
  char* data;
  size_t file_size;
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

typedef struct SuperBlock {
  char FileMagic[0x1e];
  char padding[2];
  u32 BlockSize;
  u32 FreeBlockMapBlock;
  u32 NumBlocks;
  u32 NumDirectoryBytes;
  u32 Unknown;
  u32 BlockMapAddr;
} SuperBlock;

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
  memcpy(sb->FileMagic, BigHdrMagic, sizeof(BigHdrMagic));
  sb->padding[0] = '\0';
  sb->padding[1] = '\0';
  sb->BlockSize = BLOCK_SIZE;
  sb->FreeBlockMapBlock = 2;  // We never use map 1.
  sb->NumBlocks = 128;
  sb->NumDirectoryBytes = 4;
  sb->Unknown = 0;
  sb->BlockMapAddr = 3;

  // Mark all pages as free, then mark the first four in use:
  // 0 is super block, 1 is FPM1, 2 is FPM2, 3 is the block map (which points to
  // 4), 4 is the stream directory. We assume we don't have more than a single
  // one for now.
  memset(&ctx->data[BLOCK_SIZE], 0xff, BLOCK_SIZE * 2);
  for (u32 i = 0; i <= 4; ++i)
    mark_block_used(ctx, i);

  u32* block_map = get_block_ptr(ctx, 3);
  *block_map = 4;
}

typedef struct PdbStreamHeader {
  u32 Version;
  u32 Signature;
  u32 Age;
  UUID UniqueId;
} PdbStreamHeader;

static int write_pdb_info_stream(CTX, u32 page, u32* size) {
  char* block_ptr = get_block_ptr(ctx, page);
  PdbStreamHeader* psh = (PdbStreamHeader*)block_ptr;
  psh->Version = 20000404; /* VC70 */
  psh->Signature = (u32)time(NULL);
  psh->Age = 1;
  ENSURE(UuidCreate(&psh->UniqueId), RPC_S_OK);
  //printf("sizeof PdbStreamHeader: %zu\n", sizeof(PdbStreamHeader));

  block_ptr += sizeof(PdbStreamHeader);

  // Named Stream Map, currently empty.
  u32* hash = (u32*)block_ptr;
  *hash++ = 0;  // No string data.
  *hash++ = 0;  // Map size 
  *hash++ = 0;  // Map cap.

  *size = sizeof(PdbStreamHeader) + sizeof(u32) * 3;
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

  u32* dir = get_block_ptr(ctx, 4);
  *dir++ = 2;  // stream 0 and 1 to start with
  *dir++ = 0;  // Old MSF Directory, size 0
  u32* pdb_stream_size_fixup = dir++;  // PDB Info Stream, size written later
  *dir = alloc_block(ctx);
  ENSURE(1, write_pdb_info_stream(ctx, *dir++, pdb_stream_size_fixup));

  ENSURE(FlushViewOfFile(ctx->data, ctx->file_size), 1);
  ENSURE(UnmapViewOfFile(ctx->data), 1);
  CloseHandle(ctx->file);

  free_ctx(ctx);
  return 1;
}

#endif
