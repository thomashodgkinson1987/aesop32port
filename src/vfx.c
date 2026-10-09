#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include "vfx.h"

#include "utils.h"

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

// Color-translation table for ST_XLAT (fade lookaside), set by
// VFX_shape_lookaside()
static uint8_t *shape_lookaside_table = NULL;

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

//
// Per-pixel operation for VFX_line_draw's Bresenham walk. Coordinates
// are window (buffer) coordinates, matching the original
// (CONVERT_QUAD_PANE_TO_WINDOW precedes rasterization in VFXA.ASM).
//

static void line_plot_pixel(WINDOW *w, int32_t stride, int32_t X, int32_t Y,
                            int32_t mode, uint8_t color,
                            const uint8_t *xlat,
                            void (*callback)(int32_t x, int32_t y))
{
    uint8_t *dst = w->buffer + (Y * stride) + X;

    switch (mode)
    {
    case LD_DRAW:
        *dst = color;
        break;
    case LD_TRANSLATE:
        if (xlat != NULL)
            *dst = xlat[*dst];
        break;
    case LD_EXECUTE:
        if (callback != NULL)
            callback(X, Y);
        break;
    default:
        break;
    }
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
    WINDOW *w;
    int32_t clip_x0, clip_y0, clip_x1, clip_y1;
    int32_t X0, Y0, X1, Y1;
    int32_t clipped = 0;
    int32_t stride;
    int32_t dx, dy, sx, sy, err, e2;
    uint8_t color = 0;
    const uint8_t *xlat = NULL;
    void (*callback)(int32_t x, int32_t y) = NULL;

    if (pane == NULL)
        return -2;

    w = pane->window;

    if (w == NULL || w->buffer == NULL)
        return -1;

    // Inclusive clip rect: pane intersected with the window, in buffer
    // coordinates (same convention as shape_draw_rle).
    clip_x0 = (pane->x0 > 0) ? pane->x0 : 0;
    clip_y0 = (pane->y0 > 0) ? pane->y0 : 0;
    clip_x1 = (pane->x1 < w->x_max) ? pane->x1 : w->x_max;
    clip_y1 = (pane->y1 < w->y_max) ? pane->y1 : w->y_max;

    if (clip_x0 > clip_x1 || clip_y0 > clip_y1)
        return -2;

    // Endpoints from pane-local to buffer coordinates.
    X0 = pane->x0 + x0;
    Y0 = pane->y0 + y0;
    X1 = pane->x0 + x1;
    Y1 = pane->y0 + y1;

    if (mode == LD_DRAW)
        color = (uint8_t)(parm & 0xff);
    else if (mode == LD_TRANSLATE)
        xlat = (const uint8_t *)(uintptr_t)(uint32_t)parm;
    else if (mode == LD_EXECUTE)
        callback = (void (*)(int32_t x, int32_t y))(uintptr_t)(uint32_t)parm;

    // Cohen-Sutherland clip to the inclusive clip rect.
    for (;;)
    {
        int32_t out0 = 0;
        int32_t out1 = 0;

        if (X0 < clip_x0)
            out0 |= 1;
        else if (X0 > clip_x1)
            out0 |= 2;
        if (Y0 < clip_y0)
            out0 |= 4;
        else if (Y0 > clip_y1)
            out0 |= 8;

        if (X1 < clip_x0)
            out1 |= 1;
        else if (X1 > clip_x1)
            out1 |= 2;
        if (Y1 < clip_y0)
            out1 |= 4;
        else if (Y1 > clip_y1)
            out1 |= 8;

        if ((out0 | out1) == 0)
            break;

        if ((out0 & out1) != 0)
            return 2;

        clipped = 1;

        // Move the outside endpoint onto the crossed edge. The crossed
        // edge always spans the other axis, so no division by zero.
        if ((out0 & 1) != 0)
        {
            Y0 = Y0 + (Y1 - Y0) * (clip_x0 - X0) / (X1 - X0);
            X0 = clip_x0;
        }
        else if ((out0 & 2) != 0)
        {
            Y0 = Y0 + (Y1 - Y0) * (clip_x1 - X0) / (X1 - X0);
            X0 = clip_x1;
        }
        else if ((out0 & 4) != 0)
        {
            X0 = X0 + (X1 - X0) * (clip_y0 - Y0) / (Y1 - Y0);
            Y0 = clip_y0;
        }
        else if ((out0 & 8) != 0)
        {
            X0 = X0 + (X1 - X0) * (clip_y1 - Y0) / (Y1 - Y0);
            Y0 = clip_y1;
        }
        else if ((out1 & 1) != 0)
        {
            Y1 = Y1 + (Y0 - Y1) * (clip_x0 - X1) / (X0 - X1);
            X1 = clip_x0;
        }
        else if ((out1 & 2) != 0)
        {
            Y1 = Y1 + (Y0 - Y1) * (clip_x1 - X1) / (X0 - X1);
            X1 = clip_x1;
        }
        else if ((out1 & 4) != 0)
        {
            X1 = X1 + (X0 - X1) * (clip_y0 - Y1) / (Y0 - Y1);
            Y1 = clip_y0;
        }
        else
        {
            X1 = X1 + (X0 - X1) * (clip_y1 - Y1) / (Y0 - Y1);
            Y1 = clip_y1;
        }
    }

    stride = w->x_max + 1;

    // Bresenham walk from (X0,Y0) to (X1,Y1): every plotted point is
    // within half a pixel of the ideal line in the minor axis, per the
    // VFX line-draw contract.
    dx = (X1 > X0) ? (X1 - X0) : (X0 - X1);
    dy = (Y1 > Y0) ? (Y1 - Y0) : (Y0 - Y1);
    sx = (X0 < X1) ? 1 : -1;
    sy = (Y0 < Y1) ? 1 : -1;
    err = dx - dy;

    for (;;)
    {
        line_plot_pixel(w, stride, X0, Y0, mode, color, xlat, callback);

        if (X0 == X1 && Y0 == Y1)
            break;

        e2 = 2 * err;

        if (e2 > -dy)
        {
            err -= dy;
            X0 += sx;
        }
        if (e2 < dx)
        {
            err += dx;
            Y0 += sy;
        }
    }

    return clipped ? 1 : 0;
}

//
// Shape-table geometry, from the EOB3 shape format (see utils.h):
//
//   SHAPEHEADER: bounds ('width<<16 | height'), origin ('hotX<<16 |
//     hotY'), xmin/ymin/xmax/ymax (inclusive sprite box), then RLE data
//
// VFX_shape_bounds() returns 'width<<16 | height' — GIL2VFX.C decodes
// it as '>> 16' for width and '(WORD)' cast for height; resolution is
// the same packing; minxy packs xmin/ymin; origin packs the hotspot.
//

static SHAPEHEADER *shape_header(void *shape_table, int32_t shape_num)
{
    if (shape_table == NULL || shape_table_get_shape_count(shape_table) <= shape_num)
        return NULL;

    return (SHAPEHEADER *)shape_table_get_shape(shape_table, shape_num);
}

//
// Draw a shape straight from its RLE stream into a window buffer at
// (pos_x, pos_y), with pane clipping and optional color translation
// (fade lookaside).
//
// This is a port of the original DrawShapeUnclipped() (VFX 1.19,
// VFXA.ASM:2824-2973), which VFX_shape_draw() drives after pane
// clipping.  The verified semantics, straight from that source:
//
//   - The RLE stream is decoded directly to the destination; there is
//     NO transparent color anywhere in shape drawing.  Run packets
//     (RSTOSB32) and string packets (RMOVSB32) write every pixel they
//     encode; skip packets are the one and only transparency mechanism.
//     (An earlier version of this port white-filled a decode buffer
//     and skipped color 255 — that is VFX_shape_transform()'s internal
//     intermediate-buffer convention (VFXA.ASM:3917), not a rule of
//     shape drawing, and it punched holes in the outtake letterbox
//     shapes, which legitimately draw palette-255 white pixels.)
//
//   - linecount = ymax + 1 - ymin: ymax is inclusive.
//
//   - Destination anchoring: shape pixel (xmin, ymin) lands at
//     pane->x0 + hotX, pane->y0 + hotY (the RLE stream then continues
//     right/down from there).  DOS adds the pane offset to the hotspot
//     and indexes lines from ymin (VFXA.ASM:2861-2875).
//
//   - DOS clips to the pane rect intersected with the window
//     (CP_L..CP_R / CP_T..CP_B); lines outside the vertical clip are
//     still walked to stay in sync with the stream (the original's
//     skipLine loop).  We do the same with per-pixel clip checks.
//
// pos_x/pos_y are pane-local (draw_bitmap subtracts the pane origin);
// for the mouse merge path the caller passes pane coords that already
// encode the buffer position.
//

static void shape_draw_rle(PANE *pane, const SHAPEHEADER *h,
                           int32_t pos_x, int32_t pos_y, const uint8_t *xlat)
{
    WINDOW *w = pane->window;

    if (w == NULL || w->buffer == NULL || h == NULL)
        return;

    int32_t clip_x0 = (pane->x0 > 0) ? pane->x0 : 0;
    int32_t clip_y0 = (pane->y0 > 0) ? pane->y0 : 0;
    int32_t clip_x1 = (pane->x1 < w->x_max) ? pane->x1 : w->x_max;
    int32_t clip_y1 = (pane->y1 < w->y_max) ? pane->y1 : w->y_max;

    if (clip_x0 > clip_x1 || clip_y0 > clip_y1)
        return;

    int32_t base_x = pane->x0 + pos_x + h->xmin;
    int32_t base_y = pane->y0 + pos_y + h->ymin;

    int32_t linecount = h->ymax + 1 - h->ymin;

    if (linecount <= 0)
        return;

    const uint8_t *p = (const uint8_t *)h + sizeof(SHAPEHEADER);
    int32_t win_w = w->x_max + 1;

    for (int32_t line = 0; line < linecount; line++)
    {
        int32_t dy = base_y + line;

        if (dy >= clip_y0 && dy <= clip_y1)
        {
            uint8_t *dst_row = w->buffer + (dy * win_w);
            int32_t x = 0; // pixel offset within this line

            for (;;)
            {
                uint8_t tok = *p++;
                int32_t type = tok & 1;
                int32_t count = tok >> 1;

                if (count > 0 && type == 0)
                {
                    // run packet: 'count' pixels of one color
                    uint8_t color = *p++;

                    for (int32_t i = 0; i < count; i++)
                    {
                        int32_t dx = base_x + x + i;

                        if (dx >= clip_x0 && dx <= clip_x1)
                        {
                            uint8_t px = color;

                            if (xlat != NULL)
                                px = xlat[px];

                            dst_row[dx] = px;
                        }
                    }

                    x += count;
                }
                else if (count > 0 && type == 1)
                {
                    // string packet: 'count' literal pixels
                    for (int32_t i = 0; i < count; i++)
                    {
                        int32_t dx = base_x + x + i;

                        if (dx >= clip_x0 && dx <= clip_x1)
                        {
                            uint8_t px = p[i];

                            if (xlat != NULL)
                                px = xlat[px];

                            dst_row[dx] = px;
                        }
                    }

                    p += count;
                    x += count;
                }
                else if (count == 0 && type == 1)
                {
                    // skip packet: transparent area
                    x += *p++;
                }
                else
                {
                    // end packet
                    break;
                }
            }
        }
        else
        {
            //
            // Line outside the vertical clip: walk the tokens without
            // drawing, exactly like the original's skipLine loop, so
            // the stream stays in sync.
            //

            for (;;)
            {
                uint8_t tok = *p++;
                int32_t type = tok & 1;
                int32_t count = tok >> 1;

                if (count > 0 && type == 0)
                    p += 1; // color byte
                else if (count > 0 && type == 1)
                    p += count; // literal data
                else if (count == 0 && type == 1)
                    p += 1; // skip byte
                else
                    break; // end packet
            }
        }
    }
}

void VFX_shape_draw(
    PANE *pane,
    void *shape_table,
    int32_t shape_number,
    int32_t hotX,
    int32_t hotY)
{
    SHAPEHEADER *h = shape_header(shape_table, shape_number);

    if (h == NULL)
        return;

    shape_draw_rle(pane, h, hotX, hotY, NULL);
}

void VFX_shape_translate_draw(
    PANE *pane,
    void *shape_table,
    int32_t shape_number,
    int32_t hotX,
    int32_t hotY)
{
    //
    // As VFX_shape_draw(), but colors pass through the table last set
    // by VFX_shape_lookaside()
    //

    SHAPEHEADER *h = shape_header(shape_table, shape_number);

    if (h == NULL)
        return;

    shape_draw_rle(pane, h, hotX, hotY, shape_lookaside_table);
}

void VFX_shape_lookaside(uint8_t *table)
{
    shape_lookaside_table = table;
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
    //
    // Trace the shape's RLE stream and return the smallest rectangle
    // holding its visible (drawn) pixels. Port of VFXA.ASM's
    // VFX_shape_visible_rectangle: run/string packets contribute
    // [x, x+count), skip packets only advance x, vx1/vy1 are exclusive
    // maxima, mirroring folds around 2*hot (2*hot-vmax, 2*hot-vmin).
    // Coordinates are in the caller's hotspot space (hotX/hotY as
    // passed in); an empty shape yields {0,0,0,0} instead of the
    // original's INT32 extremes so callers' nothing-visible test
    // (bounds[0] > bounds[2]) keeps working.
    //

    SHAPEHEADER *h = shape_header(shape_table, shape_number);
    int32_t linecount;
    const uint8_t *p;
    int32_t vx0, vy0, vx1, vy1;
    int32_t x0, y0;
    int32_t y, line;
    int found = 0;

    printf("[vfx] VFX_shape_visible_rectangle: "
           "shape_table=%p "
           "shape_number=%i "
           "hotX=%i "
           "hotY=%i "
           "mirror=%i "
           "rectangle=%p"
           "\n",
           shape_table, shape_number, hotX, hotY, mirror, (void *)rectangle);

    if (rectangle == NULL)
        return;

    rectangle[0] = 0;
    rectangle[1] = 0;
    rectangle[2] = 0;
    rectangle[3] = 0;

    if (h == NULL)
        return;

    x0 = h->xmin + hotX;
    y0 = h->ymin + hotY;

    linecount = h->ymax + 1 - h->ymin;

    if (linecount <= 0)
        return;

    vx0 = INT32_MAX;
    vy0 = INT32_MAX;
    vx1 = INT32_MIN;
    vy1 = INT32_MIN;

    p = (const uint8_t *)h + sizeof(SHAPEHEADER);
    y = y0;

    for (line = 0; line < linecount; line++)
    {
        int32_t x = x0;

        for (;;)
        {
            uint8_t tok = *p++;
            int32_t type = tok & 1;
            int32_t count = tok >> 1;

            if (count > 0 && type == 0)
            {
                // run packet: 'count' pixels of one color
                p += 1;

                if (x < vx0)
                    vx0 = x;

                x += count;

                if (x > vx1)
                    vx1 = x;

                if (y < vy0)
                    vy0 = y;

                if (y > vy1)
                    vy1 = y;

                found = 1;
            }
            else if (count > 0 && type == 1)
            {
                // string packet: 'count' literal pixels
                p += count;

                if (x < vx0)
                    vx0 = x;

                x += count;

                if (x > vx1)
                    vx1 = x;

                if (y < vy0)
                    vy0 = y;

                if (y > vy1)
                    vy1 = y;

                found = 1;
            }
            else if (count == 0 && type == 1)
            {
                // skip packet: transparent area
                x += *p++;
            }
            else
            {
                // end packet
                break;
            }
        }

        y++;
    }

    if (!found)
        return;

    if (mirror & VR_X_MIRROR)
    {
        int32_t nx0 = hotX + hotX - vx1;
        int32_t nx1 = hotX + hotX - vx0;

        vx0 = nx0;
        vx1 = nx1;
    }

    if (mirror & VR_Y_MIRROR)
    {
        int32_t ny0 = hotY + hotY - vy1;
        int32_t ny1 = hotY + hotY - vy0;

        vy0 = ny0;
        vy1 = ny1;
    }

    rectangle[0] = vx0;
    rectangle[1] = vy0;
    rectangle[2] = vx1;
    rectangle[3] = vy1;
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
    SHAPEHEADER *h = shape_header(shape_table, shape_num);

    if (h == NULL)
        return 0;

    //
    // Width/height in pixels = xmax - xmin / ymax - ymin. Verified
    // against the resource data: the menu bg header is xmax=319
    // ymax=199 with bounds 0x013f00c7 (319x199), and the cursor
    // shapes are xmax=16 ymax=16 with bounds 0x00100010 (16x16).
    //

    return (int32_t)(((uint32_t)(h->xmax - h->xmin) << 16) |
                     (uint32_t)(h->ymax - h->ymin));
}
int32_t VFX_shape_origin(void *shape_table, int32_t shape_num)
{
    SHAPEHEADER *h = shape_header(shape_table, shape_num);

    if (h == NULL)
        return 0;

    return h->origin;
}
int32_t VFX_shape_resolution(void *shape_table, int32_t shape_num)
{
    return VFX_shape_bounds(shape_table, shape_num);
}
int32_t VFX_shape_minxy(void *shape_table, int32_t shape_num)
{
    SHAPEHEADER *h = shape_header(shape_table, shape_num);

    if (h == NULL)
        return 0;

    return (int32_t)(((uint32_t)(uint16_t)h->xmin << 16) |
                     (uint32_t)(uint16_t)h->ymin);
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
    int32_t count = (shape_table != NULL) ? shape_table_get_shape_count(shape_table) : 0;

    printf("[vfx] VFX_shape_count: shape_table=%p return=%i\n", shape_table, count);

    return count;
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

//
// Single-pixel access. Coordinates are window (buffer) coordinates —
// the GIL2VFX dot wrappers subtract the pane origin before calling —
// matching VFX_line_draw's convention in this port (the original took
// pane coords and converted with CONVERT_REG_PAIR_PANE_TO_WINDOW).
// Return contract mirrors VFXA.ASM: prior pixel 0..255, -1 bad
// window, -2 bad pane, -3 off pane.
//

static int32_t pixel_clip(const PANE *pane, int32_t x, int32_t y,
                           int32_t *win_w_out, int32_t *adr_out)
{
    const WINDOW *w;
    int32_t cx0, cy0, cx1, cy1;

    if (pane == NULL)
        return -2;

    w = pane->window;

    if (w == NULL || w->buffer == NULL || w->x_max < 0 || w->y_max < 0)
        return -1;

    if (pane->x1 < pane->x0 || pane->y1 < pane->y0)
        return -2;

    cx0 = (pane->x0 > 0) ? pane->x0 : 0;
    cy0 = (pane->y0 > 0) ? pane->y0 : 0;
    cx1 = (pane->x1 < w->x_max) ? pane->x1 : w->x_max;
    cy1 = (pane->y1 < w->y_max) ? pane->y1 : w->y_max;

    if (x < cx0 || x > cx1 || y < cy0 || y > cy1)
        return -3;

    *win_w_out = w->x_max + 1;
    *adr_out = y * (w->x_max + 1) + x;

    return 0;
}

int32_t VFX_pixel_write(PANE *pane, int32_t x, int32_t y, uint32_t color)
{
    WINDOW *w;
    int32_t win_w, adr;
    int32_t rc;
    uint8_t prior;

    printf("[vfx] VFX_pixel_write: pane=%p x=%i y=%i color=%u\n",
           (void *)pane, x, y, color);

    rc = pixel_clip(pane, x, y, &win_w, &adr);

    if (rc != 0)
        return rc;

    (void)win_w;

    w = pane->window;
    prior = w->buffer[adr];
    w->buffer[adr] = (uint8_t)color;

    return (int32_t)prior;
}
int32_t VFX_pixel_read(PANE *pane, int32_t x, int32_t y)
{
    WINDOW *w;
    int32_t win_w, adr;
    int32_t rc;

    printf("[vfx] VFX_pixel_read: pane=%p x=%i y=%i\n",
           (void *)pane, x, y);

    rc = pixel_clip(pane, x, y, &win_w, &adr);

    if (rc != 0)
        return rc;

    (void)win_w;

    w = pane->window;

    return (int32_t)w->buffer[adr];
}

int32_t VFX_rectangle_hash(
    PANE *pane,
    int32_t x0,
    int32_t y0,
    int32_t x1,
    int32_t y1,
    uint32_t color)
{
    //
    // Checkerboard fill: every other pixel, odd rows offset by one —
    // the __odd_or_even/__write_pixel loop of VFXA.ASM's
    // VFX_rectangle_hash, whose row parity comes from the remaining
    // line count ((y1-y0-i) & 1). Coordinates are window (buffer)
    // coordinates like the other port rasterizers; the rect is
    // clipped to the pane intersected with the window. Returns 0,
    // -1 bad window, -2 bad pane, -4 bad rectangle.
    //

    WINDOW *w;
    int32_t cx0, cy0, cx1, cy1;
    int32_t stride;
    int32_t row, i;
    uint8_t c;

    printf("[vfx] VFX_rectangle_hash: "
           "pane=%p "
           "x0=%i "
           "y0=%i "
           "x1=%i "
           "y1=%i "
           "color=%u"
           "\n",
           (void *)pane, x0, y0, x1, y1, color);

    if (pane == NULL)
        return -2;

    w = pane->window;

    if (w == NULL || w->buffer == NULL || w->x_max < 0 || w->y_max < 0)
        return -1;

    if (pane->x1 < pane->x0 || pane->y1 < pane->y0)
        return -2;

    cx0 = (x0 > pane->x0) ? x0 : pane->x0;
    cy0 = (y0 > pane->y0) ? y0 : pane->y0;
    cx1 = (x1 < pane->x1) ? x1 : pane->x1;
    cy1 = (y1 < pane->y1) ? y1 : pane->y1;

    if (cx0 < 0)
        cx0 = 0;

    if (cy0 < 0)
        cy0 = 0;

    if (cx1 > w->x_max)
        cx1 = w->x_max;

    if (cy1 > w->y_max)
        cy1 = w->y_max;

    if (cx1 < cx0 || cy1 < cy0)
        return -4;

    stride = w->x_max + 1;
    c = (uint8_t)color;

    for (row = cy0, i = 0; row <= cy1; row++, i++)
    {
        // Original parity: odd remaining-line-count rows start one
        // pixel in (see __odd_or_even: `and edx,1` on lines left).
        int32_t dx = cx0 + (((y1 - y0 - i) & 1) ? 1 : 0);
        uint8_t *dst = w->buffer + (row * stride);

        for (; dx <= cx1; dx += 2)
            dst[dx] = c;
    }

    return 0;
}

int32_t VFX_pane_wipe(PANE *pane, int32_t color)
{
    //
    // Fill the pane's rect with a solid color. Pane coords are buffer
    // coords (screen-aligned owner buffers for subwindow panes; whole
    // buffer for assign_window panes) — the same convention as the
    // refresh source rule in video.c.
    //

    printf("[vfx] VFX_pane_wipe: pane=%p color=%i\n", (void *)pane, color);

    WINDOW *w = (pane != NULL) ? pane->window : NULL;

    if (w == NULL || w->buffer == NULL)
        return 0;

    for (int32_t y = pane->y0; y <= pane->y1; y++)
    {
        if (y < 0 || y > w->y_max)
            continue;

        uint8_t *row = w->buffer + (y * (w->x_max + 1));

        int32_t x0 = (pane->x0 < 0) ? 0 : pane->x0;
        int32_t x1 = (pane->x1 > w->x_max) ? w->x_max : pane->x1;

        if (x0 <= x1)
            memset(row + x0, (uint8_t)color, (size_t)(x1 - x0 + 1));
    }

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
    //
    // Copy the overlap of the two pane rects (each in its own window's
    // buffer coords, offset by (sx,sy)/(tx,ty)). Pixels with no source
    // behind them (outside the source window) are filled with 'fill',
    // or skipped when fill is NO_COLOR (-1). Per-pixel, so overlapping
    // buffers are safe. Used by the mouse cursor merge path.
    //

    printf("[vfx] VFX_pane_copy: source=%p sx=%i sy=%i target=%p tx=%i ty=%i fill=%i\n",
           (void *)source, sx, sy, (void *)target, tx, ty, fill);

    WINDOW *sw = (source != NULL) ? source->window : NULL;
    WINDOW *tw = (target != NULL) ? target->window : NULL;

    if (sw == NULL || sw->buffer == NULL || tw == NULL || tw->buffer == NULL)
        return 0;

    int32_t copy_w = source->x1 - source->x0 + 1;
    int32_t copy_h = source->y1 - source->y0 + 1;

    if (target->x1 - target->x0 + 1 < copy_w)
        copy_w = target->x1 - target->x0 + 1;
    if (target->y1 - target->y0 + 1 < copy_h)
        copy_h = target->y1 - target->y0 + 1;

    for (int32_t row = 0; row < copy_h; row++)
    {
        int32_t syy = source->y0 + sy + row;
        int32_t tyy = target->y0 + ty + row;

        for (int32_t col = 0; col < copy_w; col++)
        {
            int32_t sxx = source->x0 + sx + col;
            int32_t txx = target->x0 + tx + col;

            uint8_t px = 0;
            int have_pixel = 0;

            if (sxx >= 0 && sxx <= sw->x_max && syy >= 0 && syy <= sw->y_max)
            {
                px = sw->buffer[(syy * (sw->x_max + 1)) + sxx];
                have_pixel = 1;
            }

            if (!have_pixel)
            {
                if (fill == NO_COLOR)
                    continue;
                px = (uint8_t)fill;
            }

            if (txx >= 0 && txx <= tw->x_max && tyy >= 0 && tyy <= tw->y_max)
                tw->buffer[(tyy * (tw->x_max + 1)) + txx] = px;
        }
    }

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
void VFX_fixed_mul(int32_t M1, int32_t M2, int32_t *result)
{
    //
    // 16:16 fixed-point multiply: (M1 * M2) >> 16, as in the original
    // 386FX assembly. The ported signature previously said int16_t,
    // truncating the FIXED16 arguments.
    //

    *result = (int32_t)(((int64_t)M1 * (int64_t)M2) >> 16);
}

int32_t VFX_font_height(void *font)
{
    int32_t height;

    if (font == NULL)
    {
        printf("[vfx] VFX_font_height: font=(nil) return=0\n");
        return 0;
    }

    // Per VFXA.ASM VFX_font_height: the height is the FONT header's
    // char_height slot (EOB3 fonts: "2." magic + char_count,
    // char_height, font_background as int32).

    height = ((const FONT *)font)->char_height;

    printf("[vfx] VFX_font_height: font=%p return=%i\n", font, height);

    return height;
}
//
// EOB3/VFX font access: the 16-byte header (version slot holds the
// "2." magic, then char_count, char_height, font_background as
// int32) is followed by char_count uint32 offsets from the font
// base; each character is a uint32 width plus width*height
// row-major bitmap bytes (verified against resource 185: 128
// chars, monotonic offsets, space = width 4 blank).
//
// Returns the character bitmap (or NULL) with its width. Geometry
// safety comes from the caller's pane clipping, but absurd widths
// are rejected outright.
//

static const uint8_t *font_char_bits(const FONT *f, int32_t character, int32_t *width_out)
{
    const uint8_t *base = (const uint8_t *)f;
    uint32_t off;

    if (f == NULL || character < 0 || character >= f->char_count)
        return NULL;

    off = ((const uint32_t *)(base + 16))[character];

    if (off > (uint32_t)INT32_MAX - sizeof(uint32_t))
        return NULL;

    *width_out = *(const int32_t *)(base + off);

    if (*width_out < 0 || *width_out > 256 || f->char_height < 0)
        return NULL;

    return base + off + sizeof(uint32_t);
}

int32_t VFX_character_width(void *font, int32_t character)
{
    int32_t width = 0;

    if (font_char_bits((const FONT *)font, character, &width) == NULL)
        return 0;

    return width;
}
int32_t VFX_character_draw(
    PANE *pane,
    int32_t x,
    int32_t y,
    void *font,
    int32_t character,
    uint8_t *color_translate)
{
    WINDOW *w;
    const uint8_t *bits;
    int32_t width;
    int32_t height;
    int32_t stride;
    int32_t clip_x0, clip_y0, clip_x1, clip_y1;
    int32_t row, col;

    if (pane == NULL)
        return 0;

    w = pane->window;

    if (w == NULL || w->buffer == NULL)
        return 0;

    bits = font_char_bits((const FONT *)font, character, &width);

    if (bits == NULL || width == 0)
        return 0;

    height = ((const FONT *)font)->char_height;

    // Inclusive clip rect, same convention as shape_draw_rle.
    clip_x0 = (pane->x0 > 0) ? pane->x0 : 0;
    clip_y0 = (pane->y0 > 0) ? pane->y0 : 0;
    clip_x1 = (pane->x1 < w->x_max) ? pane->x1 : w->x_max;
    clip_y1 = (pane->y1 < w->y_max) ? pane->y1 : w->y_max;

    stride = w->x_max + 1;

    // Row-major blit; with a translate table, pixels reading 255 are
    // skipped (transparency), per VFX_character_draw's contract. The
    // per-pixel clip checks make even hostile widths safe.
    for (row = 0; row < height; row++)
    {
        int32_t dy = pane->y0 + y + row;

        if (dy < clip_y0 || dy > clip_y1)
            continue;

        for (col = 0; col < width; col++)
        {
            int32_t dx = pane->x0 + x + col;
            uint8_t px;

            if (dx < clip_x0 || dx > clip_x1)
                continue;

            px = bits[(row * width) + col];

            if (color_translate != NULL)
            {
                px = color_translate[px];

                if (px == 255)
                    continue;
            }

            w->buffer[(dy * stride) + dx] = px;
        }
    }

    // Full advance width even when clipped, as the original returns.
    return width;
}
void VFX_string_draw(
    PANE *pane,
    int32_t x,
    int32_t y,
    void *font,
    char *string,
    uint8_t *color_translate)
{
    if (string == NULL)
        return;

    while (*string != 0)
    {
        x += VFX_character_draw(pane, x, y, font, (int32_t)(uint8_t)*string, color_translate);
        string++;
    }
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
    (void)rot;    // the game never rotates shapes (rot always 0)
    (void)buffer; // original wrote the scaled shape to this scratch
                  // buffer; we scale on the fly instead

    SHAPEHEADER *h = shape_header(shape_table, shape_number);

    if (h == NULL || x_scale == 0 || y_scale == 0)
        return;

    //
    // Scale factors are FIXED16 (16:16); 0x10000 = 1.0. Nearest-
    // neighbor sampling: source pixel = dst * 0x10000 / scale.
    //
    // ST_XLAT: translate colors through the lookaside (fade) table
    //

    uint8_t *decoded = decode_shape_data(h);

    if (decoded == NULL)
        return;

    uint8_t *xlat = ((flags & ST_XLAT) && shape_lookaside_table != NULL)
                        ? shape_lookaside_table
                        : NULL;

    int32_t width = h->xmax - h->xmin;
    int32_t height = h->ymax - h->ymin;
    int32_t stride = h->xmax;

    WINDOW *w = pane->window;

    if (w != NULL && w->buffer != NULL)
    {
        //
        // Destination size = source size * scale (16:16); each
        // destination pixel samples source (dst * 0x10000 / scale)
        //

        int32_t dst_width = (int32_t)(((int64_t)width * x_scale) >> 16);
        int32_t dst_height = (int32_t)(((int64_t)height * y_scale) >> 16);

        //
        // pos_x/pos_y are pane-local (see shape_draw_rle)
        //

        int32_t base_x = pane->x0 + hotX;
        int32_t base_y = pane->y0 + hotY;

        for (int32_t row = 0; row < dst_height; row++)
        {
            int32_t src_y = h->ymin + (int32_t)(((int64_t)row * 0x10000) / y_scale);

            int32_t dy = base_y + h->ymin + row;

            if (dy < 0 || dy > w->y_max)
                continue;

            const uint8_t *src_row = decoded + (src_y * stride);
            uint8_t *dst = w->buffer + (dy * (w->x_max + 1));

            for (int32_t col = 0; col < dst_width; col++)
            {
                int32_t dx = base_x + h->xmin + col;

                if (dx < 0 || dx > w->x_max)
                    continue;

                int32_t src_x = h->xmin + (int32_t)(((int64_t)col * 0x10000) / x_scale);

                uint8_t px = src_row[src_x];

                //
                // 255 = untouched/transparent in the intermediate
                // buffer (the original renders transparent areas as
                // color 255 and always skips them during the inverse
                // transformation pass, VFXA.ASM:3917-3919).  Skipped
                // pixels are never translated, matching the original's
                // first-pass/second-pass split.
                //

                if (px == 255)
                    continue;

                if (xlat != NULL)
                    px = xlat[px];

                dst[dx] = px;
            }
        }
    }

    free(decoded);
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
