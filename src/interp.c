// ############################################################################
// ##                                                                        ##
// ##  INTERP.C                                                              ##
// ##                                                                        ##
// ##  AESOP runtime host interpreter                                        ##
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
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "defs.h"
#include "shared.h"
#include "rtmsg.h"
#include "rtres.h"
#include "rtsystem.h"
#include "rt.h"
#include "rtlink.h"
#include "rtobject.h"
#include "rtcode.h"
#include "event.h"

//
// Amount of memory to reserve for scaling buffer (64K) + PAGE2 (64K) + misc.
// (22K)
//

#define WINDOW_SIZE 150000

//
// Amount of memory to reserve for DLL loading/linking (100K)
//

#define DLL_HEADROOM 100000

//
// Amount of memory to reserve for miscellaneous malloc() calls (32K)
//

#define MALLOC_HEADROOM 32768

//
// Amount of memory to reserve for AESOP interpreter stack (16K)
//

#define STK_SIZE 16384

//
// Minimum AESOP resource cache size permissible (600K)
//

#define MIN_RES_SIZE 600000

//
// Maximum useful AESOP resource cache size (800K)
//

#define MAX_RES_SIZE 800000

//
// Globals
//

RTR_class *RTR;

uint32_t HROED;
uint32_t heap_size;

/*************************************************************/
// void main(int argc, char *argv[]) // Tom: commented out, new version below
int main(int argc, char *argv[]) // Tom: added
{
   int8_t RES_name[256];
   int8_t code_name[256];
   uint32_t i;
   uint32_t code;
   int32_t rtn;

   printf("[interp] main: set stdout to unbuffered\n");
   setbuf(stdout, NULL);

   printf("[interp] main: set ENABLED to 1\n");
   ENABLED = 1;

   // AIL_startup(); // Tom: commented out
   mem_init();

   if (argc < 3)
   {
      printf(MSG_BANNER);
      printf(MSG_SYN_1);
      abend(NULL);
   }

   strcpy((char *)RES_name, argv[1]);
   for (i = 0; i < strlen((char *)RES_name); i++)
   {
      if (RES_name[i] == '.')
      {
         RES_name[i] = 0;
         break;
      }
   }
   strcat((char *)RES_name, ".RES");

   printf("[interp] main: resource file: %s\n", RES_name);

   strcpy((char *)code_name, argv[2]);

   printf("[interp] main: code name: %s\n", code_name);

   heap_size = mem_avail() -
               WINDOW_SIZE -
               DLL_HEADROOM -
               MALLOC_HEADROOM -
               STK_SIZE;

   if (heap_size < MIN_RES_SIZE)
   {
      abend(MSG_NO_DOS);
   }

   if (heap_size > MAX_RES_SIZE)
      heap_size = MAX_RES_SIZE;

   printf("[interp] main: heap size: %u\n", heap_size);

   RTR = RTR_construct(mem_alloc(heap_size), heap_size, MAX_OBJ_TYPES, RES_name);
   if (RTR == NULL)
   {
      abend(MSG_RIF, RES_name);
   }

   printf("[interp] main: RTR contructed\n");

   init_object_list();
   init_notify_list();
   init_event_queue();

   RT_init(RTR, STK_SIZE, objlist);

   printf("[interp] main: RT initialised\n");

   HROED = RTR_get_resource_handle(RTR, ROED, DA_TEMPORARY | DA_EVANESCENT);
   RTR_lock(RTR, HROED);
   code = ascnum(RTD_lookup(HROED, code_name));
   RTR_unlock(HROED);

   if (code == UINT32_MAX)
      abend(MSG_SPNF);

   printf("[interp] main: running create_program with code=%u\n", code);

   rtn = create_program(1, bootstrap, code);
   rtn = destroy_object(1, rtn);

   for (i = 0; i < RTR->nentries; i++)
   {
      uint32_t f;

      f = RTR->dir[i].flags;

      if ((f & DA_FREE) && (f & DA_DISCARDED) && (!RTR->dir[i].seg))
         break;
   }

   if (envval(0, (int8_t *)"AESOP_DIAG") == 1)
   {
      printf("Entries avail: %u\n", RTR->nentries);
      printf("       In use: %u\n\n", i);

      printf("%u names in use\n", RTR->nd_entries);
   }

   RTR_destroy(RTR, RTR_FREEBASE);
   RT_shutdown();

   mem_shutdown();
   // AIL_shutdown(MSG_AIL); // Tom: commented out

   if (envval(0, (int8_t *)"AESOP_DIAG") == 1)
   {
      printf("%u bytes in heap\n", heap_size);
      printf("%u bytes left\n", mem_headroom());
   }

   exit(rtn);

   return 0;
}
