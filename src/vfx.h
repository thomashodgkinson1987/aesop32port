// ############################################################################
// ##                                                                        ##
// ##  VFX.H: C type definitions & API function prototypes                   ##
// ##                                                                        ##
// ##  Source compatible with 32-bit 80386 C/C++                             ##
// ##                                                                        ##
// ##  V0.10 of 10-Dec-92: Initial version                                   ##
// ##  V1.01 of 12-May-93: Added VFX_shape_lookaside(), new polygon calls    ##
// ##                      PCX_draw() returns void                           ##
// ##  V1.02 of 18-Jun-93: Added rotation/scaling calls                      ##
// ##                      intervals parameter added to window_fade()        ##
// ##                      Added RECT structure                              ##
// ##  V1.03 of 28-Jul-93: VERTEX2D structure changed to SCRNVERTEX          ##
// ##                      Fixed-point data types renamed                    ##
// ##                      Added VFX_bank_reset() call                       ##
// ##  V1.04 of  4-Sep-93: Indirect function prototypes changed for C++ use  ##
// ##  V1.05 of 26-Sep-93: Added FLOAT typedef                               ##
// ##                      RGB BYTE members changed to UBYTEs                ##
// ##  V1.06 of 13-Oct-93: Added VFX_pane_refresh(), other new calls         ##
// ##  V1.07 of 17-Nov-93: Added MetaWare High C support                     ##
// ##  V1.10 of  3-Dec-93: Modified VFX_pane_refresh(), WINDOW structure     ##
// ##  V1.15 of 13-Mar-94: Added new VFX.C function prototypes               ##
// ##                      Added new WINDOW members, PANE_LIST structure     ##
// ##                                                                        ##
// ##  Project: 386FX Sound & Light(TM)                                      ##
// ##   Author: Ken Arnold, John Miles, John Lemberger                       ##
// ##                                                                        ##
// ############################################################################
// ##                                                                        ##
// ##  Copyright (C) 1992-1994 Non-Linear Arts, Inc.                         ##
// ##                                                                        ##
// ##  Non-Linear Arts, Inc.                                                 ##
// ##  3415 Greystone #200                                                   ##
// ##  Austin, TX 78731                                                      ##
// ##                                                                        ##
// ##  (512) 346-9595 / FAX (512) 346-9596 / BBS (512) 454-9990              ##
// ##                                                                        ##
// ############################################################################

#ifndef VFX_H
#define VFX_H

#include <stdint.h>

// #ifndef TRUE
// #define TRUE -1
// #endif

// #ifndef FALSE
// #define FALSE 0
// #endif

#define SHAPE_FILE_VERSION '01.1' // 1.10 backwards for big-endian compare

typedef int32_t FIXED30; // 2:30 fixed-point type [-1.999,+1.999]

#define GIF_SCRATCH_SIZE 20526L // Temp memory req'd for GIF decompression

//
// VFX_map_polygon() flags
//

#define MP_XLAT 0x0001 // Use lookaside table (speed loss = ~9%)
#define MP_XP 0x0002   // Enable transparency (speed loss = ~6%)

//
// VFX_shape_transform() flags
//

#define ST_XLAT 0x0001  // Use shape_lookaside() table
#define ST_REUSE 0x0002 // Use buffer contents from prior call

//
// VFX_line_draw() modes
//

#define LD_DRAW 0
#define LD_TRANSLATE 1
#define LD_EXECUTE 2

//
// VFX_pane_scroll() modes
//

#define PS_NOWRAP 0
#define PS_WRAP 1

#define NO_COLOR -1

//
// VFX_shape_visible_rectangle() mirror values
//

#define VR_NO_MIRROR 0
#define VR_X_MIRROR 1
#define VR_Y_MIRROR 2
#define VR_XY_MIRROR 3

//
// PANE_LIST.flags values
//

#define PL_FREE 0      // Free and available for assignment
#define PL_VALID 1     // Assigned; to be refreshed
#define PL_CONTAINED 2 // Contained within another pane; don't refresh

//
// VFX data structures
//

typedef uint8_t STENCIL;

typedef struct _window
{
   uint8_t *buffer;
   int32_t x_max;
   int32_t y_max;

   STENCIL *stencil;
   uint8_t *shadow;
} WINDOW;

typedef struct _pane
{
   WINDOW *window;
   int32_t x0;
   int32_t y0;
   int32_t x1;
   int32_t y1;
} PANE;

typedef struct _pane_list
{
   PANE *array;
   uint32_t *flags;
   int32_t size;
} PANE_LIST;

typedef union
{
   void *v;
   uint8_t *b;
   uint16_t *w;
   uint32_t *d;
} FLEX_PTR;

typedef struct
{
   int32_t scrn_width;
   int32_t scrn_height;
   int32_t bytes_per_pixel;
   int32_t ncolors;
   int32_t npages;
   uint32_t flags;
} VFX_DESC;

typedef struct
{
   uint8_t r;
   uint8_t g;
   uint8_t b;
} RGB;

typedef struct
{
   uint8_t color;
   RGB rgb;
} CRGB;

typedef struct
{
   int32_t x;
   int32_t y;
} POINT;

typedef struct
{
   int32_t version;
   int32_t char_count;
   int32_t char_height;
   int32_t font_background;
} FONT;

typedef struct // Vertex structure used by polygon primitives
{
   int32_t x; // Screen X
   int32_t y; // Screen Y

   int16_t c; // Color/addition value used by some primitives

   int16_t u; // Texture source X
   int16_t v; // Texture source Y
   FIXED30 w; // Homogeneous perspective divisor (unused by VFX3D)
} SCRNVERTEX;

typedef struct
{
   int32_t x0;
   int32_t y0;
   int32_t x1;
   int32_t y1;
} RECT;

/* Tom: commented out
#define INT_TO_FIXED16(x) (((long)(int)(x)) << 16)
#define DOUBLE_TO_FIXED16(x) ((long)((x) * 65536.0 + 0.5))
#define FIXED16_TO_DOUBLE(x) (((double)(x)) / 65536.0)
#define FIXED16_TO_INT(x) ((int)((x) < 0 ? -(-(x) >> 16) : (x) >> 16))
#define ROUND_FIXED16_TO_INT(x) ((int)((x) < 0 ? -((32768 - (x)) >> 16) : ((x) + 32768) >> 16))

#define FIXED16_TO_FIXED30(x) ((x) << 14)
#define FIXED30_TO_FIXED16(x) ((x) >> 14)
#define FIXED30_TO_DOUBLE(x) (((double)x) / 1073741824.0)
#define DOUBLE_TO_FIXED30(x) ((long)(x * 1073741824.0 + 0.5))

#define PIXELS_IN_PANE(pane) (((pane).x1 - (pane).x0 + 1) * ((pane).y1 - (pane).y0 + 1))
#define PIXELS_IN_PANEP(pane) (((pane)->x1 - (pane)->x0 + 1) * ((pane)->y1 - (pane)0 > y0 + 1))
*/

//
// Hardware-specific VFX DLL functions
//

extern VFX_DESC *(*VFX_describe_driver)(void);
extern void (*VFX_init_driver)(void);
extern void (*VFX_shutdown_driver)(void);
extern void (*VFX_wait_vblank)(void);
extern void (*VFX_wait_vblank_leading)(void);
extern void (*VFX_area_wipe)(int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t color);
extern void (*VFX_window_refresh)(WINDOW *target, int32_t x0, int32_t y0, int32_t x1, int32_t y1);
extern void (*VFX_window_read)(WINDOW *destination, int32_t x0, int32_t y0, int32_t x1, int32_t y1);
extern void (*VFX_pane_refresh)(PANE *target, int32_t x0, int32_t y0, int32_t x1, int32_t y1);
extern void (*VFX_DAC_read)(int32_t color_number, RGB *triplet);
extern void (*VFX_DAC_write)(int32_t color_number, RGB *triplet);
extern void (*VFX_bank_reset)(void);
extern void (*VFX_line_address)(int32_t x, int32_t y, uint8_t **addr, uint32_t *nbytes);

//
// Device-independent VFX API functions (VFXC.C)
//

extern uint32_t VFX_stencil_size(WINDOW *source, uint32_t transparent_color);

extern STENCIL *VFX_stencil_construct(WINDOW *source, void *dest, uint32_t transparent_color);
extern void VFX_stencil_destroy(STENCIL *stencil);

extern WINDOW *VFX_window_construct(int32_t width, int32_t height);
extern void VFX_window_destroy(WINDOW *window);

extern PANE *VFX_pane_construct(WINDOW *window, int32_t x0, int32_t y0, int32_t x1, int32_t y1);
extern void VFX_pane_destroy(PANE *pane);

extern PANE_LIST *VFX_pane_list_construct(int32_t n_entries);
extern void VFX_pane_list_destroy(PANE_LIST *list);

extern void VFX_pane_list_clear(PANE_LIST *list);

extern int32_t VFX_pane_list_add(PANE_LIST *list, PANE *target);
extern int32_t VFX_pane_list_add_area(PANE_LIST *list, WINDOW *window, int32_t x0, int32_t y0, int32_t x1, int32_t y1);

extern void VFX_pane_list_delete_entry(PANE_LIST *list, int32_t entry_num);

extern void VFX_pane_list_refresh(PANE_LIST *list);

//
// Device-independent VFX API functions (VFXA.ASM)
//

extern int8_t *VFX_driver_name(void *VFXScanDLL);

extern int32_t VFX_register_driver(void *DLLbase);

extern int32_t VFX_line_draw(PANE *pane, int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t mode, int32_t parm);

extern void VFX_shape_draw(PANE *pane, void *shape_table, int32_t shape_number, int32_t hotX, int32_t hotY);

extern void VFX_shape_lookaside(uint8_t *table);
extern void VFX_shape_translate_draw(PANE *pane, void *shape_table, int32_t shape_number, int32_t hotX, int32_t hotY);

extern void VFX_shape_remap_colors(void *shape_table, uint32_t shape_number);

void VFX_shape_visible_rectangle(void *shape_table, int32_t shape_number, int32_t hotX, int32_t hotY, int32_t mirror, int32_t *rectangle);

extern int32_t VFX_shape_scan(PANE *pane, uint32_t transparentColor, int32_t hotX, int32_t hotY, void *buffer);
extern int32_t VFX_shape_bounds(void *shape_table, int32_t shape_num);
extern int32_t VFX_shape_origin(void *shape_table, int32_t shape_num);
extern int32_t VFX_shape_resolution(void *shape_table, int32_t shape_num);
extern int32_t VFX_shape_minxy(void *shape_table, int32_t shape_num);
extern void VFX_shape_palette(void *shape_table, int32_t shape_num, RGB *palette);
extern int32_t VFX_shape_colors(void *shape_table, int32_t shape_num, CRGB *colors);
extern int32_t VFX_shape_set_colors(void *shape_table, int32_t shape_number, CRGB *colors);
extern int32_t VFX_shape_count(void *shape_table);
extern int32_t VFX_shape_list(void *shape_table, uint32_t *index_list);
extern int32_t VFX_shape_palette_list(void *shape_table, uint32_t *index_list);

extern int32_t VFX_pixel_write(PANE *pane, int32_t x, int32_t y, uint32_t color);
extern int32_t VFX_pixel_read(PANE *pane, int32_t x, int32_t y);

extern int32_t VFX_rectangle_hash(PANE *pane, int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint32_t color);

extern int32_t VFX_pane_wipe(PANE *pane, int32_t color);
extern int32_t VFX_pane_copy(PANE *source, int32_t sx, int32_t sy, PANE *target, int32_t tx, int32_t ty, int32_t fill);

extern int32_t VFX_pane_scroll(PANE *pane, int32_t dx, int32_t dy, int32_t mode, int32_t parm);

extern void VFX_ellipse_draw(PANE *pane, int32_t xc, int32_t yc, int32_t width, int32_t height, int32_t color);
extern void VFX_ellipse_fill(PANE *pane, int32_t xc, int32_t yc, int32_t width, int32_t height, int32_t color);

extern void VFX_point_transform(POINT *in, POINT *out, POINT *origin, int32_t rot, int32_t x_scale, int32_t y_scale);

extern void VFX_Cos_Sin(int32_t Angle, int16_t *Cos, int16_t *Sin);
extern void VFX_fixed_mul(int16_t M1, int16_t M2, int16_t *result);

extern int32_t VFX_font_height(void *font);
extern int32_t VFX_character_width(void *font, int32_t character);
extern int32_t VFX_character_draw(PANE *pane, int32_t x, int32_t y, void *font, int32_t character, uint8_t *color_translate);
extern void VFX_string_draw(PANE *pane, int32_t x, int32_t y, void *font, char *string, uint8_t *color_translate);

extern int32_t VFX_ILBM_draw(PANE *pane, uint8_t *ILBM_buffer);
extern void VFX_ILBM_palette(uint8_t *ILBM_buffer, RGB *palette);
extern int32_t VFX_ILBM_resolution(uint8_t *ILBM_buffer);

extern void VFX_PCX_draw(PANE *pane, uint8_t *PCX_buffer);
extern void VFX_PCX_palette(uint8_t *PCX_buffer, int32_t PCX_file_size, RGB *palette);
extern int32_t VFX_PCX_resolution(uint8_t *PCX_buffer);

extern int32_t VFX_GIF_draw(PANE *pane, uint8_t *GIF_buffer, void *GIF_scratch);
extern void VFX_GIF_palette(uint8_t *GIF_buffer, RGB *palette);
extern int32_t VFX_GIF_resolution(uint8_t *GIF_buffer);

extern int32_t VFX_pixel_fade(PANE *source, PANE *destination, int32_t intervals, int32_t rnd);

extern void VFX_window_fade(WINDOW *buffer, RGB *palette, int32_t intervals);
extern int32_t VFX_color_scan(PANE *pane, uint32_t *colors);

extern void VFX_shape_transform(PANE *pane, void *shape_table, int32_t shape_number, int32_t hotX, int32_t hotY, void *buffer, int32_t rot, int32_t x_scale, int32_t y_scale, int32_t flags);

//
// VFX 3D polygon functions
//

extern void VFX_flat_polygon(PANE *pane, int32_t vcnt, SCRNVERTEX *vlist);
extern void VFX_Gouraud_polygon(PANE *pane, int32_t vcnt, SCRNVERTEX *vlist);
extern void VFX_dithered_Gouraud_polygon(PANE *pane, int16_t dither_amount, int32_t vcnt, SCRNVERTEX *vlist);
extern void VFX_map_lookaside(uint8_t *table);
extern void VFX_map_polygon(PANE *pane, int32_t vcnt, SCRNVERTEX *vlist, WINDOW *texture, uint32_t flags);
extern void VFX_translate_polygon(PANE *pane, int32_t vcnt, SCRNVERTEX *vlist, void *lookaside);
extern void VFX_illuminate_polygon(PANE *pane, int16_t dither_amount, int32_t vcnt, SCRNVERTEX *vlist);

#endif
