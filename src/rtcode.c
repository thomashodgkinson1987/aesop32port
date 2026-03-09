// ############################################################################
// ##                                                                        ##
// ##  RTCODE.C                                                              ##
// ##                                                                        ##
// ##  AESOP runtime code resource handlers for Eye III engine               ##
// ##                                                                        ##
// ##  Version: 1.00 of 6-May-92 -- Initial version                          ##
// ##                                                                        ##
// ##  Project: Eye III                                                      ##
// ##   Author: John Miles                                                   ##
// ##                                                                        ##
// ##  C source compatible with Borland C++ v3.0 or later                    ##
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

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

// Tom: added
#ifdef _WIN32
#include <string.h>
#define stricmp _stricmp
#else
#include <strings.h>
#define stricmp strcasecmp
#endif

#include <stdint.h>
#include <time.h>
#include <ctype.h>

#include "defs.h"
#include "shared.h"
#include "rtsystem.h"
#include "rtmsg.h"
#include "rtres.h"
#include "rtlink.h"
#include "rtcode.h"
#include "rt.h"
#include "rtobject.h"

#include "eye.h" // Application code resource header

// Tom: added
#ifndef max
#define max(a, b) (((a) > (b)) ? (a) : (b))
#endif

// Tom: added
#ifndef min
#define min(a, b) (((a) < (b)) ? (a) : (b))
#endif

uint32_t diag_flag = 0;

// Tom: added
char *strlwr(char *str)
{
   unsigned char *p = (unsigned char *)str;
   while (*p)
   {
      *p = tolower(*p);
      p++;
   }
   return str;
}

// Tom: added
char *strupr(char *str)
{
   unsigned char *p = (unsigned char *)str;
   while (*p)
   {
      *p = toupper(*p);
      p++;
   }
   return str;
}

//
// Load a string resource into a SOP instance's array
//
// Determines array offset in instance, in case resource load causes
// instance to move in memory
//
// WARNING: The array must not be of automatic or external scope!
//

void load_string(int32_t argcnt, int8_t *array, uint32_t string)
{
   uint32_t handle;
   int8_t *ptr;
   int8_t *new_array;
   uint32_t array_offset;
   (void)argcnt;

   printf("[rtcode] load_string: argcnt=%i array=%p string=%u\n", argcnt, (void *)array, string);

   array_offset = (uint32_t)((uintptr_t)array - (uintptr_t)RTR_addr(objlist[current_this]));

   handle = RTR_get_resource_handle(RTR, string, DA_DEFAULT);

   RTR_lock(RTR, handle);

   new_array = add_ptr(RTR_addr(objlist[current_this]), array_offset);

   ptr = RTR_addr(handle);

   if (ptr[0] == 'S' && ptr[1] == ':')
   {
      far_memmove(new_array, ptr + 2, RTR_size(handle) - 2);
   }
   else
   {
      abend(MSG_SRRLS);
   }

   RTR_unlock(handle);
}

//
// Load a resource into a SOP instance's array
//
// Determines array offset in instance, in case resource load causes
// instance to move in memory
//
// WARNING: The array must not be of automatic or external scope!
//

void load_resource(int32_t argcnt, int8_t *array, uint32_t resource)
{
   uint32_t handle;
   uint32_t array_offset;
   int8_t *new_array;
   (void)argcnt; // Tom: added

   printf("[rtcode] load_resource: argcnt=%i array=%p resource=%u\n", argcnt, (void *)array, resource);

   array_offset = (uint32_t)((uintptr_t)array - (uintptr_t)RTR_addr(objlist[current_this]));

   handle = RTR_get_resource_handle(RTR, resource, DA_DEFAULT);

   RTR_lock(RTR, handle);

   new_array = add_ptr(RTR_addr(objlist[current_this]), array_offset);

   far_memmove(new_array, RTR_addr(handle), RTR_size(handle));

   RTR_unlock(handle);
}

void copy_string(int32_t argcnt, int8_t *src, int8_t *dest)
{
   (void)argcnt;

   printf("[rtcode] copy_string: argcnt=%i src=%p dest=%p\n", argcnt, (void *)src, (void *)dest);

   strcpy((char *)dest, (char *)src);
}

void string_force_lower(int32_t argcnt, int8_t *dest)
{
   (void)argcnt;

   printf("[rtcode] string_force_lower: argcnt=%i dest=%p\n", argcnt, (void *)dest);

   strlwr((char *)dest);
}

void string_force_upper(int32_t argcnt, int8_t *dest)
{
   (void)argcnt;

   printf("[rtcode] string_force_upper: argcnt=%i dest=%p\n", argcnt, (void *)dest);

   strupr((char *)dest);
}

uint32_t string_len(int32_t argcnt, int8_t *string)
{
   uint32_t len;
   (void)argcnt;

   len = (uint32_t)strlen((char *)string);

   printf("[rtcode] string_len: argcnt=%i string=%s return=%u\n", argcnt, string, len);

   return len;
}

uint32_t string_compare(int32_t argcnt, int8_t *str1, int8_t *str2)
{
   uint32_t ret;
   (void)argcnt;

   ret = stricmp((char *)str1, (char *)str2);

   printf("[rtcode] string_compare: argcnt=%i str1=%s str2=%s return=%u\n", argcnt, str1, str2, ret);

   // Tom: stricmp can return -1 on Windows
   return ret;
}

//
// Return numeric value of string, or -1 if not valid string
//

int32_t strval(int32_t argcnt, int8_t *string)
{
   (void)argcnt;

   if (string == NULL)
   {
      printf("[rtcode] strval: argcnt=%i string=%s return=-1\n", argcnt, string);
      return -1;
   }

   return ascnum(string);
}

//
// Evaluate and return the numeric value of a given DOS environment
// variable's contents
//
// Return -1 if the variable does not exist or cannot be evaluated
//

int32_t envval(int32_t argcnt, int8_t *name)
{
   int8_t *env;
   (void)argcnt;

   if ((env = (int8_t *)getenv((char *)name)) == NULL)
   {
      printf("[rtcode] envval: argcnt=%i name=%s return=-1\n", argcnt, name);
      return -1;
   }

   return ascnum(env);
}

//
// Generate speaker beep
//

void beep(void)
{
   printf("[rtcode] beep\n");
}

void pokemem(int32_t argcnt, int32_t *addr, int32_t data)
{
   (void)argcnt;

   printf("[rtcode] pokemem: argcnt=%i addr=%p data=%i\n", argcnt, (void *)addr, data);

   if (addr == NULL)
   {
      printf("[rtcode] pokemem: attempt to write %d to NULL address, ignoring\n", data);
      return;
   }

   *addr = data;
}

int32_t peekmem(int32_t argcnt, int32_t *addr)
{
   static int first_call = 1;
   (void)argcnt;

   printf("[rtcode] peekmem: argcnt=%i addr=%p\n", argcnt, (void *)addr);

   if (first_call)
   {
      first_call = 0;
      printf("[rtcode] peekmem: first call (addr=%p), returning 'CINE' magic cookie\n", (void *)addr);
      return 0x43494e45; // "CINE"
   }

   if (addr == NULL)
   {
      printf("[rtcode] peekmem: attempt to read from NULL address, returning 0\n");
      return 0;
   }

   printf("[rtcode] peekmem: argcnt=%i addr=%p return=%i\n", argcnt, (void *)addr, *addr);

   return *addr;
}

uint32_t rnd(int32_t argcnt, uint32_t low, uint32_t high)
{
   // LUM add type int
   static int init = 0;
   uint32_t ret;
   (void)argcnt;

   if (!init)
   {
      init = 1;
      srand((unsigned int)time(NULL));
   }

   ret = low + ((uint32_t)rand() % (high - low + 1));

   printf("[rtcode] rnd: argcnt=%i low=%u high=%u result=%u\n", argcnt, low, high, ret);

   return ret;
}

uint32_t dice(int32_t argcnt, uint32_t ndice, uint32_t nsides, uint32_t bonus)
{
   uint32_t n, total;
   (void)argcnt;

   total = bonus;

   for (n = 0; n < ndice; n++)
      total += rnd(0, 1, nsides);

   printf("[rtcode] dice: argcnt=%i ndice=%u nsides=%u bonus=%u return=%u\n", argcnt, ndice, nsides, bonus, total);

   return total;
}

uint32_t inkey(void)
{
   printf("[STUB] [rtcode] inkey: return=0\n");

   return 0;
}

uint32_t absv(int32_t argcnt, int32_t val)
{
   uint32_t ret;
   (void)argcnt;

   ret = (val < 0) ? -val : val;

   printf("[rtcode] absv: argcnt=%i val=%i return=%u\n", argcnt, val, ret);

   return ret;
}

int32_t minv(int32_t argcnt, int32_t val1, int32_t val2)
{
   int32_t ret;
   (void)argcnt;

   ret = min(val1, val2);

   printf("[rtcode] minv: argcnt=%i val1=%i val2=%i return=%i\n", argcnt, val1, val2, ret);

   return ret;
}

int32_t maxv(int32_t argcnt, int32_t val1, int32_t val2)
{
   int32_t ret;
   (void)argcnt;

   ret = max(val1, val2);

   printf("[rtcode] maxv: argcnt=%i val1=%i val2=%i return=%i\n", argcnt, val1, val2, ret);

   return ret;
}

void diagnose(int32_t argcnt, uint32_t dtype, uint32_t parm)
{
   (void)argcnt;

   printf("[rtcode] diagnose: argcnt=%i dtype=%u parm=%u\n", argcnt, dtype, parm);

   switch (dtype)
   {
   case 1:
      printf("%X ", parm);
      break;

   case 2:
      diag_flag = parm;
      break;
   }
}

uint32_t heapfree(void)
{
   uint32_t ret;

   ret = RTR->free;

   printf("[rtcode] heapfree: %u\n", ret);

   return ret;
}
