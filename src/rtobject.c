// ����������������������������������������������������������������������������
// ��                                                                        ��
// ��  RTOBJECT.C                                                            ��
// ��                                                                        ��
// ��  AESOP runtime code resource handlers for Eye III engine               ��
// ��                                                                        ��
// ��  Universal object management                                           ��
// ��                                                                        ��
// ��  Version: 1.00 of 23-Aug-92 -- Initial version                         ��
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

#include <stdio.h>
#include <stdlib.h>
// #include <dos.h> // Tom: commented out
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
// #include <io.h> // Tom: commented out
#include <errno.h>
#include <sys/stat.h> // Tom: replaced `\` with `/`
#include <unistd.h>   // Tom: added for write

#include "defs.h"
#include "shared.h"
#include "rtres.h"
#include "rtlink.h"
#include "rtsystem.h"
#include "rtmsg.h"
#include "intrface.h"
#include "rtobject.h"
#include "rt.h"
#include "event.h"
#include "graphics.h"

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

extern uint32_t check_on;

//
// objlist:  List of handles to all entities in "universe"
//

uint32_t objlist[NUM_OBJECTS];

//
// Other static vars
//

static int8_t linbuf[256];
static int8_t name[256];

/***************************************************/
//
// Initialize object lists
//
// All objlist entries = -1 (available)
//
/***************************************************/

void init_object_list(void)
{
   int32_t i;

   for (i = 0; i < NUM_OBJECTS; i++)
      objlist[i] = -1;
}

/***************************************************/
//
// Return index of first free object list entry within given
// range, or -1 if none available
//
/***************************************************/

int32_t find_free_entry(int32_t min, int32_t end)
{
   int32_t i;

   for (i = min; i < end; i++)
      if (objlist[i] == -1U)
         break;

   if (i == end)
      return -1;
   else
      return i;
}

/***************************************************/
//
// Create an object at a given objlist index
//
/***************************************************/

void create_SOP_instance(uint32_t name, int32_t index)
{
   objlist[index] = create_instance(RTR, name);

   RT_execute(index, MSG_CREATE, -1U);
}

/***************************************************/
//
// Create a SOP object and return its object list index
//
// Returns -1 if no space in list
//
/***************************************************/

int32_t create_object(int32_t argcnt, uint32_t name)
{
   int32_t index;
   (void)argcnt; // Tom: added

   index = find_free_entry(0, NUM_ENTITIES);

   if (index != -1)
      create_SOP_instance(name, index);

   return index;
}

/***************************************************/
//
// Create a SOP program object at specified object list index
//
// Dynamically assign an unused program object index if specified
// index == -1
//
/***************************************************/

int32_t create_program(int32_t argcnt, int32_t index, uint32_t name)
{
   (void)argcnt; // Tom: added

   if (index == -1)
      index = find_free_entry(NUM_ENTITIES, NUM_OBJECTS);

   if (index != -1)
      create_SOP_instance(name, index);

   return index;
}

/***************************************************/
//
// Delete SOP object from object list
//
// Cancel any notifications for object, and release any
// windows allocated by object
//
/***************************************************/

int32_t destroy_object(int32_t argcnt, int32_t index)
{
   int32_t rtn;
   (void)argcnt; // Tom: added

   rtn = RT_execute(index, MSG_DESTROY, -1U);

   cancel_entity_requests(index);
   release_owned_windows(index);

   destroy_instance(RTR, objlist[index]);

   objlist[index] = -1;

   return rtn;
}

/***************************************************/
//
// Thrash resource cache (for diagnostic purposes)
//
/***************************************************/

void thrash_cache(void)
{
   int32_t i, handles[50];

   for (i = 0; i < 3; i++)
   {
      handles[i] = RTR_alloc(RTR, i * 20000, DA_MOVEABLE | DA_DISCARDABLE);
   }

   for (i = 0; i < 3; i++)
   {
      RTR_free(RTR, handles[i]);
   }
}

/***************************************************/
//
// Flush resource cache (for performance-tuning purposes)
//
/***************************************************/

uint32_t flush_cache(int32_t argcnt, uint32_t goal)
{
   (void)argcnt; // Tom: added

   return RTR_force_discard(RTR, goal);
}

/***************************************************/
//
// Dump object's static context to text file
//
/***************************************************/

void dump_static_context(uint32_t index, TF_class *TF)
{
   int32_t i;
   uint32_t n, p, offset, asize;
   uint32_t instance, thunk, expt;
   void *tptr;
   SD_entry *SD;
   THDR thdr;
   int8_t val[32];
   uint8_t *dict;
   int8_t type, *tag, *def, *size;
   void *inst, *d;

   // Tom: new version?
   // sprintf((char *)linbuf, "Entry %u: ", index);

   strcpy((char *)linbuf, "Entry ");
   ltoa(index, (char *)(&linbuf[6]), 10);

   i = strlen((char *)linbuf);
   linbuf[i] = ':';
   linbuf[i + 1] = ' ';
   linbuf[i + 2] = 0;

   instance = objlist[index];

   if (instance == -1U)
   {
      strcat((char *)linbuf, "Available");
      TF_writeln(TF, linbuf);
      return;
   }

   thunk = ((IHDR *)RTR_addr(instance))->thunk;

   tptr = RTR_addr(thunk);
   thdr = *((THDR *)tptr);

   SD = add_ptr(tptr, thdr.SD_list);

   expt = RTR_get_resource_handle(RTR, SD[thdr.nprgs - 1].exports, DA_TEMPORARY | DA_EVANESCENT);
   RTR_lock(RTR, expt);
   linbuf[i + 2] = '"';
   strcpy((char *)(&linbuf[i + 3]), (char *)RTD_lookup(expt, "N:OBJECT"));
   strcat((char *)(&linbuf[i + 3]), "\"");

   RTR_unlock(expt);

   TF_writeln(TF, linbuf);
   TF_writeln(TF, (int8_t *)"{");

   for (p = 0; p < thdr.nprgs; p++)
   {
      tptr = RTR_addr(thunk);
      SD = add_ptr(tptr, thdr.SD_list);

      offset = SD[p].static_base;

      expt = RTR_get_resource_handle(RTR, SD[p].exports, DA_TEMPORARY | DA_EVANESCENT);
      RTR_lock(RTR, expt);

      def = RTD_lookup(expt, "N:OBJECT");

      if (p)
         TF_writeln(TF, (int8_t *)"");
      linbuf[0] = '[';
      strcpy((char *)(&linbuf[1]), (char *)def);
      strcat((char *)linbuf, "]");
      TF_writeln(TF, linbuf);

      inst = add_ptr(RTR_addr(instance), offset);

      dict = RTD_first(RTR_addr(expt));
      while ((dict = RTD_iterate(RTR_addr(expt), dict, &tag, &def)) != NULL)
      {
         type = tag[0];
         if ((type != 'B') && (type != 'W') && (type != 'L'))
            continue;

         strcpy((char *)linbuf, "   ");
         strcpy((char *)(&linbuf[3]), (char *)tag);
         strcat((char *)linbuf, " = ");

         d = add_ptr(inst, ascnum(def));
         if ((size = (int8_t *)strchr((char *)def, ',')) != NULL)
            asize = (uint16_t)ascnum(size + 1);
         else
            asize = 1;

         for (n = 0; n < asize; n++)
         {
            switch (type)
            {
            case 'B':
               ltoa(*(int8_t *)d, (char *)val, 10);
               d = add_ptr(d, 1L);
               break;
            case 'W':
               ltoa(*(int16_t *)d, (char *)val, 10);
               d = add_ptr(d, 2L);
               break;
            case 'L':
               ltoa(*(int32_t *)d, (char *)val, 10);
               d = add_ptr(d, 4L);
               break;
            }

            strcat((char *)linbuf, (char *)val);

            if (n != asize - 1)
            {
               strcat((char *)linbuf, ",");

               if (!((n + 1) % 10))
               {
                  TF_writeln(TF, linbuf);
                  strcpy((char *)linbuf, "      ");
               }
            }
         }

         TF_writeln(TF, linbuf);
      }

      RTR_unlock(expt);
   }

   TF_writeln(TF, (int8_t *)"}");
}

/***************************************************/
//
// Read line from context file, skipping comment lines
//
/***************************************************/

int32_t readln(TF_class *TF, int8_t *buffer, int32_t maxlen)
{
   int32_t status;
   (void)buffer; // Tom: added
   (void)maxlen; // Tom: added

   do
      status = TF_readln(TF, linbuf, sizeof(linbuf));
   while (status && (linbuf[0] == ';'));

   return status;
}

/***************************************************/
//
// Fetch slot and ordinal entry # from context file
//
// Return NULL if EOF reached
//
/***************************************************/

CDESC *read_context_descriptor(TF_class *TF)
{
   static CDESC c;
   uint32_t HROED;
   int8_t *num;
   int8_t *name;
   int8_t *def;

   if (!readln(TF, linbuf, sizeof(linbuf)))
      return NULL;

   num = &linbuf[6];

   // c.size = -1U; // Tom: commented out, new version below
   c.size = UINT16_MAX; // Tom: added, old version above
   c.slot = (uint16_t)ascnum(num);

   name = (int8_t *)strchr((char *)num, '"');
   if (name == NULL)
   {
      c.name = (uint32_t)-1L;
      return &c;
   }

   name++;
   name[strlen((char *)name) - 1] = 0;

   HROED = RTR_get_resource_handle(RTR, ROED, DA_TEMPORARY | DA_EVANESCENT);
   RTR_lock(RTR, HROED);

   def = RTD_lookup(HROED, name);

   if (def == NULL)
      abend(MSG_OMCR, name, c.slot);

   c.name = ascnum(def);

   RTR_unlock(HROED);

   return &c;
}

/***************************************************/
//
// Restore object's static context from text file
//
/***************************************************/

void restore_static_context(uint32_t instance, TF_class *TF)
{
   uint32_t n, i, p, offset, asize;
   uint32_t thunk, expt;
   void *d, *tptr;
   SD_entry *SD;
   THDR thdr;
   int8_t *tag, *def, *size, *chrpnt;

   thunk = ((IHDR *)RTR_addr(instance))->thunk;
   thdr = *((THDR *)RTR_addr(thunk));

   while (readln(TF, linbuf, sizeof(linbuf)))
   {
      if (linbuf[0] == '{')
         continue;
      else if (linbuf[0] == '}')
         break;
      else if (linbuf[0] == '[')
      {
         if ((tag = (int8_t *)strchr((char *)linbuf, ']')) != NULL)
            *tag = 0;
         tag = (int8_t *)strchr((char *)linbuf, '[') + 1;

         for (p = 0; p < thdr.nprgs; p++)
         {
            tptr = RTR_addr(thunk);
            SD = add_ptr(tptr, thdr.SD_list);

            offset = SD[p].static_base;

            expt = RTR_get_resource_handle(RTR, SD[p].exports, DA_TEMPORARY | DA_EVANESCENT);

            RTR_lock(RTR, expt);
            def = RTD_lookup(expt, "N:OBJECT");

            RTR_unlock(expt);

            if (!strcmp((char *)def, (char *)tag))
               break;
         }

         if (p == thdr.nprgs)
            abend(MSG_CMCR, tag); //"Class '%s' missing; cannot restore"
      }
      else
      {
         tag = linbuf;

         // Skip leading white-space

         while (*tag == ' ')
            tag++;

         // Get Variable Type and Name

         for (i = 0; (tag[i] != ' ') && tag[i]; i++)
            name[i] = tag[i];
         name[i] = 0;
         if (!i)
            continue;

         RTR_lock(RTR, expt);
         def = RTD_lookup(expt, name);

         RTR_unlock(expt);

         if (def == NULL)
            abend(MSG_UVR, name); //"Unresolved variable reference '%s'"

         d = (void *)((uint32_t)RTR_addr(instance) + ascnum(def) + offset);

         if ((size = (int8_t *)strchr((char *)def, ',')) != NULL)
            asize = ascnum(size + 1);
         else
            asize = 1;

         chrpnt = (int8_t *)strchr((char *)linbuf, '=');
         if (chrpnt == NULL)
            abend(MSG_BDIE); //"Bad data item entry"

         for (n = 0; n < asize; n++)
         {

            // Skip to data

            while ((!isnum(*chrpnt)) && (*chrpnt != '\''))
            {
               if (!(*chrpnt))
                  break;
               else
                  ++chrpnt;
            }

            // Get Next Line if needed

            if ((!(*chrpnt)) && (n != asize - 1))
            {
               readln(TF, linbuf, sizeof(linbuf));
               chrpnt = linbuf;
            }

            // Store data from text file in Instance

            switch (name[0])
            {
            case 'B':
               *(int8_t *)d = (int8_t)ascnum(chrpnt);
               break;
            case 'W':
               *(int16_t *)d = (int16_t)ascnum(chrpnt);
               break;
            case 'L':
               *(int32_t *)d = (int32_t)ascnum(chrpnt);
               break;
            }

            if (n != asize - 1)
            {
               switch (name[0])
               {
               case 'B':
                  d = ((int8_t *)d + 1);
                  break;
               case 'W':
                  d = ((int8_t *)d + 2);
                  break;
               case 'L':
                  d = ((int8_t *)d + 4);
                  break;
               }

               while (isnum(*chrpnt) || (*chrpnt == ' ') || (*chrpnt == '\''))
                  chrpnt++;
            }
         } // for
      } // else
   } // while
}

/*********************************************************/
//
// Save static context of range of objects to text or binary file
//
// Return 1 if write succeeded (disk full or other error if 0)
//
/*********************************************************/

int32_t save_range(int8_t *filename, int32_t filetype, int32_t first, int32_t last)
{
   int32_t good, index;
   uint8_t typetest;
   int handle;
   TF_class *TF;
   uint32_t instance, thunk;
   HD_entry *hd_inst;
   void *tptr;
   THDR thdr;
   CDESC CD;

   good = 1;

   if (filetype == SF_TXT)
   {
      TF = TF_construct(filename, TF_WRITE);
      if (TF == NULL)
         return 0;

      for (index = first; index <= last; index++)
      {
         dump_static_context(index, TF);

         if (!TF_writeln(TF, (int8_t *)""))
         {
            good = 0;
            break;
         };
      }

      TF_destroy(TF);
   }
   else
   {
      handle = open((char *)filename, O_CREAT | O_RDWR | O_TRUNC | O_BINARY, S_IREAD | S_IWRITE);
      if (handle == -1)
         return 0;

      typetest = 26;
      write(handle, &typetest, 1);

      for (index = first; index <= last; index++)
      {
         CD.slot = index;

         instance = objlist[index];
         if (instance == -1U)
         {
            CD.name = (uint32_t)-1L;
            CD.size = 0;
         }
         else
         {
            thunk = ((IHDR *)RTR_addr(instance))->thunk;

            tptr = RTR_addr(thunk);
            thdr = *((THDR *)tptr);

            hd_inst = (HD_entry *)instance;
            CD.name = hd_inst->user;
            CD.size = thdr.isize - sizeof(IHDR);
         }

         write(handle, &CD, sizeof(CDESC));

         if (CD.size)
            if (write(handle, add_ptr(RTR_addr(objlist[index]), sizeof(IHDR)), CD.size) != CD.size)
            {
               good = 0;
               break;
            }
      }

      close(handle);
   }

   return good;
}

/*********************************************************/
//
// Restore static context of range of objects from text or binary file
//
// For each object list entry between first and last:
//
//   Read entry's context descriptor
//
//   If EOF or CDESC.slot != entry #
//       Report corrupted context file
//
//   If CDESC.name == -1 (i.e., slot unused in file)
//       If slot entry exists, destroy it
//       Continue
//
//   If slot entry exists
//       If CDESC.name != current entry's name
//          Destroy the current entry to make room for the new one
//       Else
//          Destruction of current entry not necessary; deallocate any
//          assigned resources but leave entry otherwise intact
//
//   If slot entry available (perhaps newly destroyed), create the object
//
//   Restore object's instance data from context file
//
//   If restoring, send "restore" message to newly instantiated object
//
// Errors during context restoration are fatal at the system level;
// this routine will not return if restoration fails
//
/*********************************************************/

void restore_range(int8_t *filename, uint32_t first, uint32_t last, uint32_t restoring)
{
   uint32_t bad, txttype;
   uint32_t index;
   uint8_t typetest;
   int handle;
   TF_class *TF;
   uint32_t cur;
   HD_entry *sel;
   CDESC stat_C;
   CDESC *CD;

   // Tom: commented out below, not used?
   // uint16_t CDslot; // object list index
   // uint32_t CDname; // code object name
   // uint16_t CDsize; // size of instance data (unused in text files)

   txttype = 0;
   bad = 0;

   handle = open((char *)filename, O_RDONLY | O_BINARY);
   if (handle == -1)
      bad = 1;
   else
   {
      typetest = 0;
      read(handle, &typetest, 1);

      if (typetest != 26)
      {
         close(handle);
         txttype = 1;
      }
   }

   if (txttype)
   {
      TF = TF_construct(filename, TF_READ);
      bad = (TF == NULL);
   }

   if (bad)
      abend(MSG_CNOCF, filename); //"Could not open context file '%s'"

   for (index = first; index <= last; index++)
   {
      cur = objlist[index];

      if (txttype)
      {
         CD = read_context_descriptor(TF);

         bad = (CD == NULL);
      }
      else
      {
         CD = &stat_C;
         bad = (read(handle, CD, sizeof(CDESC)) != sizeof(CDESC));
      }

      if ((bad) || (CD->slot != index))
         abend(MSG_CFCAE, index); //"Context file corrupt at entry %u"

      // Tom: commented out below, not used?
      // CDslot = CD->slot;
      // CDname = CD->name;
      // CDsize = CD->size;

      // if (CD->name == (uint32_t)-1L) // Tom: commented out, new version below
      if (CD->name == UINT32_MAX)
      {
         // if (cur != -1) // Tom: commented out, might have originally had -1L
         if (cur != UINT32_MAX) // Tom: added
         {
            destroy_object(0, index);
         }

         continue;
      }

      // if (cur != -1) // Tom: commented out, original might have been -1L
      if (cur != UINT32_MAX) // Tom: added
      {
         sel = (HD_entry *)cur;

         if (sel->user != CD->name)
         {
            destroy_object(0, index);
         }
         else
         {
            cancel_entity_requests(index);

            release_owned_windows(index);
         }
      }

      if (objlist[index] == -1U)
      {
         create_SOP_instance(CD->name, index);
      }

      if (txttype)
      {
         restore_static_context(objlist[index], TF);
      }
      else if (CD->size)
      {
         read(handle, (int8_t *)RTR_addr(objlist[index]) + sizeof(IHDR), CD->size);
      }

      if (restoring)
      {
         RT_execute(index, MSG_RESTORE, -1U);
      }
   }

   if (txttype)
      TF_destroy(TF);
   else
      close(handle);
}

/*********************************************************/
//
// Translate static context file from text to binary format
//
// For each entry in file:
//
// Read entry's context descriptor
// Save descriptor to binary file
//
//   If EOF or CDESC.slot != entry #
//       Report corrupted context file
//
//   If CDESC.name == -1 (i.e., slot unused in file)
//       Continue
//
//   Create source object instance
//   Restore instance from text file
//   Save instance to binary file
//   Destroy instance
//
// Errors during context file translation are fatal at the system level;
// this routine will not return if translation fails
//
/*********************************************************/

void translate_file(int8_t *TXT_filename, int8_t *BIN_filename, uint32_t first, uint32_t last)
{
   TF_class *TF;
   CDESC *CD;
   CDESC CD_out;
   int handle;
   int32_t index;
   uint32_t instance, thunk;
   void *tptr;
   THDR thdr;
   uint8_t typetest;

   handle = open((char *)BIN_filename, O_CREAT | O_RDWR | O_TRUNC | O_BINARY, S_IREAD | S_IWRITE);
   if (handle == -1)
      abend(MSG_COOFFT); //"Couldn't open output file for translation"

   typetest = 26;
   write(handle, &typetest, 1);

   TF = TF_construct(TXT_filename, TF_READ);
   if (TF == NULL)
      abend(MSG_COIFFT); //"Couldn't open input file for translation"

   // Tom: original line commented out
   // for (index = first; index <= last; index++)
   for (index = (int32_t)first; index <= (int32_t)last; index++) // Tom: added
   {
      CD = read_context_descriptor(TF);
      // if ((CD == NULL) || (CD->slot != index)) // Tom: original line
      if ((CD == NULL) || (CD->slot != (uint16_t)index)) // Tom: added
      {
         abend(MSG_CTCFE, index); //"Couldn't translate context file entry %u"
      }

      CD_out.name = CD->name;
      CD_out.slot = index;

      if (CD->name != (uint32_t)-1L)
      {
         instance = create_instance(RTR, CD->name);

         thunk = ((IHDR *)RTR_addr(instance))->thunk;

         tptr = RTR_addr(thunk);
         thdr = *((THDR *)tptr);

         CD_out.size = thdr.isize - sizeof(IHDR);

         write(handle, &CD_out, sizeof(CDESC));

         restore_static_context(instance, TF);

         if (CD_out.size)
         {
            if (write(handle, add_ptr(RTR_addr(instance), sizeof(IHDR)), CD_out.size) != CD_out.size)
            {
               abend(MSG_CWTE, index); //"Couldn't write translated entry %u"
            }
         }

         destroy_instance(RTR, instance);
      }
      else
      {
         CD_out.size = 0;
         write(handle, &CD_out, sizeof(CDESC));
      }
   }

   TF_destroy(TF);
   close(handle);
}
