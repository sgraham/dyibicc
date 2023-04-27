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
#include "str6.h"
#include "str7.h"
#include "str8.h"
#include "str11.h"

typedef unsigned int u32;
typedef signed int i32;
typedef unsigned short u16;
typedef unsigned __int64 u64;

typedef struct StreamData StreamData;
typedef struct SuperBlock SuperBlock;

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
#define DEFAULT_NUM_BLOCKS 256
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

struct StreamData {
  u32 stream_index;

  u32 data_length;

  char* cur_block_ptr;
  char* cur_write;

  u32* blocks;
  size_t blocks_len;
  size_t blocks_cap;
};

static void stream_write_block(CTX, StreamData* stream, const void* data, size_t len) {
  if (!stream->cur_block_ptr) {
    u32 block_id = alloc_block(ctx);
    PUSH_BACK(stream->blocks, block_id);
    stream->cur_write = stream->cur_block_ptr = get_block_ptr(ctx, block_id);
  }

  memcpy(stream->cur_write, data, len);
  stream->cur_write += len;

  stream->data_length += len;

  // TODO: useful things
}

#define SW_BLOCK(x, len) stream_write_block(ctx, stream, x, len)
#define SW_U32(x) do { u32 _ = (x); stream_write_block(ctx, stream, &_, sizeof(_)); } while(0)
#define SW_I32(x) do { i32 _ = (x); stream_write_block(ctx, stream, &_, sizeof(_)); } while(0)
#define SW_U16(x) do { i16 _ = (x); stream_write_block(ctx, stream, &_, sizeof(_)); } while(0)

static void write_superblock(CTX) {
  SuperBlock* sb = (SuperBlock*)ctx->data;
  ctx->superblock = sb;
  memcpy(sb->FileMagic, BigHdrMagic, sizeof(BigHdrMagic));
  sb->padding[0] = '\0';
  sb->padding[1] = '\0';
  sb->BlockSize = BLOCK_SIZE;
  sb->FreeBlockMapBlock = 2;  // We never use map 1.
  sb->NumBlocks = DEFAULT_NUM_BLOCKS;
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
  stream->stream_index = ctx->stream_data_len;
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

static int write_pdb_info_stream(CTX, StreamData* stream, u32 names_stream) {
  PdbStreamHeader psh = {
    .Version = 20000404, /* VC70 */
    .Signature = (u32)time(NULL),
    .Age = 1,
  };
  ENSURE(UuidCreate(&psh.UniqueId), RPC_S_OK);
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
  SW_U32(1); // Size
  SW_U32(4); // Capacity
  // Then two bit vectors, first for "present":
  SW_U32(0x01);  // Present length (1 word follows)
  SW_U32(0x02);  // 0b0000`0010    (second bucket occupied)
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
  // This is an "empty" TPI/IPI stream, we do not emit any user-defined types
  // currently.
  TpiStreamHeader tsh = {
      .Version = 20040203, /* V80 */
      .HeaderSize = sizeof(TpiStreamHeader),
      .TypeIndexBegin = 0x1000,
      .TypeIndexEnd = 0x1000,
      .TypeRecordBytes = 0,
      .HashStreamIndex = -1,
      .HashAuxStreamIndex = -1,
      .HashKeySize = 4,
      .NumHashBuckets = 0x3ffff,
      .HashValueBufferOffset = 0,
      .HashValueBufferLength = 0,
      .IndexOffsetBufferOffset = 0,
      .IndexOffsetBufferLength = 0,
      .HashAdjBufferOffset = 0,
      .HashAdjBufferLength = 0,
  };
  SW_BLOCK(&tsh, sizeof(tsh));
  return 1;
}


// Copied from:
// https://github.com/microsoft/microsoft-pdb/blob/082c5290e5aff028ae84e43affa8be717aa7af73/PDB/include/misc.h#L15
// with minor type adaptations; I think it probably has to match to put things
// in the right slots when loaded.
static unsigned long calc_hash(char* pb, size_t cb, unsigned long mod) {
  unsigned long ulHash = 0;

  // hash leading dwords using Duff's Device
  size_t cl = cb >> 2;
  unsigned long* pul = (unsigned long*)pb;
  unsigned long* pulMac = pul + cl;
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
    ulHash ^= *(pb++);
  }

  const unsigned long toLowerMask = 0x20202020;
  ulHash |= toLowerMask;
  ulHash ^= (ulHash >> 11);

  return (ulHash ^ (ulHash >> 16)) % (mod);
}

typedef struct NmtAlikeHashTable {
  u32 *hashni;
  size_t hashni_len;
  size_t hashni_cap;
} NmtAlikeHashTable;

static int nmt_add_string(NmtAlikeHashTable* nmt, char* str, u32* name_index) {
}

static int write_names_stream(CTX, StreamData* stream) {
  SW_U32(0xeffeeffe);  // Header
  SW_U32(1);           // verLongHash
  SW_U32(33);          // Size of string buffer

  // String buffer
  static char names[] = "\0\0c:\\src\\dyibicc\\scratch\\pdb\\x.c";
  stream_write_block(ctx, stream, names, sizeof(names));

  SW_U32(4);  // 4 elements in array

  // TODO: This hash seems right for these two items; need to figure out what
  // actually goes in this stream, how the table grows, etc.
  u32 hash1 = calc_hash(&names[1], 1, /*mod=*/4);
  u32 hash2 = calc_hash(&names[2], 31, /*mod=*/4);
  SW_U32(1);  // offset 1 ""
  SW_U32(2);  // offset 2 "c:\...\x.c"
  SW_U32(0);
  SW_U32(0);
  SW_U32(2);  // 2 elements filled

  return 1;
}

typedef struct DbiStreamHeader {
  i32 VersionSignature;
  u32 VersionHeader;
  u32 Age;
  u16 GlobalStreamIndex;
  u16 BuildNumber;
  u16 PublicStreamIndex;
  u16 PdbDllVersion;
  u16 SymRecordStream;
  u16 PdbDllRbld;
  i32 ModInfoSize;
  i32 SectionContributionSize;
  i32 SectionMapSize;
  i32 SourceInfoSize;
  i32 TypeServerMapSize;
  u32 MFCTypeServerIndex;
  i32 OptionalDbgHeaderSize;
  i32 ECSubstreamSize;
  u16 Flags;
  u16 Machine;
  u32 Padding;
} DbiStreamHeader;

// Part of ModInfo
typedef struct SectionContribEntry {
    u16 Section;
    char Padding1[2];
    i32 Offset;
    i32 Size;
    u32 Characteristics;
    u16 ModuleIndex;
    char Padding2[2];
    u32 DataCrc;
    u32 RelocCrc;
} SectionContribEntry;

typedef struct ModInfo {
  u32 Unused1;
  SectionContribEntry SectionContr;
  u16 Flags;
  u16 ModuleSymStream;
  u32 SymByteSize;
  u32 C11ByteSize;
  u32 C13ByteSize;
  u16 SourceFileCount;
  char Padding[2];
  u32 Unused2;
  u32 SourceFileNameIndex;
  u32 PdbFilePathNameIndex;
  // char ModuleName[];
  // char ObjFileName[];
} ModInfo;

typedef struct SectionMapHeader {
  u16 Count;     // Number of segment descriptors
  u16 LogCount;  // Number of logical segment descriptors
} SectionMapHeader;

typedef struct SectionMapEntry {
  u16 Flags;  // See the SectionMapEntryFlags enum below.
  u16 Ovl;    // Logical overlay number
  u16 Group;  // Group index into descriptor array.
  u16 Frame;
  u16 SectionName;    // Byte index of segment / group name in string table, or 0xFFFF.
  u16 ClassName;      // Byte index of class in string table, or 0xFFFF.
  u32 Offset;         // Byte offset of the logical segment within physical segment. If group is set
                      // in flags, this is the offset of the group.
  u32 SectionLength;  // Byte count of the segment or group.
} SectionMapEntry;

enum SectionMapEntryFlags {
  SMEF_Read = 1 << 0,              // Segment is readable.
  SMEF_Write = 1 << 1,             // Segment is writable.
  SMEF_Execute = 1 << 2,           // Segment is executable.
  SMEF_AddressIs32Bit = 1 << 3,    // Descriptor describes a 32-bit linear address.
  SMEF_IsSelector = 1 << 8,        // Frame represents a selector.
  SMEF_IsAbsoluteAddress = 1 << 9, // Frame represents an absolute address.
  SMEF_IsGroup = 1 << 10           // If set, descriptor represents a group.
};

typedef struct FileInfoSubstreamHeader {
  u16 NumModules;
  u16 NumSourceFiles;

  //u16 ModIndices[NumModules];
  //u16 ModFileCounts[NumModules];
  //u32 FileNameOffsets[NumSourceFiles];
  //char NamesBuffer[][NumSourceFiles];
} FileInfoSubstreamHeader;

typedef struct DbiWriteData {
  u32 global_symbol_stream;
  u32 public_stream;
  u32 sym_record_stream;
  u32 section_header_stream;
  u32 module_sym_stream;
  u32 module_symbols_byte_size;
  u32 module_c13_byte_size;
  u32 num_source_files;
} DbiWriteData;

static int write_dbi_stream(CTX,
                            StreamData* stream,
                            DbiWriteData* dwd) {
  u32 block_id = alloc_block(ctx);
  PUSH_BACK(stream->blocks, block_id);

  char* start = get_block_ptr(ctx, block_id);
  char* cur = start;
#if 1
  DbiStreamHeader* dsh = (DbiStreamHeader*)cur;
  dsh->VersionSignature = -1;
  dsh->VersionHeader = 19990903; /* V70 */
  dsh->Age = 1;
  dsh->GlobalStreamIndex = dwd->global_symbol_stream;
  dsh->BuildNumber = 0x8eb; // TODO This is what llvm emits.
  dsh->PublicStreamIndex = dwd->public_stream;
  dsh->PdbDllVersion = 0;
  dsh->SymRecordStream = dwd->sym_record_stream;
  dsh->PdbDllRbld = 0;

  // All of these are filled in after being written later.
  dsh->ModInfoSize = 0;
  dsh->SectionContributionSize = 0;
  dsh->SectionMapSize = 0;
  dsh->SourceInfoSize = 0;
  dsh->OptionalDbgHeaderSize = 0;
  dsh->ECSubstreamSize = 0;

  dsh->TypeServerMapSize = 0;      // empty
  dsh->MFCTypeServerIndex = 0;     // empty
  dsh->Flags = 0;
  dsh->Machine = 0x8664;
  dsh->Padding = 0;

  cur = (char*)(dsh + 1);

#if 0
  unsigned char mod_info[] = {
  // ModInfo
  0x00, 0x00, 0x00, 0x00, // Unused1
  // SectionContribEntry
  0x01, 0x00, // Section
  0x00, 0x00, // Padding1
  0x00, 0x00, 0x00, 0x00, // Offset
  0x22, 0x00, 0x00, 0x00, // Size
  0x20, 0x00, 0x50, 0x60, // Characteristics
  0x00, 0x00, // ModuleIndex
  0x00, 0x00, // Padding2
  0x24, 0x58, 0xd2, 0x68, // DataCrc
  0x00, 0x00, 0x00, 0x00, // RelocCrc
  // end of SectionContribEntry
  0x00, 0x00,  // Flags
  0x0b, 0x00, // ModuleSymStream
  0x48, 0x01, 0x00, 0x00, // SymByteSize
  0x00, 0x00, 0x00, 0x00, // C11ByteSize
  0x80, 0x00, 0x00, 0x00, // C13ByteSize
  0x01, 0x00, // SourceFileCount
  0x00, 0x00, // Padding
  0x00, 0x00, 0x00, 0x00, // Unused2
  0x00, 0x00, 0x00, 0x00, // SourceFileNameIndex
  0x00, 0x00, 0x00, 0x00, // PdbFilePathNameIndex
  0x43, 0x3a, 0x5c, 0x55, 0x73, 0x65, 0x72, 0x73, 0x5c, 0x73, 0x67, 0x72,
  0x61, 0x68, 0x61, 0x6d, 0x5c, 0x41, 0x70, 0x70, 0x44, 0x61, 0x74, 0x61,
  0x5c, 0x4c, 0x6f, 0x63, 0x61, 0x6c, 0x5c, 0x54, 0x65, 0x6d, 0x70, 0x5c,
  0x78, 0x2d, 0x36, 0x61, 0x64, 0x32, 0x31, 0x35, 0x2e, 0x6f, 0x62, 0x6a,
  0x00, // ModuleName
  0x43, 0x3a, 0x5c, 0x55, 0x73, 0x65, 0x72, 0x73, 0x5c, 0x73, 0x67, 0x72,
  0x61, 0x68, 0x61, 0x6d, 0x5c, 0x41, 0x70, 0x70, 0x44, 0x61, 0x74, 0x61,
  0x5c, 0x4c, 0x6f, 0x63, 0x61, 0x6c, 0x5c, 0x54, 0x65, 0x6d, 0x70, 0x5c,
  0x78, 0x2d, 0x36, 0x61, 0x64, 0x32, 0x31, 0x35, 0x2e, 0x6f, 0x62, 0x6a,
  0x00, // ObjFileName

  // 162 ModInfo bytes to here
  0x00, 0x00, // Probably 4 align?

  // TBD Should be another ModInfo, but doesn't make sense; Unused1 is set?
  0x01, 0x00,
  0x00, 0x00,
  0xff, 0xff,
  0x00, 0x00, 0x00, 0x00,
  0x00, 0x00,
  0xff, 0xff, 0xff, 0xff,
  0x00, 0x00,
  0x00, 0x00,
  0xff, 0xff,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x0c, 0x00,
  0xe0, 0x01, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00,
  0x01, 0x00, 0x00, 0x00,
  0x2a, 0x20, 0x4c, 0x69, 0x6e, 0x6b, 0x65, 0x72, 0x20, 0x2a, 0x00, 0x00,
  // "* Linker *\0\0" is about all that makes sense here.
  };
  memcpy(cur, mod_info, sizeof(mod_info));
  dsh->ModInfoSize = sizeof(mod_info);
#else
  // Module Info Substream. We output a single module with a single section for
  // the whole jit blob.
  ModInfo* mod = (ModInfo*)cur;
  mod->Unused1 = 0;

  SectionContribEntry* sce = &mod->SectionContr;
  sce->Section = 1;
  sce->Padding1[0] = 0;
  sce->Padding1[1] = 0;
  sce->Offset = 0;
  sce->Size = ctx->image_size;
  sce->Characteristics =
      IMAGE_SCN_CNT_CODE | IMAGE_SCN_ALIGN_16BYTES | IMAGE_SCN_MEM_EXECUTE | IMAGE_SCN_MEM_READ;
  sce->ModuleIndex = 0;
  sce->Padding2[0] = 0;
  sce->Padding2[1] = 0;
  sce->DataCrc = 0;
  sce->RelocCrc = 0;

  mod->Flags = 0;
  mod->ModuleSymStream = dwd->module_sym_stream;
  mod->SymByteSize = dwd->module_symbols_byte_size;
  mod->C11ByteSize = 0;
  mod->C13ByteSize = dwd->module_c13_byte_size;
  mod->SourceFileCount = dwd->num_source_files;
  mod->Padding[0] = 0;
  mod->Padding[1] = 0;
  mod->Unused2 = 0;
  mod->SourceFileNameIndex = 0;
  mod->PdbFilePathNameIndex = 0;

  char* names = (char*)(mod + 1);
  static const char obj_name[] = "dyn_basic_pdb-synthetic-for-jit.obj";
  memcpy(names, obj_name, sizeof(obj_name));
  names += sizeof(obj_name);
  memcpy(names, obj_name, sizeof(obj_name));
  names += sizeof(obj_name);

  dsh->ModInfoSize = align_to(names - cur, 4);
#endif
  cur += dsh->ModInfoSize;

#if 1
  unsigned char seccontrib[] = {
  // Section Contribution Substream
  0x2d, 0xba, 0x2e, 0xf1,  // Ver60

  // Expecting 5 SectionContribEntry based on SectionContributionSize
  // TBD: Why are there some here and some inside ModInfo?

  // SectionContribEntry0
  0x01, 0x00,  // Section
  0x00, 0x00,  // Padding1
  0x00, 0x00, 0x00, 0x00, // Offset
  0x22, 0x00, 0x00, 0x00, // Size
  0x20, 0x00, 0x50, 0x60, // Characteristics
  0x00, 0x00, // ModuleIndex
  0x00, 0x00, // Padding2
  0x24, 0x58, 0xd2, 0x68, // DataCrc
  0x00, 0x00, 0x00, 0x00, // RelocCrc

  // SectionContribEntry1
  0x02, 0x00,
  0x00, 0x00,
  0x00, 0x00, 0x00, 0x00,
  0x1c, 0x00, 0x00, 0x00,  // Size
  0x40, 0x00, 0x00, 0x40,
  0x01, 0x00,
  0x00, 0x00,
  0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00,

  // SectionContribEntry2
  0x02, 0x00,
  0x00, 0x00,
  0x1c, 0x00, 0x00, 0x00,
  0x39, 0x00, 0x00, 0x00,  // Size
  0x40, 0x00, 0x00, 0x40,
  0x01, 0x00,
  0x00, 0x00,
  0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00,

  // SectionContribEntry3
  0x02, 0x00,
  0x00, 0x00,
  0x58, 0x00, 0x00, 0x00,
  0x08, 0x00, 0x00, 0x00,  // Size
  0x40, 0x00, 0x30, 0x40,
  0x00, 0x00,
  0x00, 0x00,
  0x84, 0x6b, 0xb9, 0x1a,
  0x00, 0x00, 0x00, 0x00,

  // SectionContribEntry4
  0x03, 0x00,
  0x00, 0x00,
  0x00, 0x00, 0x00, 0x00,
  0x0c, 0x00, 0x00, 0x00,  // Size
  0x40, 0x00, 0x30, 0x40,
  0x00, 0x00,
  0x00, 0x00,
  0xd7, 0x88, 0x4b, 0xb7,
  0x00, 0x00, 0x00, 0x00,
  };
  memcpy(cur, seccontrib, sizeof(seccontrib));
  dsh->SectionContributionSize = sizeof(seccontrib);
#else
  // Section Contribution Substream
  u32* section_contrib = (u32*)cur;
  *section_contrib = 0xeffe0000 + 19970605; /* Ver60 */
  dsh->SectionContributionSize = 4;
  //SectionContribEntry* sce2 = (SectionContribEntry*)(section_contrib + 1);
  //memcpy(sce2, sce, sizeof(SectionContribEntry));
#endif
  cur += dsh->SectionContributionSize;


#if 1
  // Section Map Substream
  unsigned char smss[] = {
  0x04, 0x00,  // Count
  0x04, 0x00,  // LogCount

  0x0d, 0x01, 0x00, 0x00,
  0x00, 0x00, 0x01, 0x00,
  0xff, 0xff, 0xff, 0xff,
  0x00, 0x00, 0x00, 0x00,
  0x22, 0x00, 0x00, 0x00,

  0x09, 0x01, 0x00, 0x00,
  0x00, 0x00, 0x02, 0x00,
  0xff, 0xff, 0xff, 0xff,
  0x00, 0x00, 0x00, 0x00,
  0x60, 0x00, 0x00, 0x00,

  0x09, 0x01, 0x00, 0x00,
  0x00, 0x00, 0x03, 0x00,
  0xff, 0xff, 0xff, 0xff,
  0x00, 0x00, 0x00, 0x00,
  0x0c, 0x00, 0x00, 0x00,

  0x08, 0x02, 0x00, 0x00,
  0x00, 0x00, 0x04, 0x00,
  0xff, 0xff, 0xff, 0xff,
  0x00, 0x00, 0x00, 0x00,
  0xff, 0xff, 0xff, 0xff,
  };
  memcpy(cur, smss, sizeof(smss));
  dsh->SectionMapSize = sizeof(smss);
#else
  SectionMapHeader* smh = (SectionMapHeader*)cur;
  smh->Count = 1;
  smh->LogCount = 1;
  SectionMapEntry* sme = (SectionMapEntry*)(smh + 1);
  sme->Flags = SMEF_Read | SMEF_Write | SMEF_AddressIs32Bit | SMEF_IsSelector;
  sme->Ovl = 0;
  sme->Group = 0;
  sme->Frame = 1;
  sme->SectionName = 0xffff;
  sme->ClassName = 0xffff;
  sme->Offset = 0;
  sme->SectionLength = 0x22;
  dsh->SectionMapSize = sizeof(SectionMapHeader) + sizeof(SectionMapEntry);
#endif

  cur += dsh->SectionMapSize;

#if 0
  unsigned char file_info_ss[] = {
  // File Info Substream TBD
  0x02, 0x00,  // NumModules
  0x01, 0x00,  // NumSourceFiles
  0x00, 0x00,  // ModIndices[0]
  0x01, 0x00,  // ModIndices[1]
  0x01, 0x00,  // ModFileCounts[0]
  0x00, 0x00,  // ModFileCounts[1]
  0x00, 0x00, 0x00, 0x00, // FileNameOffsets[0]

  0x63, 0x3a, 0x5c, 0x73, 0x72, 0x63, 0x5c, 0x64, 0x79, 0x69, 0x62, 0x69,
  0x63, 0x63, 0x5c, 0x73, 0x63, 0x72, 0x61, 0x74, 0x63, 0x68, 0x5c, 0x70,
  0x64, 0x62, 0x5c, 0x78, 0x2e, 0x63, 0x00, // NamesBuffer[0][0]

  0x00,  // align to 4?
  };
  memcpy(cur, file_info_ss, sizeof(file_info_ss));
  dsh->SourceInfoSize = sizeof(file_info_ss);
#else
  // File Info Substream
  FileInfoSubstreamHeader* fish = (FileInfoSubstreamHeader*)cur;
  fish->NumModules = 1;
  fish->NumSourceFiles = 1;
  u16* file_info = (u16*)(fish + 1);
  *file_info++ = 0; // ModIndices[0]
  *file_info++ = 1; // ModFileCounts[0]
  u32* file_name_offsets = (u32*)file_info;
  *file_name_offsets++ = 0;
  char* filename_buf = (char*)file_name_offsets;
  static const char source_name[] = "c:\\path\\source.c";
  memcpy(filename_buf, source_name, sizeof(source_name));
  filename_buf += sizeof(source_name);
  dsh->SourceInfoSize = align_to(filename_buf - (char*)fish, 4);
#endif

  cur += dsh->SourceInfoSize;

  // No TypeServerMap, MFCTypeServerMap

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
  memcpy(cur, empty_nmt, sizeof(empty_nmt));
  dsh->ECSubstreamSize = sizeof(empty_nmt);
  cur += dsh->ECSubstreamSize;


  // TODO, use SW_
  // Index 5 points to the section header stream, which is theoretically
  // optional, but llvm-pdbutil doesn't like it if it's not there, so I'm
  // guessing that various microsoft things don't either. The stream it points
  // at is empty, but that seems to be sufficient.
  unsigned char dbg_hdr[] = {
    // OptionalDbgHeader Stream
    0xff, 0xff,
    0xff, 0xff,
    0xff, 0xff,
    0xff, 0xff,
    0xff, 0xff,
    (unsigned char)dwd->section_header_stream, 0,  // Section Header
    0xff, 0xff,
    0xff, 0xff,
    0xff, 0xff,
    0xff, 0xff,
    0xff, 0xff
  };
  memcpy(cur, dbg_hdr, sizeof(dbg_hdr));
  dsh->OptionalDbgHeaderSize = sizeof(dbg_hdr);
  cur += dsh->OptionalDbgHeaderSize;

  stream->data_length = cur - start;
  return 1;
#else
  memcpy(start, str3_raw, str3_raw_len);
  stream->data_length = str3_raw_len;
  return 1;
#endif
}

static int write_directory(CTX) {
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

  // This can't easily use StreamData because it's the directory of streams. It
  // would take a larger pdb that we expect to be writing here to overflow the
  // first block (especially since we don't write types), so just assert that we
  // didn't grow too large for now.
  if (ctx->superblock->NumDirectoryBytes > BLOCK_SIZE) {
    fprintf(stderr, "%s:%d: directory grew beyond BLOCK_SIZE\n", __FILE__, __LINE__);
    return 0;
  }

  return 1;
}

static int create_file_map(CTX) {
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

  // Stream 3: DBI Stream.
  StreamData* stream3 = add_stream(ctx);

  // Stream 4: IPI Stream.
  StreamData* stream4 = add_stream(ctx);

  // Stream 5: "/LinkInfo", empty.
  //add_stream(ctx);

  // HACK
  StreamData* global_symbol_hash = add_stream(ctx);
  stream_write_block(ctx, global_symbol_hash, str6_raw, str6_raw_len);

  StreamData* public_symbol_hash = add_stream(ctx);
  stream_write_block(ctx, public_symbol_hash, str7_raw, str7_raw_len);

  // HACK
  StreamData* sym_records = add_stream(ctx);
  stream_write_block(ctx, sym_records, str8_raw, str8_raw_len);

  // Section Headers; empty. Referred to by DBI in 'optional' dbg headers, and
  // llvm-pdbutil wants it to exist, but handles an empty stream reasonably.
  StreamData* section_headers = add_stream(ctx);

  // Module blah.obj
  // HACK
  StreamData* module_stream = add_stream(ctx);
  stream_write_block(ctx, module_stream, str11_raw, str11_raw_len);

  // "/names": named, so stream index doesn't matter.
  StreamData* names_stream = add_stream(ctx);

  ENSURE(1, write_empty_tpi_ipi_stream(ctx, stream2));
  DbiWriteData dwd = {
    .global_symbol_stream = global_symbol_hash->stream_index,
    .public_stream = public_symbol_hash->stream_index,
    .sym_record_stream = sym_records->stream_index,
    .section_header_stream = section_headers->stream_index,
    .module_sym_stream = module_stream->stream_index,
    .module_symbols_byte_size = 0x148,
    .module_c13_byte_size = 0x80,
    .num_source_files = 1,
  };
  ENSURE(write_dbi_stream(ctx, stream3, &dwd), 1);
  ENSURE(write_empty_tpi_ipi_stream(ctx, stream4), 1);
  ENSURE(write_names_stream(ctx, names_stream), 1);
  ENSURE(write_pdb_info_stream(ctx, stream1, names_stream->stream_index), 1);

  ENSURE(write_directory(ctx), 1);

  ENSURE(FlushViewOfFile(ctx->data, ctx->file_size), 1);
  ENSURE(UnmapViewOfFile(ctx->data), 1);
  CloseHandle(ctx->file);

  free_ctx(ctx);
  return 1;
}

#endif
