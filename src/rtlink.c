// ����������������������������������������������������������������������������
// ��                                                                        ��
// ��  RTLINK.C                                                              ��
// ��                                                                        ��
// ��  AESOP runtime object linker                                           ��
// ��                                                                        ��
// ��  Version: 1.00 of 13-Jun-92 -- Initial version                         ��
// ��                                                                        ��
// ��  Project: Extensible State-Object Processor (AESOP/16)                 ��
// ��   Author: John Miles                                                   ��
// ��                                                                        ��
// ��  C source compatible with IBM PC ANSI C/C++ implementations            ��
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
#include <stdint.h> // Tom: added

#include "defs.h"
#include "rtres.h"
#include "rt.h"
#include "rtlink.h"
#include "rtcode.h"
#include "rtsystem.h"
#include "rtmsg.h"

typedef struct
{
   uint32_t val;
} XCR_entry; // External code reference entry

typedef struct
{
   uint16_t offset;
} XDR_entry; // External data reference entry

/***************************************************/
//
// Utility subs for thunk creation
//
/***************************************************/

int sort_by_msg(const void *a, const void *b) // Tom: TODO change int to int32_t?
{
   uint16_t na, nb;

   na = ((MV_entry *)a)->msg;
   nb = ((MV_entry *)b)->msg;

   if (na > nb)
      return 1;
   else if (na == nb)
      return 0;
   else
      return -1;
}

int sort_by_class(const void *a, const void *b) // Tom: TODO change int to int32_t?
{
   uint16_t na, nb;

   na = ((MV_entry *)a)->SD_offset;
   nb = ((MV_entry *)b)->SD_offset;

   if (na < nb)
      return 1;
   else if (na == nb)
      return 0;
   else
      return -1;
}

/***************************************************/
//
// Create thunk for specified object
//
/***************************************************/

uint32_t construct_thunk(RTR_class *RTR, RTR_class *LNK, uint32_t object)
{
   uint16_t depth; // 0..MAX_G = derived object..base class
   uint32_t class, xclass;
   uint32_t tsize;
   uint32_t source;
   uint16_t target;
   int16_t i;
   uint16_t j, k, m, n, mcnt;
   PRG_HDR prg, xprg;
   THDR thdr;
   uint8_t *dict;
   int8_t *tag, *tagbase, *def;
   uint32_t thunk;
   uint32_t HCRFD;
   uint32_t impt[MAX_G], expt[MAX_G], code[MAX_G];
   uint32_t xexpt, xcode;
   uint32_t exports[MAX_G];
   uint16_t s_S[MAX_G], x_S[MAX_G];
   uint16_t SD_offset[MAX_G];
   uint16_t index, offset, found;
   uint16_t XR_list;
   SD_entry *SD, *SDarray;
   MV_entry *MV;
   void *XR;
   XDR_entry *xdr_ptr;

   void *thunk_ptr;
   uint32_t def_off;
   uint32_t *XR_ptr, *CR_ptr;

   // Tom: added these to zero local arrays

   memset(code, 0, sizeof(code));
   memset(impt, 0, sizeof(impt));
   memset(expt, 0, sizeof(expt));
   memset(exports, 0, sizeof(exports));
   memset(s_S, 0, sizeof(s_S));
   memset(x_S, 0, sizeof(x_S));
   memset(SD_offset, 0, sizeof(SD_offset));

   //
   // Load programs and dictionaries, calculate thunk size
   //

   HCRFD = RTR_get_resource_handle(RTR, CRFD, DA_TEMPORARY | DA_EVANESCENT);
   RTR_lock(RTR, HCRFD);

   thdr.MV_list = sizeof(THDR);
   thdr.SD_list = sizeof(THDR);
   thdr.max_msg = UINT16_MAX;
   thdr.nprgs = 0;
   thdr.isize = sizeof(IHDR);
   thdr.use_cnt = 0;

   XR_list = sizeof(THDR);

   tsize = sizeof(THDR);

   for (i = 0; i < MAX_G; i++)
      s_S[i] = x_S[i] = 0;

   depth = 0;
   class = object;

   while (class != UINT32_MAX)
   {
      code[depth] = RTR_get_resource_handle(RTR, class, DA_DEFAULT);

      if (!code[depth])
         abend(MSG_PONF, class); // "Program object %lu not found"

      RTR_lock(RTR, code[depth]);

      prg = *(PRG_HDR *)RTR_addr(code[depth]);

      ++thdr.nprgs;
      tsize += sizeof(SD_entry);
      XR_list += sizeof(SD_entry);

      exports[depth] = prg.exports;

      impt[depth] = RTR_get_resource_handle(LNK, prg.imports, DA_TEMPORARY | DA_EVANESCENT);
      RTR_lock(LNK, impt[depth]);

      expt[depth] = RTR_get_resource_handle(LNK, prg.exports, DA_TEMPORARY | DA_EVANESCENT);
      RTR_lock(LNK, expt[depth]);

      //
      // Calculate Size of Message Vector List
      //

      mcnt = 0;
      dict = RTD_first(RTR_addr(expt[depth]));
      while ((dict = RTD_iterate(RTR_addr(expt[depth]), dict, &tag, &def)) != NULL)
      {
         switch (tag[0])
         {
         case 'M': // Message
            ++thdr.max_msg;
            ++mcnt;
            thdr.SD_list += sizeof(MV_entry);
            XR_list += sizeof(MV_entry);
            tsize += sizeof(MV_entry);
            break;
         }
      }

      //
      // Calculate Size of External Reference List
      //

      dict = RTD_first(RTR_addr(impt[depth]));
      while ((dict = RTD_iterate(RTR_addr(impt[depth]), dict, &tag, &def)) != NULL)
      {
         switch (tag[0])
         {
         case 'C': // Code
            tsize += sizeof(XCR_entry);
            x_S[depth] += sizeof(XCR_entry);
            break;

         case 'B': // Byte
         case 'W': // Word
         case 'L': // Long
            tsize += sizeof(XDR_entry);
            x_S[depth] += sizeof(XDR_entry);
            break;
         }
      }

      s_S[depth] += prg.static_size;
      thdr.isize += prg.static_size;

      class = prg.parent;

      if (++depth == MAX_G)
         abend(MSG_AILE); // "AESOP inheritance limit exceeded"

      if (tsize > 65535L)
         abend(MSG_TTL); // "Thunk too large"
   }

   //
   // Allocate memory for thunk and construct it
   //

   thunk = RTR_alloc(RTR, tsize, DA_MOVEABLE | DA_PRECIOUS);

   *(THDR *)RTR_addr(thunk) = thdr;

   // SD = (SD_entry *)thdr.SD_list; // Tom: commented out, new possibly broken version below
   SD = (SD_entry *)((uint32_t)thdr.SD_list); // Tom: added

   i = depth - 1;
   j = 0;
   k = sizeof(IHDR);
   m = XR_list;
   n = thdr.SD_list;

   while (i >= 0)
   {
      SDarray = (SD_entry *)((uint32_t)RTR_addr(thunk) + (uint32_t)SD);
      SDarray[j].SOP = code[i];
      SDarray[j].exports = exports[i];
      SDarray[j].static_base = k;
      SDarray[j].extern_base = m;

      SD_offset[i] = n;

      // XR = (void *)m; // Tom: commented out, new possibly broken version below
      XR = (void *)((uint32_t)m);

      dict = RTD_first(RTR_addr(impt[i]));
      while ((dict = RTD_iterate(RTR_addr(impt[i]), dict, &tag, &def)) != NULL)
      {
         int8_t *loRTDLookupResult;
         tagbase = RTR_addr(impt[i]);
         switch (tag[0])
         {
         case 'C': // Code

            // LUM fixed the testing of the RTD_lookup result
            loRTDLookupResult = RTD_lookup(HCRFD, &tag[2]);
            offset = (uint16_t)ascnum(loRTDLookupResult);
            // if (offset == -1U)
            if (loRTDLookupResult == NULL)
               abend(MSG_MCR, &tag[2]); // "Missing code resource '%s'"

            thunk_ptr = RTR_addr(thunk);
            def_off = ascnum(def);
            XR_ptr = (void *)((uint32_t)thunk_ptr + (uint32_t)XR + def_off);
            CR_ptr = (void *)((uint32_t)&code_resources + offset);
            *XR_ptr = *CR_ptr;

            break;

         case 'B': // Byte
         case 'W': // Word
         case 'L': // Long
            target = (uint16_t)ascnum(def);
            source = ascnum((int8_t *)strchr((char *)def, ',') + 1);

            xclass = source;
            index = sizeof(IHDR);
            found = 0;

            // while (xclass != -1L) // Tom: commented out, new version below
            while (xclass != UINT32_MAX)
            {
               xcode = RTR_get_resource_handle(RTR, xclass, DA_DEFAULT);

               if (!xcode)
                  abend(MSG_FPNF, xclass); //"Friend program %lu not found"

               RTR_lock(RTR, xcode);
               tag = tag - tagbase + (int8_t *)RTR_addr(impt[i]);

               xprg = *(PRG_HDR *)RTR_addr(xcode);

               xclass = xprg.parent;

               if (!found)
               {
                  xexpt = RTR_get_resource_handle(LNK, xprg.exports, DA_TEMPORARY | DA_EVANESCENT);

                  RTR_lock(LNK, xexpt);
                  tag = tag - tagbase + (int8_t *)RTR_addr(impt[i]);

                  // LUM fixed the testing of the RTD_lookup result
                  loRTDLookupResult = RTD_lookup(xexpt, tag);
                  offset = (uint16_t)ascnum(loRTDLookupResult);
                  // if (offset != -1U)
                  if (loRTDLookupResult != NULL)
                  {
                     found = 1;
                     index += offset;
                  }

                  RTR_unlock(xexpt);
               }
               else
                  index += xprg.static_size;

               RTR_unlock(xcode);
            }

            if (!found)
               abend(MSG_UER, tag); //"Unresolved external reference '%s'"

            thunk_ptr = RTR_addr(thunk);
            xdr_ptr = (XDR_entry *)((uint32_t)thunk_ptr + (uint32_t)XR + target);
            xdr_ptr->offset = index;

            break;
         }
      }

      n += sizeof(SD_entry);
      m += x_S[i];
      k += s_S[i];
      j++;
      i--;
   }

   MV = add_offset(RTR_addr(thunk), thdr.MV_list);

   for (i = m = 0; i < depth; i++)
   {
      dict = RTD_first(RTR_addr(expt[i]));
      while ((dict = RTD_iterate(RTR_addr(expt[i]), dict, &tag, &def)) != NULL)
         if (tag[0] == 'M')
         {
            MV[m].msg = (uint16_t)ascnum(&tag[2]);
            MV[m].handler = (uint32_t)ascnum(def);
            MV[m].SD_offset = SD_offset[i];
            ++m;
         }
   }

   if (m)
   {
      //
      // Sort all message vectors by ascending message number
      //

      qsort(MV, m, sizeof(MV_entry), sort_by_msg);

      //
      // Sort identical message vectors by ascending class
      //

      k = m - 1;
      for (i = 0; i < k; i++)
         if (MV[i + 1].msg == MV[i].msg)
         {
            for (j = i + 2; j < m; j++)
               if (MV[j].msg != MV[i].msg)
                  break;

            qsort(&MV[i], j - i, sizeof(MV_entry), sort_by_class);

            i = j - 1;
         }
   }

   //
   // Unlock dictionaries and exit w/handle to thunk
   //

   RTR_unlock(HCRFD);
   for (i = 0; i < depth; i++)
   {
      RTR_unlock(impt[i]);
      RTR_unlock(expt[i]);
      RTR_unlock(code[i]);
   }

   return thunk;
}

/***************************************************/
//
// Create SOP instance, building thunk if necessary
//
/***************************************************/

uint32_t create_instance(RTR_class *RTR, uint32_t object)
{
   uint32_t thunk, instance;
   HD_entry *sel;
   ND_entry *entry;
   THDR *thdr;
   IHDR ihdr;

   entry = RTR_search_name_dir(RTR, object);

   if ((entry == NULL) || (entry->thunk == UINT32_MAX))
   {
      thunk = construct_thunk(RTR, RTR, object);

      RTR_search_name_dir(RTR, object)->thunk = thunk;
   }
   else
      thunk = (uint32_t)entry->thunk;

   ihdr.thunk = thunk;

   thdr = (THDR *)RTR_addr(thunk);
   ++thdr->use_cnt;

   instance = RTR_alloc(RTR, thdr->isize, DA_MOVEABLE | DA_PRECIOUS);

   sel = (HD_entry *)instance;

   sel->user = object;

   *(IHDR *)RTR_addr(instance) = ihdr;

   return instance;
}

/***************************************************/
//
// Destroy instance of specified SOP
//
// Delete thunk if no other instances of this object exist
//
/***************************************************/

void destroy_instance(RTR_class *RTR, uint32_t instance)
{
   uint32_t thunk;

   thunk = ((IHDR *)RTR_addr(instance))->thunk;

   if (!(--(((THDR *)RTR_addr(thunk))->use_cnt)))
      RTR_free(RTR, thunk);

   RTR_free(RTR, instance);
}
