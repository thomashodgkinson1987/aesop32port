// ����������������������������������������������������������������������������
// ��                                                                        ��
// ��  RTRES.C                                                               ��
// ��                                                                        ��
// ��  AESOP runtime memory, resource, and data structure management         ��
// ��                                                                        ��
// ��  Version: 1.00 of 6-May-92 -- Initial version                          ��
// ��                                                                        ��
// ��  Project: Extensible State-Object Processor (AESOP/16)                 ��
// ��   Author: John Miles                                                   ��
// ��                                                                        ��
// ��  C source compatible with Borland C++ v3.0                             ��
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
#include <fcntl.h>
// #include <io.h> // Tom: commented out
// #include <dos.h> // Tom: commented out
#include <string.h>
#include <unistd.h> // Tom: added

#include "defs.h"
#include "rtsystem.h"
#include "rtmsg.h"
#include "rtres.h"
#include "rt.h"
#include "intrface.h"
#include "graphics.h" // for dprint()
// #include "modsnd32.h" // Tom: commented out

#define FAST_LOCK 0 // user field = resource name if 0, file offset if 1

/******************************************/
//
// Diagnostic code
//
/******************************************/

#if 0
uint32_t RTR_chksum(uint32_t entry)
{
   HD_entry *sel;
   uint8_t *ptr;
   uint32_t chksum;

   sel = (HD_entry *)entry;
   ptr = sel->seg;

   chksum = 0;
   while (((uint32_t)ptr) < ((uint32_t)(sel->seg) + sel->size))
   {
      chksum += (uint32_t)*ptr;
      ptr++;
   }

   return (uint32_t)chksum;
}

void RTR_double_check(uint32_t caller, uint32_t entry)
{
   HD_entry *sel;
   uint8_t *ptr;
   uint32_t chksum;
   uint8_t *cname;

   sel = (HD_entry *)entry;
   ptr = sel->seg;

   chksum = 0;
   while (((uint32_t)ptr) < ((uint32_t)(sel->seg) + sel->size))
   {
      chksum += (uint32_t)*ptr;
      ptr++;
   }

   if (sel->chksum != chksum)
   {
      if (caller > 10)
      {
         cname = (uint8_t *)caller;
      }
      else
      {
         switch (caller)
         {
         case 6:
            cname = "before RT_execute: do_END";
            break;
         case 5:
            cname = "in RT_execute";
            break;
         case 4:
            cname = "before get_res_handle";
            break;
         case 3:
            cname = "during PASS";
            break;
         case 2:
            cname = "during SEND";
            break;
         case 1:
            cname = "during LOCK";
            break;
         case 0:
            cname = "during CALL";
            break;
         }
      }
      abend("RES# %d trashed %s.\n  HRES=%x -> %x  Size = %x  Flags = %x  Old=%x New=%x\n",
            sel->user, cname, sel, sel->seg, sel->size, sel->flags, sel->chksum, chksum);
   }
}

void resource_chksum(int8_t *situation)
{
   uint32_t res = CHKRES;
   int32_t i;

   if (CHKRES == 0)
      return;

   if (RTR == NULL)
      return;

   for (i = 0; i < RTR->nentries; i++)
      if (RTR->dir[i].user == res)
         break;

   if (i != RTR->nentries)
   {
      if (!(RTR->dir[i].flags & DA_DISCARDED))
         RTR_double_check((uint32_t)situation, (uint32_t)&RTR->dir[i]);
      else
         printf("Resource # %d discarded %s\n", res, situation);
   }
}

void RTR_flag_check(int8_t *situation)
{
   HD_entry *sel;

   if ((uint32_t)situation > 10)
   {
      sel = (HD_entry *)HRES_to_check;

      if ((sel->flags & DA_DISCARDED) || (sel->flags & DA_FREE))
      {
         abend("RES# %d flags trashed %s.\n  HRES=%x -> %x  Size = %x  Flags = %x \n",
               sel->user, situation, sel, sel->seg, sel->size, sel->flags);
      }
   }
}

void RTR_HRES_chksum(int8_t *situation)
{
   if (HRES_to_check == 0)
      return;

   if (check_on == 0)
      return;

   // RTR_flag_check(situation);

   // RTR_double_check((uint32_t)situation, (uint32_t) HRES_to_check);
}
#endif

/***************************************************/
//
// Discard a given resource to expand free space
// in cache (MOVEABLE resources only)
//
// If do_move=0, caller is responsible for garbage collection
//
// If discarded object is EVANESCENT, free its entry
//
// Returns size of discarded object in bytes (rounded to next paragraph)
//
/***************************************************/

static uint32_t RTR_discard(RTR_class *RTR, uint32_t index, uint32_t do_move)
{
   uint32_t i, n;
   void *dest, *src;
   uint32_t nbytes, size;
   HD_entry *sel;

   sel = &RTR->dir[index];

   size = sel->size;
   dest = sel->seg;
   src = add_ptr(dest, size);
   nbytes = ptr_dif(RTR->next_M, src);

   n = RTR->nentries;
   for (i = 0; i < n; i++)
   {
      if (RTR->dir[i].flags & (DA_FIXED | DA_DISCARDED))
         continue;

      if (ptr_dif(RTR->dir[i].seg, dest) > 0)
         RTR->dir[i].seg = (int8_t *)RTR->dir[i].seg - size;
   }

   if (do_move)
   {
      // PollMod(); // Tom: commented out
      far_memmove(dest, src, nbytes);
   }

   sel->flags |= DA_DISCARDED;

   RTR->next_M = (int8_t *)RTR->next_M - size;
   RTR->free = RTR->free + size;

   if (sel->flags & DA_EVANESCENT)
   {
      RTR_free(RTR, (uint32_t)sel);
   }

   return size;
}

/***************************************************/
//
// Return index of least-recently-used, unlocked, MOVEABLE resource
//
/***************************************************/

static uint32_t RTR_LRU(RTR_class *RTR)
{
   uint32_t i, oldest;
   uint32_t n, age;

   n = RTR->nentries;
   oldest = age = -1;

   for (i = 0; i < n; i++)
   {
      if (RTR->dir[i].flags &
          (DA_FIXED | DA_PRECIOUS | DA_DISCARDED | DA_FREE))
         continue;

      if (RTR->dir[i].locks > 0)
         continue;

      if (RTR->dir[i].history < age)
      {
         age = RTR->dir[i].history;
         oldest = i;
      }
   }

   return oldest;
}

/***************************************************/
//
// Create /goal/ bytes of free space in resource cache:
//
// 1) If /goal/ bytes already free, return immediately
//
// 2) Discard all MOVABLE FREE entries which are still present
//
// 3) If /goal/ bytes free, return
//
// 4) Discard all unlocked TEMPORARY entries
//
// 5) While free < goal
//
//       Discard least-recently-used valid candidate
//
//       If no LRU candidates available, return FALSE
//
// 6) Return TRUE
//
/***************************************************/

static uint32_t RTR_make_room(RTR_class *RTR, uint32_t goal)
{
   int32_t i;
   int16_t first, next;
   uint32_t index;
   uint32_t next_seg, size_deleted, nbytes;
   void *dest, *src, *end;

   //
   // 1) If /goal/ bytes already free, return immediately
   //

   if (RTR->free >= goal)
      return 1;

   standby_cursor();

   //
   // 2) Discard all MOVABLE FREE entries which are still present
   //

   for (i = RTR->nentries - 1; i >= 0; i--)
      if ((RTR->dir[i].flags & DA_FREE) &&
          (!(RTR->dir[i].flags & (DA_FIXED | DA_DISCARDED))))
      {
         RTR_discard(RTR, i, 1);
      }

   //
   // 3) If /goal/ bytes free, return
   //

   if (RTR->free >= goal)
   {
      resume_cursor();
      return 1;
   }

   //
   // 4) Discard all unlocked TEMPORARY entries
   //

   for (first = RTR->nentries - 1; first >= 0; first--)
   {
      if (RTR->dir[first].flags &
          (DA_FIXED | DA_PRECIOUS | DA_DISCARDABLE | DA_DISCARDED))
         continue;

      if (RTR->dir[first].locks > 0)
         continue;
#if 1
      //
      // Found at least one unlocked TEMPORARY entry; try to merge adjacent
      // entries' blocks to avoid redundant calls to far_memmove()
      //

      for (next = first - 1; next >= 0; next--)
      {
         next_seg = (uint32_t)RTR->dir[next + 1].seg;

         if (RTR->dir[next].flags &
             (DA_FIXED | DA_PRECIOUS | DA_DISCARDABLE | DA_DISCARDED))
            break;

         if (RTR->dir[next].locks > 0)
            break;

         if (((uint32_t)RTR->dir[next].seg + RTR->dir[next].size) != next_seg)
            break;
      }

      end = RTR->next_M;
      dest = RTR->dir[next + 1].seg;

      size_deleted = 0;
      for (i = next + 1; i <= first; i++)
         size_deleted += RTR_discard(RTR, i, 0);

      src = add_ptr(dest, size_deleted);
      nbytes = ptr_dif(end, src);

      // PollMod(); // Tom: commented out
      far_memmove(dest, src, nbytes);

      first = next + 1;
#else
      RTR_discard(RTR, first, 1);
#endif
   }

   //
   // 5) While free < goal
   //

   while (RTR->free < goal)
   {
      //
      // If no LRU candidates available, return FALSE
      //
      index = RTR_LRU(RTR);
      if (index == -1)
      {
         resume_cursor();
         return 0;
      }

      //
      // Discard least-recently-used valid candidate
      //
      RTR_discard(RTR, index, 1);
   }

   resume_cursor();

   return 1;
}

/***************************************************/
//
// Get /bytes/ free space for /entry/, establish selector fields
//
// Note: Does not initialize user field
//
/***************************************************/

static uint32_t RTR_assign_space(RTR_class *RTR, uint32_t bytes, uint32_t attrib, uint32_t entry)
{
   HD_entry *sel;

   sel = (HD_entry *)entry;

   if (!RTR_make_room(RTR, bytes))
      return 0;

   sel->flags = attrib & 0xffffffff;
   sel->locks = 0;
   sel->size = bytes;
   sel->history = RTR->LRU_cnt;

   RTR->free -= bytes;

   if (attrib & DA_FIXED)
   {
      RTR->last_F = (uint8_t *)RTR->last_F - bytes;
      sel->seg = RTR->last_F;
   }
   else
   {
      sel->seg = RTR->next_M;
      RTR->next_M = (uint8_t *)RTR->next_M + bytes;
   }

   return 1;
}

/***************************************************/
//
// Initialize all entries in resource directory block
//
/***************************************************/

static void RTR_init_dir(RTR_class *RTR, uint32_t first)
{
   uint32_t i, j;

   for (i = 0, j = first; i < DIR_BLK; i++, j++)
   {
      RTR->dir[j].size = 0;
      RTR->dir[j].flags = DA_FREE | DA_DISCARDED;
      RTR->dir[j].history = 0;
      RTR->dir[j].locks = 0;
      RTR->dir[j].user = UINT32_MAX;
      RTR->dir[j].seg = 0;
   }
}

/***************************************************/
//
// Return next available entry (-1 if error) in resource
// directory, expanding directory if necessary
//
/***************************************************/

static uint32_t RTR_new_entry(RTR_class *RTR)
{
   uint32_t i, f, n;
   uint32_t nbytes;
   void *dest, *src;

   n = RTR->nentries;

   if (n >= (unsigned)(-DIR_BLK))
      return UINT32_MAX;

   for (i = 0; i < n; i++)
   {
      f = RTR->dir[i].flags;

      if ((f & DA_FREE) && (f & DA_DISCARDED))
         return (uint32_t)&RTR->dir[i];
   }

   if (!RTR_make_room(RTR, SIZE_DB))
      return UINT32_MAX;

   for (i = 0; i < n; i++)
   {
      if (RTR->dir[i].flags & (DA_DISCARDED | DA_FIXED))
         continue;
      RTR->dir[i].seg = ((int8_t *)RTR->dir[i].seg) + SIZE_DB;
   }

   src = add_ptr(RTR->dir, n * sizeof(HD_entry));
   dest = add_ptr(src, SIZE_DB);
   nbytes = ptr_dif(RTR->next_M, src);

   // PollMod(); // Tom: commented out
   far_memmove(dest, src, nbytes);

   RTR->next_M = (int8_t *)RTR->next_M + SIZE_DB;
   RTR->free -= SIZE_DB;
   RTR->nentries += DIR_BLK;

   RTR_init_dir(RTR, n);

   return (uint32_t)&RTR->dir[n];
}

/***************************************************/
//
// Read resource header to get data attribs & len, return w/file
// pointer positioned at start of resource data
//
// Maintain copy of current ordinal directory block as part of
// RTR structure to reduce excessive seeking
//
// Return size of resource in bytes
//
// Return 0 if resource was created by reference and never
// subsequently written (PLACEHOLDER)
//
/***************************************************/

uint32_t RTR_seek(RTR_class *RTR, uint32_t rnum)
{
   uint32_t dirblk, next;
   uint16_t dirent;

   dirblk = (rnum / (uint32_t)OD_SIZE);
   dirent = (uint16_t)(rnum % (uint32_t)OD_SIZE);

   if (RTR->cur_blk != dirblk)
   {
      RTR->cur_blk = dirblk;

      next = RTR->RFH.FOB;

      do
      {
         lseek(RTR->file, next, SEEK_SET);
         read(RTR->file, &RTR->OD, sizeof(OD_block));
         next = RTR->OD.next;

         // PollMod(); // Tom: commented out
      } while (dirblk--);
   }

   lseek(RTR->file, RTR->OD.index[dirent], SEEK_SET);
   read(RTR->file, &RTR->REH, sizeof(RF_entry_hdr));

   if (RTR->REH.data_attrib & DA_PLACEHOLDER)
      return 0;

   return RTR->REH.data_size;
}

/***************************************************/
//
// Read resource image into cache at entry /entry/
//
// Assumes resource file pointer -> start at resource data
//
/***************************************************/

static void RTR_read(RTR_class *RTR, uint32_t entry)
{
   uint8_t *ptr;
   HD_entry *sel;
   uint32_t len;

   sel = (HD_entry *)entry;

   ptr = sel->seg;
   len = sel->size;

   while (len > (uint32_t)DOS_BUFFSIZE)
   {
      // PollMod(); // Tom: commented out

      read(RTR->file, ptr, DOS_BUFFSIZE);
      len -= DOS_BUFFSIZE;
      ptr += DOS_BUFFSIZE;
   }
   read(RTR->file, ptr, len);

   // PollMod(); // Tom: commented out
}

/***************************************************/
//
// Create run-time resource cache class instance
//
//     base: Address in memory for resource cache (need not be aligned)
//
//     size: Cache size in bytes (need not be paragraph multiple)
//
//   nnames: Max. # of entries in cache's name directory
//
// filename: Name of resource file
//
/***************************************************/

RTR_class *RTR_construct(void *base, uint32_t size, uint32_t nnames, int8_t *filename)
{
   RTR_class *RTR;
   void *beg, *end;

   RTR = mem_alloc(sizeof(RTR_class));

   RTR->file = open((char *)filename, O_RDONLY | O_BINARY);
   if (RTR->file == -1)
      return NULL;

   read(RTR->file, &RTR->RFH, sizeof(RF_file_hdr));

   end = add_ptr(base, size);
   beg = base;

   RTR->base = base;

   RTR->nentries = DIR_BLK;
   RTR->dir = beg;

   RTR->next_M = add_ptr(beg, SIZE_DB);
   RTR->last_F = end;

   RTR->free = ptr_dif(RTR->last_F, RTR->next_M);

   RTR->LRU_cnt = 0;

   RTR_init_dir(RTR, 0);

   RTR->cur_blk = UINT32_MAX;

   //RTR->name_dir = RTR_alloc(RTR, (uint32_t)((uint32_t)nnames * sizeof(ND_entry)), DA_FIXED | DA_PRECIOUS); // Tom: commented out
   RTR->name_dir = RTR_alloc(RTR, nnames * sizeof(ND_entry), DA_FIXED | DA_PRECIOUS); // Tom: new version
   RTR->nd_entries = 0;

   return RTR;
}

/***************************************************/
//
// Destroy run-time resource cache class instance
//
// flags: See RTRES.H
//
/***************************************************/

void RTR_destroy(RTR_class *RTR, uint32_t flags)
{
   close(RTR->file);

   if (flags & RTR_FREEBASE)
   {
      mem_free(RTR->base);
   }

   mem_free(RTR);
}

/***************************************************/
//
// Allocate /bytes/ of memory in resource cache and return segment
// address of entry descriptor (handle)
//
// Initializes user field to -1
//
// Returns -1 if sufficient memory could not be allocated
//
// Currently only named resources (resource file entries) should be
// marked DISCARDABLE, since the client is not notified when a block
// is discarded
//
/***************************************************/

uint32_t RTR_alloc(RTR_class *RTR, uint32_t bytes, uint32_t attrib)
{
   uint32_t entry;
   HD_entry *sel;

   entry = RTR_new_entry(RTR);
   if (entry == UINT32_MAX)
      return UINT32_MAX;

   if (!RTR_assign_space(RTR, bytes, attrib, entry))
      return UINT32_MAX;

   sel = (HD_entry *)entry;
   sel->user = UINT32_MAX;

   return entry;
}

/***************************************************/
//
// Release cache memory previously allocated with RTR_alloc()
//
// Also frees name directory entry, if used
//
// If block being freed is last MOVABLE or first FIXED, and it hasn't
// already been discarded, discard it (along with any free neighbors
// if FIXED; this is the only way FIXED entries can ever be discarded at
// present)
//
/***************************************************/

void RTR_free(RTR_class *RTR, uint32_t entry)
{
   uint32_t i, n;
   HD_entry *sel;
   ND_entry *dir;

   if (entry == UINT32_MAX)
      return;

   sel = (HD_entry *)entry;

   sel->flags |= DA_FREE;
   sel->locks = 0;

   for (i = 0, dir = RTR_addr(RTR->name_dir); i < RTR->nd_entries; i++, dir++)
      if (dir->handle == entry)
         dir->handle = 0;
      else if (dir->thunk == entry)
         dir->thunk = UINT32_MAX;

   if (!(sel->flags & DA_FIXED))
   {
      while ((!(sel->flags & DA_FIXED)) &&
             (!(sel->flags & DA_DISCARDED)) &&
             ((uint32_t)sel->seg + sel->size == (uint32_t)RTR->next_M))
      {
         RTR_discard(RTR, (entry - (uint32_t)RTR->dir) / sizeof(HD_entry), 1);

         n = RTR->nentries;
         for (i = 0; i < n; i++)
            if ((!(RTR->dir[i].flags & DA_FIXED)) &&
                (RTR->dir[i].flags & DA_FREE) &&
                (!(RTR->dir[i].flags & DA_DISCARDED)) &&
                ((uint32_t)RTR->dir[i].seg + sel->size == (uint32_t)RTR->next_M))
            {
               sel = &(RTR->dir[i]);
               entry = (uint32_t)sel;
               break;
            }
      }
   }
   else
   {
      while ((sel->flags & DA_FIXED) && (!(sel->flags & DA_DISCARDED)) && (sel->seg == RTR->last_F))
      {
         RTR->last_F = (uint8_t *)RTR->last_F + sel->size;
         RTR->free += sel->size;
         sel->flags |= DA_DISCARDED;

         n = RTR->nentries;
         for (i = 0; i < n; i++)
            if ((RTR->dir[i].flags & DA_FIXED) && (RTR->dir[i].flags & DA_FREE) && (!(RTR->dir[i].flags & DA_DISCARDED)) && (RTR->dir[i].seg == RTR->last_F))
            {
               sel = &RTR->dir[i];
               break;
            }
      }
   }
}

/***************************************************/
//
// Discard entries until amount of space is available in cache
// (-1UL for maximum)
//
// Returns 1 on success, 0 if space could not be obtained
//
/***************************************************/

uint32_t RTR_force_discard(RTR_class *RTR, uint32_t goal)
{
   return RTR_make_room(RTR, goal);
}

/***************************************************/
//
// "Lock" a resource cache entry
//
// A locked resource cannot be automatically discarded until the lock count is
// decremented to zero via call(s) to RTR_unlock()
//
// Locking does NOT guarantee that the resource will not be moved when other
// resources are discarded, only that it will always be instantly accessible
// by dereferencing the handle segment pointer
//
/***************************************************/

void RTR_lock(RTR_class *RTR, uint32_t entry)
{
   uint32_t i, n;
   HD_entry *sel;

   // PollMod(); // Tom: commented out

   sel = (HD_entry *)entry;

   if (sel->flags & (DA_FIXED | DA_PRECIOUS | DA_FREE))
      return;

   if ((sel->flags & DA_DISCARDED) && (sel->user != UINT32_MAX))
   {
      if (RTR_assign_space(RTR, sel->size, sel->flags, entry) == UINT32_MAX)
         return;

#if FAST_LOCK
      lseek(RTR->file, sel->user, SEEK_SET);
#else
      RTR_seek(RTR, sel->user);
#endif

      RTR_read(RTR, entry);

      sel->flags &= (~DA_DISCARDED);
   }

   ++sel->locks;

   sel->history = ++RTR->LRU_cnt;

   if (RTR->LRU_cnt == 65535U)
   {
      n = RTR->nentries;
      for (i = 0; i < n; i++)
         RTR->dir[i].history >>= 3;

      RTR->LRU_cnt >>= 3;
   }
}

/***************************************************/
//
// "Unlock" a resource cache entry
//
// A locked resource cannot be automatically discarded until the lock count is
// decremented to zero via call(s) to RTR_unlock()
//
/***************************************************/

void RTR_unlock(uint32_t entry)
{
   HD_entry *sel;

   sel = (HD_entry *)entry;

   if (sel->locks > 0)
      --sel->locks;
}

/***************************************************/
//
// Return the size of a cached resource (not necessarily
// present)
//
/***************************************************/

uint32_t RTR_size(uint32_t entry)
{
   HD_entry *sel;

   sel = (HD_entry *)entry;

   return sel->size;
}

/***************************************************/
//
// Return the current address of a cached resource
//
// Addresses of FIXED resources may safely be stored and re-used at will
// by the client program, since FIXED resources are neither MOVEABLE nor
// (at least for the time being) DISCARDABLE.
//
// However, addresses of MOVEABLE resources may change any time a block of
// memory is allocated from the resource cache.  Additionally, DISCARDABLE
// resources MUST be locked via RTR_lock() before calling this function.
// Do not expect pointers to MOVEABLE resources to remain valid after
// any function which can load resources or otherwise allocate cache memory.
// The RTR_fix_segment() function may be used to adjust segment addresses
// of moveable resources.
//
// This function is normally implemented via a macro #defined in RTRES.H.
//
/***************************************************/

#ifndef RTR_addr

void *RTR_addr(uint32_t entry)
{
   return (void *)(*(uint32_t *)entry);
}

#endif

/***************************************************/
//
// Read from resource file into a specified block of memory
//
// RTR_seek() must first have been called to set the resource file
// pointer position to the desired resource
//
/***************************************************/

void RTR_read_resource(RTR_class *RTR, void *dest, uint32_t len)
{
   while (len > (uint32_t)DOS_BUFFSIZE)
   {
      read(RTR->file, dest, DOS_BUFFSIZE);
      len -= DOS_BUFFSIZE;
      dest = add_ptr(dest, DOS_BUFFSIZE);
   }
   read(RTR->file, dest, len);
}

/***************************************************/
//
// Allocate cache memory and load resource /resource/ with attributes
// /attrib/
//
// Use resource's default attributes if /attrib/ = DA_DEFAULT
//
// Normally this function should not be called to load named resources,
// since RTR_get_resource_handle() maintains a name directory to avoid
// redundantly loading named resources
//
// Returns -1U if resource could not be loaded
//
/***************************************************/

uint32_t RTR_load_resource(RTR_class *RTR, uint32_t resource, uint32_t attrib)
{
   HD_entry *sel;
   uint32_t entry;

   if (!RTR_seek(RTR, resource))
      return UINT32_MAX;

   entry = RTR_alloc(RTR, RTR->REH.data_size, (attrib == DA_DEFAULT) ? RTR->REH.data_attrib : attrib);

   if (entry != UINT32_MAX)
   {
      sel = (HD_entry *)entry;

#if FAST_LOCK
      // sel->user = tell(RTR->file);
      sel->user = lseek(RTR->file, 0, SEEK_CUR); // Tom: tell() replaced with lseek
#else
      sel->user = resource;
#endif

      RTR_read(RTR, entry);
   }

   return entry;
}

/***************************************************/
//
// Return handle to cached image of resource /resource/
// (or 0 if not valid resource)
//
// If /resource/ has not been loaded, load it and enter its
// handle in the (sorted) resource name directory for faster access
// in future calls to this function
//
// This is the preferred way to access most resources in the resource file
//
/***************************************************/

uint32_t RTR_get_resource_handle(RTR_class *RTR, uint32_t resource, uint32_t attrib)
{
   int32_t i, insert, replace;
   ND_entry *dir;
   void *dest, *src;
   uint32_t nbytes;
   printf("[TOM] RTR_get_resource_handle: resource=%u attrib=%u\n", resource, attrib);
   dir = RTR_search_name_dir(RTR, resource);

   if (dir == NULL)
   {
      dir = RTR_addr(RTR->name_dir);

      for (i = replace = insert = 0; i < RTR->nd_entries; i++)
      {
         if (dir->OE > resource)
         {
            insert = 1;
            break;
         }

         if (dir->OE == resource)
         {
            replace = 1;
            break;
         }

         dir++;
      }

      if (insert)
      {
         src = dir;
         dest = add_ptr(src, (uint32_t)sizeof(ND_entry));
         nbytes = (uint32_t)((RTR->nd_entries - i) * sizeof(ND_entry));

         // PollMod(); // Tom: commented out
         far_memmove(dest, src, nbytes);
      }

      if (!replace)
         ++RTR->nd_entries;

      dir->OE = resource;
      dir->handle = RTR_load_resource(RTR, resource, attrib);
      dir->thunk = UINT32_MAX;
   }

   if (dir->handle == UINT32_MAX)
      dir->handle = 0;

   return dir->handle;
}

/***************************************************/
//
// Free a resource by name
//
// This function allows resource cache entries to be freed based on
// the resource ordinal entry number (or "name") of the cached entry,
// rather than the entry's handle
//
/***************************************************/

void RTR_free_resource(RTR_class *RTR, uint32_t resource)
{
   ND_entry *dir;

   dir = RTR_search_name_dir(RTR, resource);

   if (dir == NULL)
      return;

   RTR_free(RTR, dir->handle);
}

/***************************************************/
//
// Perform binary search of name directory for specified resource
//
// Return pointer to name directory entry or NULL if entry
// deleted or not found
//
/***************************************************/

ND_entry *RTR_search_name_dir(RTR_class *RTR, uint32_t resource)
{
   int32_t min, max, mid;
   ND_entry *dir, *try;
   uint32_t entry;

   dir = RTR_addr(RTR->name_dir);

   min = 0;
   max = RTR->nd_entries - 1;

   while (min <= max)
   {
      mid = (min + max) >> 1;

      try = &dir[mid];

      entry = try->OE;

      if (entry > resource)
         max = mid - 1;
      else if (entry < resource)
         min = mid + 1;
      else
         return (try->handle == 0) ? NULL : try;
   }

   return NULL;
}

#if 0

/***************************************************/
//
// Show ASCII resource name
//
/***************************************************/

int8_t *ASCII_name(uint32_t name)
{
   extern uint32_t HROED;
   static int8_t aname[35];
   uint8_t *dict;
   int8_t *tag, *def;
   uint32_t num;
   int32_t i;

   dict = RTD_first(RTR_addr(HROED));
   while ((dict = RTD_iterate(RTR_addr(HROED), dict, &tag, &def)) != NULL)
   {
      num = ascnum(def);

      if (num == name)
         break;
   }

   if (dict == NULL)
      sprintf(aname, "Not found (%ld)", name);
   else
      strncpy(aname, tag, 34);

   for (i = strlen(aname); i < 34; i++)
      aname[i] = ' ';

   aname[34] = 0;

   return aname;
}

/***************************************************/
//
// Show resource cache stats (for diagnostic purposes)
//
/***************************************************/

void RTR_dump(RTR_class *RTR)
{
   uint32_t i, j, f;
   HD_entry R;
   uint32_t cs;
   uint8_t *ptr;
   uint32_t cnt;
   extern uint32_t heap_size;
   uint32_t present;

   fprintf(stdout, " # of entries: %u\n", RTR->nentries);
   fprintf(stdout, "         Size: %lu\n", heap_size);
   fprintf(stdout, "         Base: %Fp\n\n", RTR->base);

   fprintf(stdout, "Next moveable: %Fp\n", RTR->next_M);
   fprintf(stdout, "   Last fixed: %Fp\n", RTR->last_F);
   fprintf(stdout, "         Free: %lu\n\n", RTR->free);

   present = 0;

   for (i = 0; i < RTR->nentries; i++)
   {
      f = RTR->dir[i].flags;

      if ((f & DA_FREE) && (f & DA_DISCARDED) && (!RTR->dir[i].seg))
         break;
   }

   fprintf(stdout, "Entries avail: %u\n", RTR->nentries);
   fprintf(stdout, "       In use: %u\n\n", i);

   for (j = 0; j < i; j++)
   {
      R = RTR->dir[j];

      for (cnt = 0, cs = 0, ptr = RTR_addr(norm(&RTR->dir[j]));
           cnt < R.size; cnt++)
         cs += ptr[cnt];

      fprintf(stdout, "%s %.06lu bytes, lock %.02d, LRU %.05u) ",
              ASCII_name(R.user), R.size, R.locks, R.history);

      if (R.flags & DA_FIXED)
         fprintf(stdout, "Fix ");
      else
         fprintf(stdout, "Mov ");

      if (R.flags & DA_PRECIOUS)
         fprintf(stdout, "Prec ");
      if (R.flags & DA_DISCARDABLE)
         fprintf(stdout, "D'ble ");
      if (R.flags & DA_EVANESCENT)
         fprintf(stdout, "Ev ");

      if (R.flags & DA_DISCARDED)
         fprintf(stdout, "DISC ");
      if (R.flags & DA_FREE)
         fprintf(stdout, "FREE ");

      if (!(R.flags & (DA_DISCARDED | DA_FREE)))
         present += R.size;

      fprintf(stdout, "\n");
   }

   fprintf(stdout, "\n%lu bytes present\n", present);
}

#endif
