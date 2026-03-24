#ifndef TOM_UTILS
#define TOM_UTILS

#include <stdint.h>
#include "defs.h"
#include "vfx.h"

typedef struct
{
    int32_t version;
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

void save_buffer_to_ppm(const uint8_t *buffer, int32_t width, int32_t height, const char *filename);
void save_shape_to_ppm_from_shape(const void *shape, const char *filename);

uint8_t *decode_shape_data(const void *shape);

void debug_shape_table(const void *shape_table);
void debug_draw_shape(const void *shape_table, int32_t shape_number, int32_t hotX, int32_t hotY);

void print_shape_table_header(const void *shape_table);
void print_shape_header(const void *shape);
void print_decoded_shape_data(const void *shape);

int32_t shape_table_get_shape_count(const void *shape_table);
int32_t shape_table_get_version(const void *shape_table);
uint64_t *shape_table_get_offsets(const void *shape_table);
uint32_t *shape_table_get_offsets_as_uint32(const void *shape_table);
void *shape_table_get_shape(const void *shape_table, int32_t index);

int16_t shape_get_bounds_x(const void *shape);
int16_t shape_get_bounds_y(const void *shape);
int16_t shape_get_origin_x(const void *shape);
int16_t shape_get_origin_y(const void *shape);
int32_t shape_get_xmin(const void *shape);
int32_t shape_get_ymin(const void *shape);
int32_t shape_get_xmax(const void *shape);
int32_t shape_get_ymax(const void *shape);
uint8_t *shape_get_data(const void *shape);

void update_palette(const PAL_HDR *PHDR, AESOP_Palette *palette);

void draw_shape_unclipped(void *buffer, void *shape, int hotX, int hotY, int CP_W);

#endif
