//
//  Runtime heap/resource class
//

#ifndef RTRES_H
#define RTRES_H

#define MAX_OBJ_TYPES 2450 // # of possible resource names
#define DIR_BLK 256        // # of cache directory entries/block
#define OD_SIZE 128        // # of entries/ordinal file directory block

#define DA_FIXED 0x00000001U    // Entry attribute equates
#define DA_MOVEABLE 0x00000000U // (only low word preserved in cache)
#define DA_PRECIOUS 0x00000020U
#define DA_DISCARDABLE 0x00000010U
#define DA_TEMPORARY 0x00000000U

#define DA_EVANESCENT 0x00000040U // Special runtime attribute equates
#define DA_DISCARDED 0x00000100U
#define DA_FREE 0x00000200U
#define DA_DEFAULT 0xFFFFFFFFU

#define DA_PLACEHOLDER 0x10000000L

#define SA_UNUSED 0x00000001 // Storage attribute flag equates
#define SA_DELETED 0x00000002

#define ROED 0 // Resource Ordinal Entry Directory
#define RDES 1 // Resource Description Directory
#define RDEP 2 // Resource Dependency Directory
#define CRFD 3 // Code Resource Function Directory

#define RTR_FREEBASE 0x0001U // TRUE for destructor to free heap memory

#define SIZE_DB (DIR_BLK * sizeof(HD_entry))

#define DOS_BUFFSIZE 32768U

typedef struct
{
   void *seg; // pointer to resource data

   uint32_t size;    // size of resource in bytes
   uint32_t flags;   // DA_ flags
   uint32_t history; // LRU counter value
   uint32_t locks;   // locking depth
   uint32_t user;    // .RES file offset or instance object name
} HD_entry;          // cached resource entry descriptor

typedef struct
{
   int8_t signature[16];
   uint32_t file_size;
   uint32_t lost_space;
   uint32_t FOB;
   uint32_t create_time;
   uint32_t modify_time;
} RF_file_hdr; // resource file header

typedef struct
{
   uint32_t timestamp;
   uint32_t data_attrib;
   uint32_t data_size;
} RF_entry_hdr; // resource file entry header

typedef struct OD_block
{
   uint32_t next;
   uint8_t flags[OD_SIZE];
   uint32_t index[OD_SIZE];
} OD_block;

typedef struct // name directory entry
{
   uint32_t OE; // public
   HRES thunk;  // public
   HRES handle; // public
} ND_entry;

typedef struct
{
   HD_entry *dir;     // public
   uint16_t nentries; // public

   int16_t file;
   RF_file_hdr RFH;
   RF_entry_hdr REH;
   OD_block OD;
   uint32_t cur_blk;

   uint16_t LRU_cnt;
   void *base;
   void *next_M;
   uint32_t free;
   void *last_F;

   HRES name_dir;
   int16_t nd_entries;
} RTR_class;

//
// RTR_addr macro returns current address of resource cache entry
// (See comments in RTRES.C)
//

#define RTR_addr(x) ((void *)(*(uint32_t *)(x)))

//
// RTR_member macro allows access to HD_entry structure members
//

#define RTR_member(x, y) ((HD_entry *)(x)->y)

//
// Extern *RTR allows public access to application's main RTR class
//

extern RTR_class *LNK;
extern RTR_class *RTR;

RTR_class *cdecl RTR_construct(void *base, uint32_t size, uint32_t nnames, int8_t *filename);
void cdecl RTR_destroy(RTR_class *RTR, uint32_t flags);

uint32_t cdecl RTR_force_discard(RTR_class *RTR, uint32_t goal);

HRES cdecl RTR_alloc(RTR_class *RTR, uint32_t bytes, uint32_t attrib);
void cdecl RTR_free(RTR_class *RTR, HRES entry);

void cdecl RTR_lock(RTR_class *RTR, HRES entry);
void cdecl RTR_unlock(HRES entry);

uint32_t cdecl RTR_size(HRES entry);

HRES cdecl RTR_get_resource_handle(RTR_class *RTR, uint32_t resource, uint32_t attrib);
void cdecl RTR_free_resource(RTR_class *RTR, uint32_t resource);

HRES cdecl RTR_load_resource(RTR_class *RTR, uint32_t resource, uint32_t attrib);
void cdecl RTR_read_resource(RTR_class *RTR, void *dest, uint32_t len);
uint32_t cdecl RTR_seek(RTR_class *RTR, uint32_t rnum);

#ifndef RTR_addr
void *cdecl RTR_addr(HRES entry);
#endif
void cdecl RTR_fixup(void **ptr, HRES entry);

ND_entry *cdecl RTR_search_name_dir(RTR_class *RTR, uint32_t resource);

int8_t *cdecl ASCII_name(uint32_t name);
void cdecl RTR_dump(RTR_class *RTR);

void cdecl RTR_HRES_chksum(int8_t *situation);
uint32_t cdecl RTR_chksum(HRES entry);

#endif
