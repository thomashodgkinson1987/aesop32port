#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "utils.h"

#include "globals.h"

#include "defs.h" // for PAL_HDR
#include "vfx.h"  // for RGB

AESOP_Palette test_palette;

// Tom: drop-in replacement for DOS ltoa()
char *ltoa(long val, char *buffer, int radix)
{
   if (radix == 10)
   {
      sprintf(buffer, "%ld", val);
   }
   else if (radix == 16)
   {
      sprintf(buffer, "%lx", val);
   }
   else if (radix == 8)
   {
      sprintf(buffer, "%lo", val);
   }
   else
   {
      // Fallback for weird bases (like base 2)
      sprintf(buffer, "%ld", val);
   }
   return buffer;
}

// Tom: drop-in replacement for DOS ultoa()
char *ultoa(unsigned long val, char *buffer, int radix)
{
   if (radix == 10)
   {
      sprintf(buffer, "%ld", val);
   }
   else if (radix == 16)
   {
      sprintf(buffer, "%lx", val);
   }
   else if (radix == 8)
   {
      sprintf(buffer, "%lo", val);
   }
   else
   {
      // Fallback for weird bases (like base 2)
      sprintf(buffer, "%ld", val);
   }
   return buffer;
}

void save_buffer_to_ppm(
    const uint8_t *buffer,
    int32_t width,
    int32_t height,
    const char *filename)
{
   FILE *out = fopen(filename, "wb");
   if (!out)
      return;

   // P6 is binary PPM
   fprintf(out, "P6\n%d %d\n255\n", width, height);

   for (int32_t i = 0; i < width * height; i++)
   {
      uint8_t idx = buffer[i];
      fwrite(&test_palette.colors[idx], 1, 3, out);
   }

   fclose(out);
}

void save_shape_to_ppm_from_shape(const void *shape, const char *filename)
{
   int32_t width = shape_get_xmax(shape);
   int32_t height = shape_get_ymax(shape);
   uint8_t *decoded_shape_data = decode_shape_data(shape);

   save_buffer_to_ppm(decoded_shape_data, width, height, filename);

   free(decoded_shape_data);
}

uint8_t *decode_shape_data(const void *shape)
{
   uint8_t *shape_data = shape_get_data(shape);

   uint8_t *buffer = (uint8_t *)calloc(320 * 200, 1);
   if (!buffer)
      exit(EXIT_FAILURE);

   // clear to white to save black pixels
   memset(buffer, 255, 320 * 200);

   int32_t CP_W = shape_get_xmax(shape);

   int linecount = shape_get_ymax(shape) + 1 - shape_get_ymin(shape);
   if (linecount <= 0)
      return NULL;

   int startX = shape_get_xmin(shape);
   int startY = shape_get_ymin(shape);
   uint8_t *dest_line_ptr = buffer + (startY * CP_W) + startX;

   while (linecount > 0)
   {
      uint8_t *dest_ptr = dest_line_ptr;

      while (1)
      {
         uint8_t token = *shape_data++;

         int32_t type = token & 1;
         int32_t count = token >> 1;

         if (count > 0 && type == 0)
         {
            uint8_t color = *shape_data++;
            memset(dest_ptr, color, (size_t)count);
            dest_ptr += count;
         }
         else if (count > 0 && type == 1)
         {
            memcpy(dest_ptr, shape_data, (size_t)count);
            dest_ptr += count;
            shape_data += count;
         }
         else if (count == 0 && type == 1)
         {
            int skip_count = *shape_data++;
            dest_ptr += skip_count;
         }
         else if (count == 0 && type == 0)
         {
            break;
         }
      }
      dest_line_ptr += CP_W;
      linecount--;
   }

   return buffer;
}

void debug_shape_table(const void *shape_table)
{
   if (shape_table_get_shape_count(shape_table) == 0)
      return;

   static uint32_t output_count = 0;

   for (int32_t i = 0; i < shape_table_get_shape_count(shape_table); ++i)
   {
      const void *shape = shape_table_get_shape(shape_table, i);

      char filename[256];
      snprintf(filename, sizeof(filename), "../misc-files/extracted/image_%02u_%04i.pgm", output_count, i);
      save_shape_to_ppm_from_shape(shape, filename);
   }

   output_count++;
}

void debug_draw_shape(const void *shape_table, int32_t shape_number, int32_t hotX, int32_t hotY)
{
   (void)hotX;
   (void)hotY;

   if (shape_table_get_shape_count(shape_table) == 0)
      return;

   const void *shape = shape_table_get_shape(shape_table, shape_number);
   uint8_t *decoded_shape_data = decode_shape_data(shape);

   void *pixels;
   int pitch;
   if (SDL_LockTexture(sdl_texture, NULL, &pixels, &pitch) == 0)
   {
      uint32_t *pixels_ptr = (uint32_t *)pixels;
      for (int32_t y = 0; y < shape_get_ymax(shape); ++y)
      {
         for (int32_t x = 0; x < shape_get_xmax(shape); ++x)
         {
            uint8_t decoded_pixel = decoded_shape_data[y * shape_get_xmax(shape) + x];
            if (decoded_pixel == 0)
            {
               pixels_ptr[y * (pitch / 4) + x] = 0;
            }
            else
            {
               RGB rgb = test_palette.colors[decoded_pixel];
               pixels_ptr[y * (pitch / 4) + x] = (255 << 24) | (rgb.r << 16) | (rgb.g << 8) | rgb.b;
            }
         }
      }
      SDL_UnlockTexture(sdl_texture);
   }

   free(decoded_shape_data);
}

void print_shape_table_header(const void *shape_table)
{
   printf("version: %04x\n", shape_table_get_version(shape_table));
   printf("shape_count: %i\n", shape_table_get_shape_count(shape_table));
}

void print_shape_header(const void *shape)
{
   printf("bounds: %ix%i\n", shape_get_bounds_x(shape), shape_get_bounds_y(shape));
   printf("origin: %ix%i\n", shape_get_origin_x(shape), shape_get_origin_y(shape));
   printf("xmin: %i\n", shape_get_xmin(shape));
   printf("ymin: %i\n", shape_get_ymin(shape));
   printf("xmax: %i\n", shape_get_xmax(shape));
   printf("ymax: %i\n", shape_get_ymax(shape));
}

void print_decoded_shape_data(const void *shape)
{
   uint8_t *shape_data = decode_shape_data(shape);

   int32_t width = shape_get_xmax(shape);
   int32_t height = shape_get_ymax(shape);

   for (int32_t y = 0; y < height; ++y)
   {
      for (int32_t x = 0; x < width; ++x)
      {
         printf(" %02x" + (x == 0), shape_data[y * width + x]);
      }
      printf("\n");
   }

   free(shape_data);
}

int32_t shape_table_get_version(const void *shape_table)
{
   return ((SHAPETABLEHEADER *)shape_table)->version;
}

int32_t shape_table_get_shape_count(const void *shape_table)
{
   return ((SHAPETABLEHEADER *)shape_table)->shape_count;
}

uint64_t *shape_table_get_offsets(const void *shape_table)
{
   return (uint64_t *)((uint8_t *)shape_table + sizeof(SHAPETABLEHEADER));
}

uint32_t *shape_table_get_offsets_as_uint32(const void *shape_table)
{
   uint32_t *shape_offsets = (uint32_t *)calloc(
       (size_t)shape_table_get_shape_count(shape_table),
       sizeof(uint32_t));

   if (!shape_offsets)
      exit(EXIT_FAILURE);

   uint64_t *shape_offsets_start = shape_table_get_offsets(shape_table);

   for (int32_t i = 0; i < shape_table_get_shape_count(shape_table); ++i)
   {
      shape_offsets[i] = (uint32_t)(shape_offsets_start[i]);
   }

   return shape_offsets;
}

void *shape_table_get_shape(const void *shape_table, int32_t index)
{
   if (shape_table_get_shape_count(shape_table) < index)
      return NULL;

   uint64_t *shape_offsets = shape_table_get_offsets(shape_table);
   void *shape = (uint8_t *)shape_table + (uint32_t)(shape_offsets[index]);

   return shape;
}

int16_t shape_get_bounds_x(const void *shape)
{
   SHAPEHEADER *shape_header = (SHAPEHEADER *)shape;
   int32_t bounds = shape_header->bounds;
   bounds = bounds >> 16;
   int16_t trimmed_bounds = (int16_t)bounds;
   return trimmed_bounds;
}

int16_t shape_get_bounds_y(const void *shape)
{
   return (int16_t)((SHAPEHEADER *)shape)->bounds;
}

int16_t shape_get_origin_x(const void *shape)
{
   return (int16_t)(((SHAPEHEADER *)shape)->origin >> 16);
}

int16_t shape_get_origin_y(const void *shape)
{
   return (int16_t)((SHAPEHEADER *)shape)->origin;
}

int32_t shape_get_xmin(const void *shape)
{
   return ((SHAPEHEADER *)shape)->xmin;
}

int32_t shape_get_ymin(const void *shape)
{
   return ((SHAPEHEADER *)shape)->ymin;
}

int32_t shape_get_xmax(const void *shape)
{
   return ((SHAPEHEADER *)shape)->xmax;
}

int32_t shape_get_ymax(const void *shape)
{
   return ((SHAPEHEADER *)shape)->ymax;
}

uint8_t *shape_get_data(const void *shape)
{
   return (uint8_t *)shape + sizeof(SHAPEHEADER);
}

void update_palette(const PAL_HDR *PHDR, AESOP_Palette *palette)
{
   palette->ncolors = PHDR->ncolors;

   uint8_t *rgb_data = (uint8_t *)PHDR + sizeof(PAL_HDR);

   for (int i = 0; i < palette->ncolors; i++)
   {
      // Convert 6-bit VGA to 8-bit RGB
      palette->colors[i].r = (rgb_data[i * 3 + 0] << 2) | (rgb_data[i * 3 + 0] >> 4);
      palette->colors[i].g = (rgb_data[i * 3 + 1] << 2) | (rgb_data[i * 3 + 1] >> 4);
      palette->colors[i].b = (rgb_data[i * 3 + 2] << 2) | (rgb_data[i * 3 + 2] >> 4);
   }
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
   uint8_t *dest_line_ptr = (uint8_t *)buffer + (startY * CP_W) + startX;

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
            memset(dest_ptr, color, (size_t)count);
            dest_ptr += count;
         }

         else if (count > 0 && type == 1)
         {
            // STRING PACKET (Assembly: jnz StringPacket)
            // Action: Copy a sequence of 'count' unique pixels.
            // The next 'count' bytes in the file are the pixel colors.

            // (Assembly uses RMOVSB32 here to copy 32-bits at a time)
            memcpy(dest_ptr, shape_data, (size_t)count);
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
