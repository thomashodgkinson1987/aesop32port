#ifndef TOM_UTILS
#define TOM_UTILS

#include <stdint.h>
#include "defs.h"
#include "vfx.h"

typedef struct
{
    int8_t version_byte_1;
    int8_t version_byte_2;
    int8_t version_byte_3;
    int8_t version_byte_4;
    int32_t shape_count;
} SHAPETABLEHEADER;

typedef struct
{
    int32_t bounds; // Encoded bounding box info
    int32_t origin; // Encoded hotspot info
    int32_t xmin;   // Sprite bounding box min X
    int32_t ymin;   // Sprite bounding box min Y
    int32_t xmax;   // Sprite bounding box max X
    int32_t ymax;   // Sprite bounding box max Y
} SHAPEHEADER;

typedef struct
{
    uint16_t ncolors;
    RGB colors[256];
} AESOP_Palette;

char *ltoa(long val, char *buffer, int radix);
char *ultoa(unsigned long val, char *buffer, int radix);

void draw_shape_unclipped(void *buffer, void *shape, int hotX, int hotY, int CP_W);

void save_buffer_to_pgm(const uint8_t *buffer, int32_t width, int32_t height, const char *filename);
void save_shape_to_pgm_from_table_and_number(void *table, int32_t shape_number, const char *filename);
void save_shape_to_pgm_from_shape(void *shape, const char *filename);

void save_buffer_to_ppm(const uint8_t *buffer, int32_t width, int32_t height, const char *filename);
void save_shape_to_ppm_from_shape(void *shape, const char *filename);

void update_palette(const PAL_HDR *PHDR, AESOP_Palette *palette);

uint32_t *get_shape_offsets(void *shape_table);
uint8_t *decode_shape_data(void *shape);

void debug_shape_table(void *shape_table);
void print_shape_table_header(void *shape_table);
void print_shape_header(void *shape);

#endif
