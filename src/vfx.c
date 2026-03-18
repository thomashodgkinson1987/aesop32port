#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

#include <stdlib.h>
#include <string.h>

// Tom: added (SDL)
#include <SDL2/SDL.h>
#include "globals.h"

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

//
// Hardware-specific VFX DLL function pointers (instantiations)
//

VFX_DESC *(*VFX_describe_driver)(void) = NULL;
void (*VFX_init_driver)(void) = NULL;
void (*VFX_shutdown_driver)(void) = NULL;
void (*VFX_wait_vblank)(void) = NULL;
void (*VFX_wait_vblank_leading)(void) = NULL;
void (*VFX_area_wipe)(int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t color) = NULL;
void (*VFX_window_refresh)(WINDOW *target, int32_t x0, int32_t y0, int32_t x1, int32_t y1) = NULL;
void (*VFX_window_read)(WINDOW *destination, int32_t x0, int32_t y0, int32_t x1, int32_t y1) = NULL;
void (*VFX_pane_refresh)(PANE *target, int32_t x0, int32_t y0, int32_t x1, int32_t y1) = NULL;
void (*VFX_DAC_read)(int32_t color_number, RGB *triplet) = NULL;
void (*VFX_DAC_write)(int32_t color_number, RGB *triplet) = NULL;
void (*VFX_bank_reset)(void) = NULL;
void (*VFX_line_address)(int32_t x, int32_t y, uint8_t **addr, uint32_t *nbytes) = NULL;

void draw_shape_unclipped(void *buffer, void *shape, int hotX, int hotY, int CP_W);
void save_buffer_to_pgm(const uint8_t *buffer, int width, int height, const char *filename);
void save_shape_to_pgm(void *table, int32_t shape_number, const char *filename);

/**
 * Saves a raw 8-bit image buffer to a PGM file.
 *
 * @param buffer   Pointer to the array containing pixel data
 * @param width    Width of the image in pixels (bytes per line)
 * @param height   Height of the image in lines
 * @param filename Output filename (e.g., "output.pgm")
 */
void save_buffer_to_pgm(
    const uint8_t *buffer,
    int width,
    int height,
    const char *filename)
{
    // Open the output file for binary writing ("wb")
    FILE *out = fopen(filename, "wb");
    if (!out)
    {
        printf("Error: Could not open %s for writing!\n", filename);
        return;
    }

    // Write the PGM Header
    // P5 means "Raw binary grayscale"
    // %d %d are the Width and Height
    // 255 is the maximum pixel value
    fprintf(out, "P5\n%d %d\n255\n", width, height);

    // Write the entire buffer to the file at once
    size_t total_pixels = width * height;
    size_t written = fwrite(buffer, sizeof(uint8_t), total_pixels, out);

    if (written != total_pixels)
    {
        printf("Warning: Expected to write %zu bytes, but wrote %zu\n",
               total_pixels, written);
    }
    else
    {
        printf("Image successfully saved as %s\n", filename);
    }

    fclose(out);
}

void save_shape_to_pgm(void *table, int32_t shape_number, const char *filename)
{
}

void draw_shape_unclipped(void *buffer, void *shape, int hotX, int hotY, int CP_W)
{
    // Recalculate the clipped pane width (CP_W) from the window data.
    // (Note: The assembly receives CP_W as an argument but immediately overwrites
    // it with wnd_x1 + 1, so we do the same here).
    // CP_W = 16;

    // 2. Resolve Sprite Dimensions
    SHAPEHEADER *header = (SHAPEHEADER *)shape;

    CP_W = header->xmax;

    // Calculate the number of scanlines (rows) to draw
    int linecount = header->ymax + 1 - header->ymin;
    if (linecount <= 0)
        return;

    // Calculate the exact starting memory address in the video buffer.
    // Address = Buffer_Base + (Y * ScreenWidth) + X
    int startX = header->xmin + hotX;
    int startY = header->ymin + hotY;
    uint8_t *dest_line_ptr = buffer + (startY * CP_W) + startX;

    // Set our data pointer just past the header to the start of the RLE tokens
    uint8_t *shape_data = (uint8_t *)shape + sizeof(SHAPEHEADER);

    // 3. The Main Scanline Loop
    while (linecount > 0)
    {

        // Set the active drawing pointer to the start of the current row
        uint8_t *dest_ptr = dest_line_ptr;

        // 4. The Token Decoding Loop
        while (1)
        {

            // Read the next instruction token from the sprite data
            uint8_t token = *shape_data++;

            // In the Assembly, `shr al, 1` divides the token by 2 and puts the
            // remainder (bit 0) into the Carry Flag (CF).
            int type = token & 1;   // The operation type (The Carry Flag)
            int count = token >> 1; // The number of pixels to process (AL)

            // Evaluate the token based on the 4 possible states:

            if (count > 0 && type == 0)
            {
                // RUN PACKET (Assembly: ja RunPacket)
                // Action: Draw 'count' pixels of a single repeating color.
                // The next byte in the file is the color to draw.
                uint8_t color = *shape_data++;

                // (Assembly uses RSTOSB32 here to blast 32-bits at a time)
                memset(dest_ptr, color, count);
                dest_ptr += count;
            }

            else if (count > 0 && type == 1)
            {
                // STRING PACKET (Assembly: jnz StringPacket)
                // Action: Copy a sequence of 'count' unique pixels.
                // The next 'count' bytes in the file are the pixel colors.

                // (Assembly uses RMOVSB32 here to copy 32-bits at a time)
                memcpy(dest_ptr, shape_data, count);
                dest_ptr += count;
                shape_data += count;
            }

            else if (count == 0 && type == 1)
            {
                // SKIP PACKET (Assembly: jc SkipPacket or fallthrough)
                // Action: Skip over transparent background pixels.
                // The next byte tells us exactly how many pixels to skip.
                int skip_count = *shape_data++;
                dest_ptr += skip_count;
            }

            else if (count == 0 && type == 0)
            {
                // END PACKET (Assembly: jnc EndPacket)
                // Action: Stop drawing the current scanline.
                break;
            }
        }

        // Advance the base destination pointer exactly one horizontal line down.
        dest_line_ptr += CP_W;

        // Decrement our row counter and loop
        linecount--;
    }
}

//
// Device-independent VFX API functions (VFXC.C)
//

uint32_t VFX_stencil_size(WINDOW *source, uint32_t transparent_color)
{
    printf("[STUB] [vfx] VFX_stencil_size: "
           "source=%p "
           "transparent_color=%u "
           "return=0"
           "\n",
           (void *)source, transparent_color);
    return 0;
}

STENCIL *VFX_stencil_construct(WINDOW *source, void *dest, uint32_t transparent_color)
{
    printf("[STUB] [vfx] VFX_stencil_construct: "
           "source=%p "
           "dest=%p "
           "transparent_color=%u "
           "return=NULL"
           "\n",
           (void *)source, dest, transparent_color);
    return NULL;
}
void VFX_stencil_destroy(STENCIL *stencil)
{
    printf("[STUB] [vfx] VFX_stencil_destroy: stencil=%p\n", (void *)stencil);
}

WINDOW *VFX_window_construct(int32_t width, int32_t height)
{
    printf("[STUB] [vfx] VFX_window_construct: width=%i height=%i return=NULL\n",
           width, height);
    return NULL;
}
void VFX_window_destroy(WINDOW *window)
{
    printf("[STUB] [vfx] VFX_window_destroy: window=%p\n", (void *)window);
}

PANE *VFX_pane_construct(
    WINDOW *window,
    int32_t x0,
    int32_t y0,
    int32_t x1,
    int32_t y1)
{
    printf("[STUB] [vfx] VFX_pane_construct: "
           "window=%p "
           "x0=%i "
           "y0=%i "
           "x1=%i "
           "y1=%i "
           "return=NULL"
           "\n",
           (void *)window, x0, y0, x1, y1);
    return NULL;
}
void VFX_pane_destroy(PANE *pane)
{
    printf("[STUB] [vfx] VFX_pane_destroy: pane=%p\n", (void *)pane);
}

PANE_LIST *VFX_pane_list_construct(int32_t n_entries)
{
    printf("[STUB] [vfx] VFX_pane_list_construct: n_entries=%i return=NULL\n",
           n_entries);
    return NULL;
}
void VFX_pane_list_destroy(PANE_LIST *list)
{
    printf("[STUB] [vfx] VFX_pane_list_destroy: list=%p\n", (void *)list);
}

void VFX_pane_list_clear(PANE_LIST *list)
{
    printf("[STUB] [vfx] VFX_pane_list_clear: list=%p\n", (void *)list);
}

int32_t VFX_pane_list_add(PANE_LIST *list, PANE *target)
{
    printf("[STUB] [vfx] VFX_pane_list_add: list=%p target=%p return=0\n",
           (void *)list, (void *)target);
    return 0;
}
int32_t VFX_pane_list_add_area(
    PANE_LIST *list,
    WINDOW *window,
    int32_t x0,
    int32_t y0,
    int32_t x1,
    int32_t y1)
{
    printf("[STUB] [vfx] VFX_pane_list_add_area: "
           "list=%p "
           "window=%p "
           "x0=%i "
           "y0=%i "
           "x1=%i "
           "y1=%i "
           "return=0"
           "\n",
           (void *)list, (void *)window, x0, y0, x1, y1);
    return 0;
}

void VFX_pane_list_delete_entry(PANE_LIST *list, int32_t entry_num)
{
    printf("[STUB] [vfx] VFX_pane_list_delete_entry: list=%p entry_num=%i\n",
           (void *)list, entry_num);
}

void VFX_pane_list_refresh(PANE_LIST *list)
{
    printf("[STUB] [vfx] VFX_pane_list_refresh: list=%p\n", (void *)list);
}

//
// Device-independent VFX API functions (VFXA.ASM)
//

int8_t *VFX_driver_name(void *VFXScanDLL)
{
    printf("[STUB] [vfx] VFX_driver_name: VFXScanDLL=%p return=NULL\n", VFXScanDLL);
    return NULL;
}

int32_t VFX_register_driver(void *DLLbase)
{
    printf("[STUB] [vfx] VFX_register_driver: DLLbase=%p return=0\n", DLLbase);
    return 0;
}

int32_t VFX_line_draw(
    PANE *pane,
    int32_t x0,
    int32_t y0,
    int32_t x1,
    int32_t y1,
    int32_t mode,
    int32_t parm)
{
    printf("[STUB] [vfx] VFX_line_draw: "
           "pane=%p "
           "x0=%i "
           "y0=%i "
           "x1=%i "
           "y1=%i "
           "mode=%i "
           "parm=%i "
           "return=0"
           "\n",
           (void *)pane, x0, y0, x1, y1, mode, parm);
    return 0;
}

static uint32_t output_count = 0;

void VFX_shape_draw(
    PANE *pane,
    void *shape_table,
    int32_t shape_number,
    int32_t hotX,
    int32_t hotY)
{
    printf("[STUB] [vfx] VFX_shape_draw: "
           "pane=%p "
           "shape_table=%p "
           "shape_number=%i "
           "hotX=%i "
           "hotY=%i"
           "\n",
           (void *)pane, shape_table, shape_number, hotX, hotY);

    ////////

    SHAPETABLEHEADER *shape_table_header = (SHAPETABLEHEADER *)shape_table;

    printf("---- SHAPETABLEHEADER ----\n");
    printf("shape_table_header=%p\n", (void *)shape_table_header);
    printf("version=%x %x %x %x\n", shape_table_header->version_byte_1, shape_table_header->version_byte_2, shape_table_header->version_byte_3, shape_table_header->version_byte_4);
    printf("shape_count=%i\n", shape_table_header->shape_count);
    printf("--------------------------\n");

    if (shape_table_header->shape_count == 0)
        return;

    uint32_t *shape_offsets = (uint32_t *)malloc(sizeof(*shape_offsets) * shape_table_header->shape_count);

    printf("---- SHAPEOFFSETS ----\n");
    for (uint32_t i = 0; i < shape_table_header->shape_count; ++i)
    {
        shape_offsets[i] = *(uint32_t *)((uint32_t)shape_table + sizeof(SHAPETABLEHEADER) + (i * 8));
        printf("shape_offsets[%i]=%x\n", i, shape_offsets[i]);
    }
    printf("----------------------\n");

    for (uint32_t i = 0; i < shape_table_header->shape_count; ++i)
    {
        SHAPEHEADER *shape_header = (SHAPEHEADER *)((uint32_t)shape_table + shape_offsets[i]);

        printf("---- SHAPEHEADER ----\n");
        printf("#%i addr=%p off=%x\n", i, (void *)shape_header, shape_offsets[i]);
        printf("bounds: %x\n", shape_header->bounds);
        printf("origin: %x\n", shape_header->origin);
        printf("xmin: %x\n", shape_header->xmin);
        printf("ymin: %x\n", shape_header->ymin);
        printf("xmax: %x\n", shape_header->xmax);
        printf("ymax: %x\n", shape_header->ymax);
        printf("---------------------\n");

        uint8_t *buffer = (uint8_t *)calloc(320 * 200, 1);

        draw_shape_unclipped(buffer, shape_header, 0, 0, 320);

        char filename[64];
        snprintf(filename, sizeof(filename), "../misc-files/extracted/image_%04u.pgm", output_count++);
        save_buffer_to_pgm(buffer, shape_header->xmax, shape_header->ymax, filename);

        free(buffer);
    }

    free(shape_offsets);
}

void VFX_shape_lookaside(uint8_t *table)
{
    printf("[STUB] [vfx] VFX_shape_lookaside: table=%p\n", (void *)table);
}
void VFX_shape_translate_draw(
    PANE *pane,
    void *shape_table,
    int32_t shape_number,
    int32_t hotX,
    int32_t hotY)
{
    printf("[STUB] [vfx] VFX_shape_translate_draw: "
           "pane=%p "
           "shape_table=%p "
           "shape_number=%i "
           "hotX=%i "
           "hotY=%i"
           "\n",
           (void *)pane, shape_table, shape_number, hotX, hotY);
}

void VFX_shape_remap_colors(void *shape_table, uint32_t shape_number)
{
    printf("[STUB] [vfx] VFX_shape_remap_colors: shape_table=%p shaoe_number=%u\n",
           shape_table, shape_number);
}

void VFX_shape_visible_rectangle(
    void *shape_table,
    int32_t shape_number,
    int32_t hotX,
    int32_t hotY,
    int32_t mirror,
    int32_t *rectangle)
{
    printf("[STUB] [vfx] VFX_shape_visible_rectangle: "
           "shape_table=%p "
           "shape_number=%i "
           "hotX=%i "
           "hotY=%i "
           "mirror=%i "
           "rectangle=%p"
           "\n",
           shape_table, shape_number, hotX, hotY, mirror, (void *)rectangle);
}

int32_t VFX_shape_scan(
    PANE *pane,
    uint32_t transparentColor,
    int32_t hotX,
    int32_t hotY,
    void *buffer)
{
    printf("[STUB] [vfx] VFX_shape_scan: "
           "pane=%p "
           "transparentColor=%u "
           "hotX=%i "
           "hotY=%i "
           "buffer=%p "
           "return=0"
           "\n",
           (void *)pane, transparentColor, hotX, hotY, buffer);
    return 0;
}
int32_t VFX_shape_bounds(void *shape_table, int32_t shape_num)
{
    printf("[STUB] [vfx] VFX_shape_bounds: shape_table=%p shape_num=%i return=0\n",
           shape_table, shape_num);
    return 0;
}
int32_t VFX_shape_origin(void *shape_table, int32_t shape_num)
{
    printf("[STUB] [vfx] VFX_shape_origin: shape_table=%p shape_num=%i return=0\n",
           shape_table, shape_num);
    return 0;
}
int32_t VFX_shape_resolution(void *shape_table, int32_t shape_num)
{
    printf("[STUB] [vfx] VFX_shape_resolution: "
           "shape_table=%p "
           "shape_num=%i "
           "return=0"
           "\n",
           shape_table, shape_num);
    return 0;
}
int32_t VFX_shape_minxy(void *shape_table, int32_t shape_num)
{
    printf("[STUB] [vfx] VFX_shape_minxy: shape_table=%p shape_num=%i return=0\n",
           shape_table, shape_num);
    return 0;
}
void VFX_shape_palette(void *shape_table, int32_t shape_num, RGB *palette)
{
    printf("[STUB] [vfx] VFX_shape_palette: "
           "shape_table=%p "
           "shape_num=%i "
           "palette=%p"
           "\n",
           shape_table, shape_num, (void *)palette);
}
int32_t VFX_shape_colors(void *shape_table, int32_t shape_num, CRGB *colors)
{
    printf("[STUB] [vfx] VFX_shape_colors: "
           "shape_table=%p "
           "shape_num=%i "
           "colors=%p "
           "return=0"
           "\n",
           shape_table, shape_num, (void *)colors);
    return 0;
}
int32_t VFX_shape_set_colors(void *shape_table, int32_t shape_number, CRGB *colors)
{
    printf("[STUB] [vfx] VFX_shape_set_colors: "
           "shape_table=%p "
           "shape_number=%i "
           "colors=%p "
           "return=0"
           "\n",
           shape_table, shape_number, (void *)colors);
    return 0;
}
int32_t VFX_shape_count(void *shape_table)
{
    printf("[STUB] [vfx] VFX_shape_count: shape_table=%p return=0\n", shape_table);
    return 0;
}
int32_t VFX_shape_list(void *shape_table, uint32_t *index_list)
{
    printf("[STUB] [vfx] VFX_shape_list: shape_table=%p index_list=%p return=0\n",
           shape_table, (void *)index_list);
    return 0;
}
int32_t VFX_shape_palette_list(void *shape_table, uint32_t *index_list)
{
    printf("[STUB] [vfx] VFX_shape_palette_list: "
           "shape_table=%p "
           "index_list=%p "
           "return=0"
           "\n",
           shape_table, (void *)index_list);
    return 0;
}

int32_t VFX_pixel_write(PANE *pane, int32_t x, int32_t y, uint32_t color)
{
    printf("[STUB] [vfx] VFX_pixel_write: pane=%p x=%i y=%i color=%u return=0\n",
           (void *)pane, x, y, color);
    return 0;
}
int32_t VFX_pixel_read(PANE *pane, int32_t x, int32_t y)
{
    printf("[STUB] [vfx] VFX_pixel_read: pane=%p x=%i y=%i return=0\n",
           (void *)pane, x, y);
    return 0;
}

int32_t VFX_rectangle_hash(
    PANE *pane,
    int32_t x0,
    int32_t y0,
    int32_t x1,
    int32_t y1,
    uint32_t color)
{
    printf("[STUB] [vfx] VFX_rectangle_hash: "
           "pane=%p "
           "x0=%i "
           "y0=%i "
           "x1=%i "
           "y1=%i "
           "color=%u "
           "return=0"
           "\n",
           (void *)pane, x0, y0, x1, y1, color);
    return 0;
}

int32_t VFX_pane_wipe(PANE *pane, int32_t color)
{
    printf("[STUB] [vfx] VFX_pane_wipe: pane=%p color=%i return=0\n",
           (void *)pane, color);
    return 0;
}
int32_t VFX_pane_copy(
    PANE *source,
    int32_t sx,
    int32_t sy,
    PANE *target,
    int32_t tx,
    int32_t ty,
    int32_t fill)
{
    printf("[STUB] [vfx] VFX_pane_copy: "
           "source=%p "
           "sx=%i "
           "sy=%i "
           "target=%p "
           "tx=%i "
           "ty=%i "
           "fill=%i "
           "return=0"
           "\n",
           (void *)source, sx, sy, (void *)target, tx, ty, fill);
    return 0;
}

int32_t VFX_pane_scroll(
    PANE *pane,
    int32_t dx,
    int32_t dy,
    int32_t mode,
    int32_t parm)
{
    printf("[STUB] [vfx] VFX_pane_scroll: "
           "pane=%p "
           "dx=%i "
           "dy=%i "
           "mode=%i "
           "parm=%i "
           "return=0"
           "\n",
           (void *)pane, dx, dy, mode, parm);
    return 0;
}

void VFX_ellipse_draw(
    PANE *pane,
    int32_t xc,
    int32_t yc,
    int32_t width,
    int32_t height,
    int32_t color)
{
    printf("[STUB] [vfx] VFX_ellipse_draw: "
           "pane=%p "
           "xc=%i "
           "yc=%i "
           "width=%i "
           "height=%i "
           "color=%i"
           "\n",
           (void *)pane, xc, yc, width, height, color);
}
void VFX_ellipse_fill(
    PANE *pane,
    int32_t xc,
    int32_t yc,
    int32_t width,
    int32_t height,
    int32_t color)
{
    printf("[STUB] [vfx] VFX_ellipse_fill: "
           "pane=%p "
           "xc=%i "
           "yc=%i "
           "width=%i "
           "height=%i "
           "color=%i"
           "\n",
           (void *)pane, xc, yc, width, height, color);
}

void VFX_point_transform(
    POINT *in,
    POINT *out,
    POINT *origin,
    int32_t rot,
    int32_t x_scale,
    int32_t y_scale)
{
    printf("[STUB] [vfx] VFX_point_transform: "
           "in=%p "
           "out=%p "
           "origin=%p "
           "rot=%i "
           "x_scale=%i "
           "y_scale=%i"
           "\n",
           (void *)in, (void *)out, (void *)origin, rot, x_scale, y_scale);
}

void VFX_Cos_Sin(int32_t Angle, int16_t *Cos, int16_t *Sin)
{
    printf("[STUB] [vfx] VFX_Cos_Sin: Angle=%i Cos=%p Sin=%p\n",
           Angle, (void *)Cos, (void *)Sin);
}
void VFX_fixed_mul(int16_t M1, int16_t M2, int16_t *result)
{
    printf("[STUB] [vfx] VFX_fixed_mul: M1=%i M2=%i result=%p\n",
           M1, M2, (void *)result);
}

int32_t VFX_font_height(void *font)
{
    printf("[STUB] [vfx] VFX_font_height: font=%p return=0\n", font);
    return 0;
}
int32_t VFX_character_width(void *font, int32_t character)
{
    printf("[STUB] [vfx] VFX_character_width: font=%p character=%i return=0\n",
           font, character);
    return 0;
}
int32_t VFX_character_draw(
    PANE *pane,
    int32_t x,
    int32_t y,
    void *font,
    int32_t character,
    uint8_t *color_translate)
{
    printf("[STUB] [vfx] VFX_character_draw: "
           "pane=%p "
           "x=%i "
           "y=%i "
           "font=%p "
           "character=%i "
           "color_translate=%p "
           "return=0"
           "\n",
           (void *)pane, x, y, font, character, (void *)color_translate);
    return 0;
}
void VFX_string_draw(
    PANE *pane,
    int32_t x,
    int32_t y,
    void *font,
    char *string,
    uint8_t *color_translate)
{
    printf("[STUB] [vfx] VFX_string_draw: "
           "pane=%p "
           "x=%i "
           "y=%i "
           "font=%p "
           "string=%s "
           "color_translate=%p"
           "\n",
           (void *)pane, x, y, font, string, color_translate);
}

int32_t VFX_ILBM_draw(PANE *pane, uint8_t *ILBM_buffer)
{
    printf("[STUB] [vfx] VFX_ILBM_draw: pane=%p ILBM_buffer=%p return=0\n",
           (void *)pane, (void *)ILBM_buffer);
    return 0;
}
void VFX_ILBM_palette(uint8_t *ILBM_buffer, RGB *palette)
{
    printf("[STUB] [vfx] VFX_ILBM_palette: ILBM_buffer=%p palette=%p\n",
           (void *)ILBM_buffer, (void *)palette);
}
int32_t VFX_ILBM_resolution(uint8_t *ILBM_buffer)
{
    printf("[STUB] [vfx] VFX_ILBM_resolution: ILBM_buffer=%p return=0\n",
           (void *)ILBM_buffer);
    return 0;
}

void VFX_PCX_draw(PANE *pane, uint8_t *PCX_buffer)
{
    printf("[STUB] [vfx] VFX_PCX_draw: pane=%p PCX_buffer=%p\n",
           (void *)pane, (void *)PCX_buffer);
}
void VFX_PCX_palette(uint8_t *PCX_buffer, int32_t PCX_file_size, RGB *palette)
{
    printf("[STUB] [vfx] VFX_PCX_palette: "
           "PCX_buffer=%p "
           "PCX_file_size=%i "
           "palette=%p"
           "\n",
           (void *)PCX_buffer, PCX_file_size, (void *)palette);
}
int32_t VFX_PCX_resolution(uint8_t *PCX_buffer)
{
    printf("[STUB] [vfx] VFX_PCX_resolution: PCX_buffer=%p return=0\n",
           (void *)PCX_buffer);
    return 0;
}

int32_t VFX_GIF_draw(PANE *pane, uint8_t *GIF_buffer, void *GIF_scratch)
{
    printf("[STUB] [vfx] VFX_GIF_draw: "
           "pane=%p "
           "GIF_buffer=%p "
           "GIF_scratch=%p "
           "return=0"
           "\n",
           (void *)pane, (void *)GIF_buffer, GIF_scratch);
    return 0;
}
void VFX_GIF_palette(uint8_t *GIF_buffer, RGB *palette)
{
    printf("[STUB] [vfx] VFX_GIF_palette: GIF_buffer=%p palette=%p\n",
           (void *)GIF_buffer, (void *)palette);
}
int32_t VFX_GIF_resolution(uint8_t *GIF_buffer)
{
    printf("[STUB] [vfx] VFX_GIF_resolution: GIF_buffer=%p return=0\n",
           (void *)GIF_buffer);
    return 0;
}

int32_t VFX_pixel_fade(
    PANE *source,
    PANE *destination,
    int32_t intervals,
    int32_t rnd)
{
    printf("[STUB] [vfx] VFX_pixel_fade: "
           "source=%p "
           "destination=%p "
           "intervals=%i "
           "rnd=%i "
           "return=0"
           "\n",
           (void *)source, (void *)destination, intervals, rnd);
    return 0;
}

void VFX_window_fade(WINDOW *buffer, RGB *palette, int32_t intervals)
{
    printf("[STUB] [vfx] VFX_window_fade: "
           "buffer=%p "
           "palette=%p "
           "intervals=%i"
           "\n",
           (void *)buffer, (void *)palette, intervals);
}
int32_t VFX_color_scan(PANE *pane, uint32_t *colors)
{
    printf("[STUB] [vfx] VFX_color_scan: pane=%p colors=%p return=0\n",
           (void *)pane, (void *)colors);
    return 0;
}

void VFX_shape_transform(
    PANE *pane,
    void *shape_table,
    int32_t shape_number,
    int32_t hotX,
    int32_t hotY,
    void *buffer,
    int32_t rot,
    int32_t x_scale,
    int32_t y_scale,
    int32_t flags)
{
    printf("[STUB] [vfx] VFX_shape_transform: "
           "pane=%p "
           "shape_table=%p "
           "shape_number=%i "
           "hotX=%i "
           "hotY=%i "
           "buffer=%p "
           "rot=%i "
           "x_scale=%i "
           "y_scale=%i "
           "flags=%i"
           "\n",
           (void *)pane, shape_table, shape_number, hotX, hotY, buffer, rot, x_scale, y_scale, flags);

    ////////

    SHAPETABLEHEADER *shape_table_header = (SHAPETABLEHEADER *)shape_table;

    printf("---- SHAPETABLEHEADER ----\n");
    printf("shape_table_header=%p\n", (void *)shape_table_header);
    printf("version=%x %x %x %x\n", shape_table_header->version_byte_1, shape_table_header->version_byte_2, shape_table_header->version_byte_3, shape_table_header->version_byte_4);
    printf("shape_count=%i\n", shape_table_header->shape_count);
    printf("--------------------------\n");

    if (shape_table_header->shape_count == 0)
        return;

    uint32_t *shape_offsets = (uint32_t *)malloc(sizeof(*shape_offsets) * shape_table_header->shape_count);

    printf("---- SHAPEOFFSETS ----\n");
    for (uint32_t i = 0; i < shape_table_header->shape_count; ++i)
    {
        shape_offsets[i] = *(uint32_t *)((uint32_t)shape_table + sizeof(SHAPETABLEHEADER) + (i * 8));
        printf("shape_offsets[%i]=%x\n", i, shape_offsets[i]);
    }
    printf("----------------------\n");

    for (uint32_t i = 0; i < shape_table_header->shape_count; ++i)
    {
        SHAPEHEADER *shape_header = (SHAPEHEADER *)((uint32_t)shape_table + shape_offsets[i]);

        printf("---- SHAPEHEADER ----\n");
        printf("#%i addr=%p off=%x\n", i, (void *)shape_header, shape_offsets[i]);
        printf("bounds: %x\n", shape_header->bounds);
        printf("origin: %x\n", shape_header->origin);
        printf("xmin: %x\n", shape_header->xmin);
        printf("ymin: %x\n", shape_header->ymin);
        printf("xmax: %x\n", shape_header->xmax);
        printf("ymax: %x\n", shape_header->ymax);
        printf("---------------------\n");

        uint8_t *buffer = (uint8_t *)calloc(320 * 200, 1);

        draw_shape_unclipped(buffer, shape_header, 0, 0, 320);

        char filename[64];
        snprintf(filename, sizeof(filename), "../misc-files/extracted/image_%04u.pgm", output_count++);
        save_buffer_to_pgm(buffer, shape_header->xmax, shape_header->ymax, filename);

        free(buffer);
    }

    free(shape_offsets);
}

//
// VFX 3D polygon functions
//

void VFX_flat_polygon(PANE *pane, int32_t vcnt, SCRNVERTEX *vlist)
{
    printf("[STUB] [vfx] VFX_flat_polygon: pane=%p vcnt=%i vlist=%p\n",
           (void *)pane, vcnt, (void *)vlist);
}
void VFX_Gouraud_polygon(PANE *pane, int32_t vcnt, SCRNVERTEX *vlist)
{
    printf("[STUB] [vfx] VFX_Gouraud_polygon: pane=%p vcnt=%i vlist=%p\n",
           (void *)pane, vcnt, (void *)vlist);
}
void VFX_dithered_Gouraud_polygon(
    PANE *pane,
    int16_t dither_amount,
    int32_t vcnt,
    SCRNVERTEX *vlist)
{
    printf("[STUB] [vfx] VFX_dithered_Gouraud_polygon: "
           "pane=%p "
           "dither_amount=%i "
           "vcnt=%i "
           "vlist=%p"
           "\n",
           (void *)pane, dither_amount, vcnt, (void *)vlist);
}
void VFX_map_lookaside(uint8_t *table)
{
    printf("[STUB] [vfx] VFX_map_lookaside: table=%p\n", (void *)table);
}
void VFX_map_polygon(
    PANE *pane,
    int32_t vcnt,
    SCRNVERTEX *vlist,
    WINDOW *texture,
    uint32_t flags)
{
    printf("[STUB] [vfx] VFX_map_polygon: "
           "pane=%p "
           "vcnt=%i "
           "vlist=%p "
           "texture=%p "
           "flags=%u"
           "\n",
           (void *)pane, vcnt, (void *)vlist, (void *)texture, flags);
}
void VFX_translate_polygon(
    PANE *pane,
    int32_t vcnt,
    SCRNVERTEX *vlist,
    void *lookaside)
{
    printf("[STUB] [vfx] VFX_translate_polygon: "
           "pane=%p "
           "vcnt=%i "
           "vlist=%p "
           "lookaside=%p"
           "\n",
           (void *)pane, vcnt, (void *)vlist, lookaside);
}
void VFX_illuminate_polygon(
    PANE *pane,
    int16_t dither_amount,
    int32_t vcnt,
    SCRNVERTEX *vlist)
{
    printf("[STUB] [vfx] VFX_illuminate_polygon: "
           "pane=%p "
           "dither_amount=%i "
           "vcnt=%i "
           "vlist=%p"
           "\n",
           (void *)pane, dither_amount, vcnt, (void *)vlist);
}
