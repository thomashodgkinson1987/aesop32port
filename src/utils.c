#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "utils.h"
#include "globals.h"

#include "defs.h"
#include "vfx.h"

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

void save_buffer_to_pgm(
    const uint8_t *buffer,
    int32_t width,
    int32_t height,
    const char *filename)
{
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

void save_shape_to_pgm_from_table_and_number(void *table, int32_t shape_number, const char *filename)
{
   uint32_t *shape_offsets = get_shape_offsets(table);
   void *shape = table + shape_offsets[shape_number];

   save_shape_to_pgm_from_shape(shape, filename);

   free(shape_offsets);
}

void save_shape_to_pgm_from_shape(void *shape, const char *filename)
{
   SHAPEHEADER *shape_header = (SHAPEHEADER *)shape;
   int32_t width = shape_header->xmax;
   int32_t height = shape_header->ymax;
   uint8_t *buffer = decode_shape_data(shape);

   save_buffer_to_pgm(buffer, width, height, filename);

   free(buffer);
}

void save_shape_to_ppm_from_shape(void *shape, const char *filename)
{
   SHAPEHEADER *shape_header = (SHAPEHEADER *)shape;
   int32_t width = shape_header->xmax;
   int32_t height = shape_header->ymax;
   uint8_t *buffer = decode_shape_data(shape);

   save_buffer_to_ppm(buffer, width, height, filename);

   free(buffer);
}

uint8_t *decode_shape_data(void *shape)
{
   SHAPEHEADER *shape_header = (SHAPEHEADER *)shape;
   uint8_t *shape_data = (uint8_t *)shape + sizeof(SHAPEHEADER);

   uint8_t *buffer = (uint8_t *)calloc(320 * 200, sizeof(uint8_t));
   if (!buffer)
      exit(EXIT_FAILURE);

   int32_t CP_W = shape_header->xmax;

   int linecount = shape_header->ymax + 1 - shape_header->ymin;
   if (linecount <= 0)
      return NULL;

   int startX = shape_header->xmin;
   int startY = shape_header->ymin;
   uint8_t *dest_line_ptr = buffer + (startY * CP_W) + startX;

   while (linecount > 0)
   {
      uint8_t *dest_ptr = dest_line_ptr;

      while (1)
      {
         uint8_t token = *shape_data++;

         int type = token & 1;
         int count = token >> 1;

         if (count > 0 && type == 0)
         {
            uint8_t color = *shape_data++;
            memset(dest_ptr, color, count);
            dest_ptr += count;
         }
         else if (count > 0 && type == 1)
         {
            memcpy(dest_ptr, shape_data, count);
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

uint32_t *get_shape_offsets(void *shape_table)
{
   SHAPETABLEHEADER *shape_table_header = (SHAPETABLEHEADER *)shape_table;

   uint32_t *shape_offsets = (uint32_t *)calloc(
       shape_table_header->shape_count,
       sizeof(uint32_t));

   if (!shape_offsets)
      exit(EXIT_FAILURE);

   uint64_t *shape_offsets_start = (uint64_t *)((uint8_t *)shape_table + sizeof(SHAPETABLEHEADER));

   // printf("---- SHAPEOFFSETS ----\n");
   for (int32_t i = 0; i < shape_table_header->shape_count; ++i)
   {
      shape_offsets[i] = shape_offsets_start[i];
      // printf("shape_offsets[%i]=%x\n", i, shape_offsets[i]);
   }
   // printf("----------------------\n");

   return shape_offsets;
}

void debug_shape_table(void *shape_table)
{
   SHAPETABLEHEADER *shape_table_header = (SHAPETABLEHEADER *)shape_table;

   // print_shape_table_header(shape_table_header);

   if (shape_table_header->shape_count == 0)
      return;

   uint32_t *shape_offsets = get_shape_offsets(shape_table);

   for (uint32_t i = 0; i < shape_table_header->shape_count; ++i)
   {
      void *shape = (uint8_t *)shape_table + shape_offsets[i];

      // print_shape_header(shape);

      static uint32_t output_count = 1;
      char filename[256];
      snprintf(filename, sizeof(filename), "../misc-files/extracted/image_%04u.pgm", output_count++);
      save_shape_to_ppm_from_shape(shape, filename);
   }

   free(shape_offsets);
}

void debug_draw_shape(void *shape_table, int32_t shape_number, int32_t hotX, int32_t hotY)
{
   SHAPETABLEHEADER *shape_table_header = (SHAPETABLEHEADER *)shape_table;

   if (shape_table_header->shape_count == 0)
      return;

   uint32_t *shape_offsets = get_shape_offsets(shape_table);
   void *shape = (uint8_t *)shape_table + shape_offsets[shape_number];
   SHAPEHEADER *shape_header = (SHAPEHEADER *)shape;
   uint8_t *shape_data = decode_shape_data(shape);

   void *pixels;
   int pitch;
   if (SDL_LockTexture(sdl_texture, NULL, &pixels, &pitch) == 0)
   {
      uint32_t *pixels_ptr = (uint32_t *)pixels;
      for (int32_t y = 0; y < shape_header->ymax; ++y)
      {
         for (int32_t x = 0; x < shape_header->xmax; ++x)
         {
            uint8_t decoded_pixel = shape_data[y * shape_header->xmax + x];
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

   free(shape_data);
   free(shape_offsets);
}

void print_shape_table_header(void *shape_table)
{
   SHAPETABLEHEADER *shape_table_header = (SHAPETABLEHEADER *)shape_table;

   printf("---- SHAPETABLEHEADER ----\n");
   printf("shape_table_header=%p\n", (void *)shape_table_header);
   printf("version=%x %x %x %x\n", shape_table_header->version_byte_1, shape_table_header->version_byte_2, shape_table_header->version_byte_3, shape_table_header->version_byte_4);
   printf("shape_count=%i\n", shape_table_header->shape_count);
   printf("--------------------------\n");
}

void print_shape_header(void *shape)
{
   SHAPEHEADER *shape_header = (SHAPEHEADER *)shape;

   printf("---- SHAPEHEADER ----\n");
   printf("bounds: %x\n", shape_header->bounds);
   printf("origin: %x\n", shape_header->origin);
   printf("xmin: %i\n", shape_header->xmin);
   printf("ymin: %i\n", shape_header->ymin);
   printf("xmax: %i\n", shape_header->xmax);
   printf("ymax: %i\n", shape_header->ymax);
   printf("---------------------\n");
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
