// ����������������������������������������������������������������������������
// ��                                                                        ��
// ��  RTCODE.C                                                              ��
// ��                                                                        ��
// ��  AESOP runtime code resource handlers for Eye III engine               ��
// ��                                                                        ��
// ��  Version: 1.00 of 6-May-92 -- Initial version                          ��
// ��                                                                        ��
// ��  Project: Eye III                                                      ��
// ��   Author: John Miles                                                   ��
// ��                                                                        ��
// ��  C source compatible with Borland C++ v3.0 or later                    ��
// ��  Large memory model (16-bit DOS)                                       ��
// ��                                                                        ��
// ����������������������������������������������������������������������������
// ��                                                                        ��
// ��  Copyright (C) 1992 Miles Design, Inc.                                 ��
// ��                                                                        ��
// ��  Miles Design, Inc.                                                    ��
// ��  10926 Jollyville #308                                                 ��
// ��  Austin, TX 78759                                                      ��
// ��  (512) 345-2642 / BBS (512) 454-9990 / FAX (512) 338-9630              ��
// ��                                                                        ��
// ����������������������������������������������������������������������������

// #include <conio.h> // Tom: commented out
#include <stdio.h>
// #include <dos.h> // Tom: commented out
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <stdint.h> // Tom: added

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

uint32_t diag_flag = 0;

//
// Load a string resource into a SOP instance's array
//
// Determines array offset in instance, in case resource load causes
// instance to move in memory
//
// WARNING: The array must not be of automatic or external scope!
//

#pragma off(unreferenced)
void load_string(int32_t argcnt, int8_t *array, uint32_t string)
#pragma on(unreferenced)
{
   uint32_t handle;
   int8_t *ptr;
   int8_t *new_array;
   uint32_t array_offset;

   array_offset = (uint32_t)array - (uint32_t)RTR_addr(objlist[current_this]);

   handle = RTR_get_resource_handle(RTR, string, DA_DEFAULT);

   RTR_lock(RTR, handle);

   new_array = add_ptr(RTR_addr(objlist[current_this]), array_offset);

   ptr = RTR_addr(handle);

   switch (*(uint16_t *)ptr)
   {
   case ':S':
      far_memmove(new_array, ptr + 2, RTR_size(handle) - 2L);
      break;

   default:
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

#pragma off(unreferenced)
void load_resource(int32_t argcnt, int8_t *array, uint32_t resource)
#pragma on(unreferenced)
{
   uint32_t handle;
   uint32_t array_offset;
   int8_t *new_array;

   array_offset = FP_OFF(array) - FP_OFF(RTR_addr(objlist[current_this]));

   handle = RTR_get_resource_handle(RTR, resource, DA_DEFAULT);

   RTR_lock(RTR, handle);

   new_array = add_ptr(RTR_addr(objlist[current_this]), array_offset);

   far_memmove(new_array, RTR_addr(handle), RTR_size(handle));

   RTR_unlock(handle);
}

#pragma off(unreferenced)
void copy_string(int32_t argcnt, int8_t *src, int8_t *dest)
#pragma on(unreferenced)
{
   strcpy((char *)dest, (char *)src);
}

#pragma off(unreferenced)
void string_force_lower(int32_t argcnt, int8_t *dest)
#pragma on(unreferenced)
{
   strlwr((char *)dest);
}

#pragma off(unreferenced)
void string_force_upper(int32_t argcnt, int8_t *dest)
#pragma on(unreferenced)
{
   strupr((char *)dest);
}

#pragma off(unreferenced)
uint32_t string_len(int32_t argcnt, int8_t *string)
#pragma on(unreferenced)
{
   return strlen((char *)string);
}

#pragma off(unreferenced)
uint32_t string_compare(int32_t argcnt, int8_t *str1, int8_t *str2)
#pragma on(unreferenced)
{
   return stricmp((char *)str1, (char *)str2);
}

//
// Return numeric value of string, or -1 if not valid string
//

#pragma off(unreferenced)
int32_t strval(int32_t argcnt, int8_t *string)
#pragma on(unreferenced)
{
   if (string == NULL)
      return -1L;

   return ascnum(string);
}

//
// Evaluate and return the numeric value of a given DOS environment
// variable's contents
//
// Return -1 if the variable does not exist or cannot be evaluated
//

#pragma off(unreferenced)
int32_t envval(int32_t argcnt, int8_t *name)
#pragma on(unreferenced)
{
   int8_t *env;

   if ((env = (int8_t *)getenv((char *)name)) == NULL)
      return -1L;

   return ascnum(env);
}

//
// Generate speaker beep
//

void beep(void)
{
   uint16_t dx, ax;

   outp(0x43, 0x0b6);
   outp(0x42, 169);
   outp(0x42, 4);

   outp(0x61, (inp(0x61) | 3));

   for (dx = 5; dx > 0; dx--)
      for (ax = 65535; ax > 0; ax--)
         ;

   outp(0x61, (inp(0x61) & 0x0fc));
}

#pragma off(unreferenced)
void pokemem(int32_t argcnt, int32_t *addr, int32_t data)
#pragma on(unreferenced)
{
   *addr = data;
}

#pragma off(unreferenced)
int32_t peekmem(int32_t argcnt, int32_t *addr)
#pragma on(unreferenced)
{
   return *addr;
}

#pragma off(unreferenced)
uint32_t rnd(int32_t argcnt, uint32_t low, uint32_t high)
#pragma on(unreferenced)
{
   // LUM add type int
   static int init = 0;

   if (!init)
   {
      init = 1;
      srand(*(uint16_t *)0x0000046c);
   }

   return low + ((uint32_t)rand() % (high - low + 1L));
}

#pragma off(unreferenced)
uint32_t dice(int32_t argcnt, uint32_t ndice, uint32_t nsides, uint32_t bonus)
#pragma on(unreferenced)
{
   uint32_t n, total;

   total = bonus;

   for (n = 0; n < ndice; n++)
      total += rnd(0, 1, nsides);

   return total;
}

uint32_t inkey(void)
{
   return (uint32_t)kbhit();
}

#pragma off(unreferenced)
uint32_t absv(int32_t argcnt, int32_t val)
#pragma on(unreferenced)
{
   return (val < 0L) ? -val : val;
}

#pragma off(unreferenced)
int32_t minv(int32_t argcnt, int32_t val1, int32_t val2)
#pragma on(unreferenced)
{
   return min(val1, val2);
}

#pragma off(unreferenced)
int32_t maxv(int32_t argcnt, int32_t val1, int32_t val2)
#pragma on(unreferenced)
{
   return max(val1, val2);
}

#pragma off(unreferenced)
void diagnose(int32_t argcnt, uint32_t dtype, uint32_t parm)
#pragma on(unreferenced)
{
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
   return RTR->free;
}
