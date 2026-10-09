//
//  Run-time object management
//

#ifndef RTOBJECT_H
#define RTOBJECT_H

#include <stdint.h>

// On-disk save entries are 8 bytes (1-byte packing, as written by
// the original): without this, the compiler pads CDESC to 12 bytes
// and every save-file read past entry 0 lands misaligned — observed
// as garbage object names ("Program object 17629184 not found")
// when the xfer convert flow restores ITEMS_00.BIN.
#pragma pack(push, 1)

typedef struct
{
   uint16_t slot; // object list index
   uint32_t name; // code object name
   uint16_t size; // size of instance data (unused in text files)
} CDESC;          // static context descriptor

#pragma pack(pop)

_Static_assert(sizeof(CDESC) == 8, "CDESC must match the 8-byte save-file entry");

#define SF_TXT 1 // text savetype
#define SF_BIN 0 // binary savetype

extern uint8_t objflags[NUM_OBJECTS];
extern uint32_t objlist[NUM_OBJECTS];

extern int8_t lvlmap[LVL_X][LVL_Y];
extern int16_t lvlobj[3][LVL_X][LVL_Y];

void init_object_list(void);

void restore_range(int8_t *filename, uint32_t first, uint32_t last, uint32_t restoring);
int32_t save_range(int8_t *filename, int32_t filetype, int32_t first, int32_t last);
void translate_file(int8_t *TXT_filename, int8_t *BIN_filename, uint32_t first, uint32_t last);

int32_t create_object(int32_t argcnt, uint32_t name);
int32_t create_program(int32_t argcnt, int32_t index, uint32_t name);
int32_t destroy_object(int32_t argcnt, int32_t index);

#endif
