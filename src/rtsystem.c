// ############################################################################
// ##                                                                        ##
// ##  RTSYSTEM.C                                                            ##
// ##                                                                        ##
// ##  AESOP runtime system-level services                                   ##
// ##                                                                        ##
// ##  Version: 1.00 of 6-May-92 -- Initial version                          ##
// ##                                                                        ##
// ##  Project: Extensible State-Object Processor (AESOP/16)                 ##
// ##   Author: John Miles                                                   ##
// ##                                                                        ##
// ##  C source compatible with IBM PC ANSI C/C++ implementations            ##
// ##  Large memory model (16-bit DOS)                                       ##
// ##                                                                        ##
// ############################################################################
// ##                                                                        ##
// ##  Copyright (C) 1992 Miles Design, Inc.                                 ##
// ##                                                                        ##
// ##  Miles Design, Inc.                                                    ##
// ##  10926 Jollyville #308                                                 ##
// ##  Austin, TX 78759                                                      ##
// ##  (512) 345-2642 / BBS (512) 454-9990 / FAX (512) 338-9630              ##
// ##                                                                        ##
// ############################################################################

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include <termios.h>
#include <signal.h>
#include <fcntl.h>

#include "defs.h"
#include "shared.h"
#include "rtsystem.h"
#include "rtmsg.h"
#include "rtres.h"
#include "rt.h"
#include "rtcode.h"
#include "intrface.h"
#include "event.h"
#include "sound.h"
#include "graphics.h"

uint32_t headroom;
uint32_t checksum;
uint32_t init;

int16_t disk_err;

void breakpoint(void)
{
   printf("[rtsystem] breakpoint\n");

#if defined(__linux__) || defined(__APPLE__)
   raise(SIGTRAP);
#elif defined(_WIN32)
   __debugbreak();
#endif
}

int32_t filelength(int16_t handle)
{
   struct stat file_info;

   if (fstat(handle, &file_info) == -1)
   {
      return -1;
   }

   int32_t filelength = (int32_t)file_info.st_size;

   printf("[rtsystem] filelength: handle=%i filelength=%i\n", handle, filelength);

   return filelength;
}

void mem_init(void)
{
   headroom = init = mem_avail();
   checksum = 0;

   printf("[rtsystem] mem_init: headroom=%u init=%u checksum=%u\n", headroom, init, checksum);
}

void mem_shutdown(void)
{
   uint32_t end;

   end = mem_avail();

   printf("[rtsystem] mem_shutdown: end=%u\n", end);

   if ((init != end) || (checksum != 0))
   {
      // abend(MSG_UH); (unbalanced heap normal in flat-model version)
   }
}

uint32_t mem_avail(void)
{
   uint32_t mem_avail;

   mem_avail = 16 * 1024 * 1024;

   printf("[rtsystem] mem_avail: %u\n", mem_avail);

   return mem_avail;
}

uint32_t mem_headroom(void)
{
   printf("[rtsystem] mem_headroom: %u\n", headroom);

   return headroom;
}

void *mem_alloc(uint32_t bytes)
{
   uint32_t left;
   void *ptr;

   ptr = (void *)malloc(bytes);

   left = mem_avail();
   if (left < headroom)
      headroom = left;

   if (ptr == NULL)
      abend(MSG_OODM);

   checksum ^= (uint32_t)ptr;

   printf("[rtsystem] mem_alloc: bytes=%u left=%u headroom=%u checksum=%u return=%p\n", bytes, left, headroom, checksum, ptr);

   return ptr;
}

int8_t *str_alloc(int8_t *str)
{
   int8_t *ptr;

   ptr = mem_alloc(strlen((char *)str) + 1);
   strcpy((char *)ptr, (char *)str);

   printf("[rtsystem] str_alloc: str=%s ptr=%p\n", str, (void *)ptr);

   return ptr;
}

void mem_free(void *ptr)
{
   checksum ^= (uint32_t)ptr;

   printf("[rtsystem] mem_free: ptr=%p checksum=%u\n", ptr, checksum);

   free(ptr);
}

/***************************************************/
//
// Convert string to number, returning -1 if not valid numeric
// string
//
// Skip leading whitespace; handles unary -/+ operators
//
// Accepts binary numbers with '0b' prefix,
// hexadecimal numbers with '0x' prefix; decimal numbers
// handled via atol() library function for speed
//
// Accepts single ASCII characters with '\'' prefix
//
/***************************************************/

int32_t ascnum(int8_t *string)
{
   int32_t i, j, len, base, neg, chr;
   int32_t total;

   while (isspace(*string))
      string++;

   neg = 0;
   switch (*string)
   {
   case '-':
      neg = 1;
      string++;
      break;
   case '+':
      string++;
      break;
   }

   if (*string == '\'')
      return (int32_t)(*(string + 1));

   if (string[0] == '0' && string[1] == 'x')
   {
      base = 16;
      string += 2;
   }
   else if (string[0] == '0' && string[1] == 'b')
   {
      base = 2;
      string += 2;
   }
   else
   {
      base = 10;
   }

   if (base == 10 && isdigit(*string))
      return neg ? -atol((char *)string) : atol((char *)string);
   else
      return -1;

   total = 0;
   len = strlen((char *)string);

   for (i = 0; i < len; i++)
   {
      chr = toupper(string[i]);

      for (j = 0; j < base; j++)
         if (chr == "0123456789ABCDEF"[j])
         {
            total = (total * base) + j;
            break;
         }

      if (j == base)
         return -1;
   }

   printf("[rtsystem] ascnum: string=%s total=%i\n", string, total);

   return total;
}

/***************************************************/
//
// AESOP interpreter opcode fault handler
//
/***************************************************/

void opcode_fault(void *PC, void *stk)
{
   printf("[rtsystem] opcode_fault: PC=%p stk=%p\n", PC, stk);

   abend(MSG_IAO, *(unsigned char *)PC, PC, stk);
}

/***************************************************/
//
// Abnormal program termination handler
//
// Give debugger a chance to return to the failing function, else
// shut everything down gracefully and exit to DOS
//
// LUM 070203: added support for storing the error into a file
/***************************************************/

void abend(char *msg, ...)
{
   va_list argptr;
   int16_t recover;
   int16_t x, y;
   char loErrorBuffer[1000 + 1]; // max length + 1

   printf("[rtsystem] abend(char *msg, ...): msg=%s\n", msg);

   curpos(&x, &y);
   if (y > 25)
      locate(0, 0);

   if (msg != NULL)
   {
      FILE *loErrorFile;
      printf("Error: ");

      va_start(argptr, msg);
      vsnprintf(loErrorBuffer, 1000, msg, argptr);
      loErrorBuffer[1000] = '\0'; // just to be sure there is a terminator even if limit is filled
      va_end(argptr);
      printf("%s\n", loErrorBuffer);

      // attempt to make an error file
      loErrorFile = fopen("aesop_e.dbg", "wt");
      if (loErrorFile != NULL)
      {
         // write error
         fprintf(loErrorFile, "The AESOP/32 engine terminated with the error:\n");
         fprintf(loErrorFile, "%s\n", loErrorBuffer);
         if (envval(0, (int8_t *)"AESOP_DIAG") == 1)
         {
            fprintf(loErrorFile, MSG_MIE, current_msg, current_index, current_event_type);
         }
         fclose(loErrorFile);
      }

      if (envval(0, (int8_t *)"AESOP_DIAG") == 1)
      {
         printf(MSG_MIE, current_msg, current_index, current_event_type);
      }
   }

   recover = 0;

   breakpoint();

   if (!recover)
   {
      shutdown_sound();
      shutdown_interface();
      // AIL_shutdown("Abend"); // Tom: commented out
      // GIL2VFX_shutdown_driver(); // Tom: commented out

      exit(1);
   }
}

/***************************************************/
//
// Open a text file for reading/writing
//
/***************************************************/

TF_class *TF_construct(int8_t *filename, int16_t oflag)
{
   TF_class *TF;
   int16_t file;
   uint32_t hbuf;

   if (oflag == TF_WRITE)
      oflag = O_CREAT | O_TRUNC | O_WRONLY;
   else
      oflag = O_RDONLY;

   file = open((char *)filename, oflag | O_BINARY, S_IREAD | S_IWRITE);
   if (file == -1)
      return NULL;

   hbuf = RTR_alloc(RTR, TF_BUFSIZE, DA_FIXED | DA_PRECIOUS);
   if (hbuf == -1U)
      return NULL;

   TF = mem_alloc(sizeof(TF_class));

   TF->file = file;
   TF->hbuf = hbuf;
   TF->buffer = RTR_addr(TF->hbuf);
   TF->p = 0;
   TF->mode = oflag;
   TF->len = filelength(file);
   TF->pos = 0;

   if (!(oflag & O_WRONLY))
      read(TF->file, TF->buffer, TF_BUFSIZE);

   printf("[rtsystem] TF_construct: filename=%s oflag=%i return=%p\n", filename, oflag, (void *)TF);

   return TF;
}

/***************************************************/
//
// Close text file/dealloc buffer
//
// Return 0 if write attempt failed
//
/***************************************************/

int16_t TF_destroy(TF_class *TF)
{
   int16_t e, f;

   printf("[rtsystem] TF_destroy: TF=%p\n", (void *)TF);

   e = f = TF->p;

   if ((TF->mode & O_WRONLY) && (TF->p != 0))
      e = write(TF->file, TF->buffer, TF->p);

   close(TF->file);

   RTR_free(RTR, TF->hbuf);
   mem_free(TF);

   printf("[rtsystem] TF_destroy: return=%i\n", (e == f));

   return (e == f);
}

/***************************************************/
//
// Write character to text file
//
// Return 0 if write attempt failed
//
/***************************************************/

int16_t TF_wchar(TF_class *TF, int8_t ch)
{
   printf("[rtsystem] TF_wchar: TF=%p ch=%i\n", (void *)TF, ch);

   TF->buffer[TF->p++] = ch;

   if (TF->p == TF_BUFSIZE)
   {
      TF->p = 0;
      if (write(TF->file, TF->buffer, TF_BUFSIZE) != TF_BUFSIZE)
      {
         return 0;
      }
   }

   return 1;
}

/***************************************************/
//
// Read character from text file
//
// Return 0 if EOF reached
//
/***************************************************/

int8_t TF_rchar(TF_class *TF)
{
   if (TF->pos >= TF->len)
   {
      return 0;
   }

   ++TF->pos;

   if (TF->p != TF_BUFSIZE)
   {
      return TF->buffer[TF->p++];
   }

   read(TF->file, TF->buffer, TF_BUFSIZE);

   TF->p = 1;

   printf("[rtsystem] TF_rchar: TF=%p return=%i\n", (void *)TF, TF->buffer[0]);

   return TF->buffer[0];
}

/***************************************************/
//
// Read text file line into buffer
//
// \r's are skipped
// \n's are truncated, replaced with \0
//
// Blank lines are ignored
//
// Return 0 if EOF reached
//
/***************************************************/

int16_t TF_readln(TF_class *TF, int8_t *buffer, int16_t maxlen)
{
   int16_t b, c;

   printf("[rtsystem] TF_readln: TF=%p buffer=%p maxlen=%i\n", (void *)TF, (void *)buffer, maxlen);

   do
   {
      b = 0;

      while (b != maxlen - 1)
      {
         c = TF_rchar(TF);

         if (c == '\n')
            break;
         if (c == '\r')
            continue;

         buffer[b++] = c;

         if (!c)
         {
            return 0;
         }
      }

      if (b == maxlen - 1)
         while ((c = TF_rchar(TF)) != '\n')
            if (!c)
            {
               return 0;
            }

      buffer[b] = 0;
   } while (!strlen((char *)buffer));

   return 1;
}

/***************************************************/
//
// Write buffer line to text file
// Return 0 if write attempt failed
//
// \r\n added at end of each buffer line
//
/***************************************************/

int16_t TF_writeln(TF_class *TF, int8_t *buffer)
{
   int16_t b, c;

   printf("[rtsystem] TF_writeln: TF=%p buffer=%p\n", (void *)TF, (void *)buffer);

   b = 0;

   while ((c = buffer[b++]) != 0)
      if (!TF_wchar(TF, c))
      {
         return 0;
      }

   TF_wchar(TF, '\r');

   return TF_wchar(TF, '\n');
}

/***************************************************/
//
// Delete a file
//
// Return 0 if file did not exist, -1 if deletion failed,
// else 1 if deleted OK
//
/***************************************************/

int16_t delete_file(int8_t *filename)
{
   if (!unlink((char *)filename))
   {
      printf("[rtsystem] delete_file: filename=%s return=1\n", filename);
      return 1;
   }

   if (errno == ENOENT)
   {
      printf("[rtsystem] delete_file: filename=%s return=0\n ", filename);
      return 0;
   }

   printf("[rtsystem] delete_file: filename=%s return=-1\n", filename);
   return -1;
}

/***************************************************/
//
// Copy a file
//
// Return 0 if source file not found, -1 if copy error occurred,
// else 1 if copied OK
//
/***************************************************/

int16_t copy_file(int8_t *src_filename, int8_t *dest_filename)
{
   uint32_t hbuf;
   int8_t *buffer;
   int16_t status;
   int16_t s, d, n;

   printf("[rtsystem] copy_file: src_filename=%s dest_filename=%s\n", src_filename, dest_filename);

   s = open((char *)src_filename, O_RDONLY | O_BINARY);

   if (s == -1)
      return 0;

   d = open((char *)dest_filename, O_BINARY | O_CREAT | O_TRUNC | O_WRONLY,
            S_IREAD | S_IWRITE);

   if (d == -1)
   {
      close(s);
      return -1;
   }

   hbuf = RTR_alloc(RTR, TF_BUFSIZE, DA_FIXED | DA_PRECIOUS);
   if (hbuf == -1U)
   {
      close(s);
      close(d);
      return -1;
   }

   buffer = RTR_addr(hbuf);
   status = 1;

   while ((n = read(s, buffer, TF_BUFSIZE)) != 0)
   {
      if (n == -1)
      {
         status = -1;
         break;
      }

      if (write(d, buffer, n) != n)
      {
         status = -1;
         break;
      }
   }

   close(s);
   close(d);

   RTR_free(RTR, hbuf);

   return status;
}

/****************************************************************************/
//
// Determine the size in bytes of a file
//
/****************************************************************************/

int32_t file_size(int8_t *filename)
{
   int16_t handle;
   int32_t len;

   disk_err = 0;

   handle = open((char *)filename, O_RDONLY | O_BINARY);
   if (handle == -1)
   {
      disk_err = FILE_NOT_FOUND;
      return -1;
   }

   len = filelength(handle);
   if (len == -1)
      disk_err = CANT_READ_FILE;

   close(handle);

   printf("[rtsystem] file_size: filename=%s return=%i\n", filename, len);

   return len;
}

/****************************************************************************/
//
// Read a file directly into memory
//
/****************************************************************************/

int8_t *read_file(int8_t *filename, void *dest)
{
   int16_t i, handle;
   uint32_t len;
   int8_t *buf, *mem;

   printf("[rtsystem] read_file: filename=%s dest=%p\n", filename, dest);

   disk_err = 0;
   len = file_size(filename);

   if (len == UINT32_MAX)
   {
      disk_err = FILE_NOT_FOUND;
      return NULL;
   }

   buf = mem = (dest == NULL) ? mem_alloc(len) : dest;

   if (buf == NULL)
   {
      disk_err = OUT_OF_MEMORY;
      return NULL;
   }

   handle = open((char *)filename, O_RDONLY | O_BINARY);
   if (handle == -1)
   {
      mem_free(mem);
      disk_err = FILE_NOT_FOUND;
      return NULL;
   }

   while (len >= DOS_BUFFSIZE)
   {
      i = read(handle, buf, DOS_BUFFSIZE);
      if (i != (int16_t)DOS_BUFFSIZE)
      {
         mem_free(mem);
         disk_err = CANT_READ_FILE;
         return NULL;
      }
      len -= DOS_BUFFSIZE;
      buf = add_ptr(buf, DOS_BUFFSIZE);
   }

   i = read(handle, buf, (uint16_t)len);
   if (i != (uint16_t)len)
   {
      mem_free(mem);
      disk_err = CANT_READ_FILE;
      return NULL;
   }

   close(handle);
   return mem;
}

/****************************************************************************/
//
// Write a binary file to disk
//
/****************************************************************************/

int16_t write_file(int8_t *filename, void *buf, uint32_t len)
{
   int16_t i, handle;

   printf("[rtsystem] write_file: filename=%s buf=%p len=%u\n", filename, buf, len);

   disk_err = 0;

   handle = open((char *)filename, O_CREAT | O_RDWR | O_TRUNC | O_BINARY, S_IREAD | S_IWRITE);
   if (handle == -1)
   {
      disk_err = CANT_WRITE_FILE;
      return 0;
   }

   while (len >= DOS_BUFFSIZE)
   {
      i = write(handle, buf, DOS_BUFFSIZE);
      if (i == -1)
      {
         disk_err = CANT_WRITE_FILE;
         return 0;
      }
      if (i != (int16_t)DOS_BUFFSIZE)
      {
         disk_err = DISK_FULL;
         return 0;
      }
      len -= DOS_BUFFSIZE;
      buf = add_ptr(buf, DOS_BUFFSIZE);
   }

   i = write(handle, buf, (uint16_t)len);
   if (i == -1)
   {
      disk_err = CANT_WRITE_FILE;
      return 0;
   }
   if (i != (uint16_t)len)
   {
      disk_err = DISK_FULL;
      return 0;
   }

   close(handle);

   return 1;
}

/****************************************************************************/
//
// Append binary data to an existing file
//
/****************************************************************************/

int16_t append_file(int8_t *filename, void *buf, uint32_t len)
{
   int16_t i, handle;

   printf("[rtsystem] append_file: filename=%s buff=%p len=%u\n", filename, buf, len);

   disk_err = 0;

   handle = open((char *)filename, O_APPEND | O_RDWR | O_BINARY);
   if (handle == -1)
   {
      disk_err = FILE_NOT_FOUND;
      return 0;
   }

   while (len >= DOS_BUFFSIZE)
   {
      i = write(handle, buf, DOS_BUFFSIZE);
      if (i == -1)
      {
         disk_err = CANT_WRITE_FILE;
         return 0;
      }
      if (i != (int16_t)DOS_BUFFSIZE)
      {
         disk_err = DISK_FULL;
         return 0;
      }
      len -= DOS_BUFFSIZE;
      buf = add_ptr(buf, DOS_BUFFSIZE);
   }

   i = write(handle, buf, (uint16_t)len);
   if (i == -1)
   {
      disk_err = CANT_WRITE_FILE;
      return 0;
   }
   if (i != (uint16_t)len)
   {
      disk_err = DISK_FULL;
      return 0;
   }

   close(handle);

   return 1;
}

// Tom: new comment
/****************************************************************************/
//
// Get file's timestamp (Linux Port)
// Returns: High 16-bits = DOS Date, Low 16-bits = DOS Time
//
/****************************************************************************/

uint32_t file_time(int8_t *filename)
{
   struct stat file_info;
   uint32_t ret;

   if (stat((char *)filename, &file_info) == -1)
   {
      printf("[rtsystem] file_time: filename=%s return=0\n", filename);
      return 0;
   }

   // Convert Linux timestamp to a usable date/time structure
   struct tm *t = localtime(&file_info.st_mtime);

   // --- PACKING THE DOS DATE ---
   // DOS Date Format (16 bits):
   // Year (from 1980) = 7 bits, Month (1-12) = 4 bits, Day (1-31) = 5 bits
   int16_t dos_year = t->tm_year - 80;
   if (dos_year < 0)
      dos_year = 0; // DOS doesn't support years before 1980

   uint16_t dos_date = (dos_year << 9) | ((t->tm_mon + 1) << 5) | t->tm_mday;

   // --- PACKING THE DOS TIME ---
   // DOS Time Format (16 bits):
   // Hour (0-23) = 5 bits, Minute (0-59) = 6 bits, Seconds/2 (0-29) = 5 bits
   uint16_t dos_time = (t->tm_hour << 11) | (t->tm_min << 5) | (t->tm_sec / 2);

   ret = ((uint32_t)dos_date << 16) | dos_time;

   printf("[rtsystem] file_time: filename=%s return=%u\n", filename, ret);

   // Original code returned DX (date) shifted left by 16, plus CX (time)
   return ret;
}

/****************************************************************************/
//
// Position text cursor
//
/****************************************************************************/

void locate(int16_t x, int16_t y)
{
   (void)x;
   (void)y;

   printf("[STUB] [rtsystem] locate: x=%i y=%i\n", x, y);
}

/****************************************************************************/
//
// Get text cursor location
//
/****************************************************************************/

void curpos(int16_t *x, int16_t *y)
{
   *x = 0;
   *y = 0;

   printf("[STUB] [rtsystem] curpos: x=%i y=%i\n", *x, *y);
}
