//
//  AESOP common definitions for 16-bit DOS
//
//  (Shared by ARC and runtime sources)
//

#ifndef DEFS_H
#define DEFS_H

#include <stdint.h>

// Tom: added
#ifndef O_BINARY
#define O_BINARY 0
#endif

// Tom: added
#ifndef S_IREAD
#define S_IREAD S_IRUSR
#endif

// Tom: added
#ifndef S_IWRITE
#define S_IWRITE S_IWUSR
#endif

typedef uint32_t HRES; // run-time resource handle
typedef uint16_t HSTR; // run-time len-prefixed string descriptor

#define MSG_CREATE 0 // predefined message tokens (sent by system)
#define MSG_DESTROY 1
#define MSG_RESTORE 2

#define MAX_G 16 // Maximum depth of "family trees"

#pragma pack(push, 1) // Tom: added

typedef struct
{
   uint32_t thunk;
} IHDR; // Instance header

typedef struct
{
   uint16_t MV_list;
   uint16_t max_msg;
   uint16_t SD_list;
   uint16_t nprgs;
   uint16_t isize;
   uint16_t use_cnt;
} THDR; // Thunk header

typedef struct
{
   uint16_t static_size;
   uint32_t imports;
   uint32_t exports;
   uint32_t parent;
} PRG_HDR; // SOP program header

typedef struct
{
   uint16_t auto_size;
} MHDR; // Message handler header

typedef struct
{
   uint16_t msg;
   uint32_t handler;
   uint16_t SD_offset;
} MV_entry; // Thunk message vector list entry

typedef struct
{
   uint32_t SOP;
   uint32_t exports;
   uint16_t static_base;
   uint16_t extern_base;
} SD_entry; // Thunk SOP descriptor list entry

typedef struct
{
   uint16_t ncolors;
   uint16_t RGB;
   uint16_t fade[11];
} PAL_HDR; // Palette resource header

#pragma pack(pop) // Tom: added

#endif
