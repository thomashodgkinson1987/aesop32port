// ############################################################################
// ##                                                                        ##
// ##  SOUND32.C                                                             ##
// ##                                                                        ##
// ##  AESOP sound system code resources for Eye III engine                  ##
// ##                                                                        ##
// ##  Version: 1.00 of 6-May-92 -- Initial version                          ##
// ##                                                                        ##
// ##  Project: Eye III                                                      ##
// ##   Author: John Miles                                                   ##
// ##                                                                        ##
// ##  C source compatible with Watcom C v9.0 or later                       ##
// ##  Flat memory model (32-bit DOS)                                        ##
// ##                                                                        ##
// ############################################################################
// ##                                                                        ##
// ##  Copyright (C) 1993 Miles Design, Inc.                                 ##
// ##                                                                        ##
// ##  Miles Design, Inc.                                                    ##
// ##  6702 Cat Creek Trail                                                  ##
// ##  Austin, TX 78731                                                      ##
// ##                                                                        ##
// ##  (512) 345-2642 / BBS (512) 454-9990 / FAX (512) 338-9630              ##
// ##                                                                        ##
// ############################################################################

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "mouse.h"
#include "vfx.h"

#include "defs.h"
#include "rtsystem.h"
#include "rtres.h"
#include "rtlink.h"
#include "rt.h"
#include "rtmsg.h"
#include "sound.h"
#include "graphics.h"

#define CFG_FN "SOUND.CFG"
#define GTL_PFX "STDPATCH."

#define ROLAND_DRV_NAME "A32MT32.DLL"
#define PCSPKR_DRV_NAME "A32SPKR.DLL"
#define ADLIB_DRV_NAME "A32ADLIB.DLL"
#define SBDIG_DRV_NAME "A32SBDG.DLL"

#define EMSHCNT 15       // 15 EMS handles for 64K sound blocks
#define XMI_BUFSIZE 2048 // Size of reserved XMIDI sequence buffer

#define XMID_LA 0 // LAPC-1/MT-32 in use
#define XMID_AD 1 // Ad Lib in use
#define XMID_PC 2 // PC speaker in use

uint8_t *PCM_storage; // "Simulated EMS" memory for flat-model SFX

int16_t XMI_device_type; // _LA, _AD, or _PC

int16_t PCM_active = 0;
int16_t XMI_active = 0;

int16_t sound_on;
int16_t music_resident;

// drvr_desc XMI_desc; // Tom: commented out

int16_t GTL = -1;

// HDRIVER hXMI, hPCM; // Tom: commented out
// HSEQUENCE hSEQ; // Tom: commented out

int8_t XMI_fn[32];

void *XMI_driver;
uint32_t hXMI_buffer;
uint32_t hXMI_state;
uint32_t hXMI_cache;

uint16_t EMS_offset[EMSHCNT]; // First free byte in each 64K block

int16_t SND_blk[64];   // EMS block of sound effect at index #
uint16_t SND_off[64];  // EMS offset of sound effect at index #
uint16_t SND_size[64]; // Size of sound effect at index #

struct // SSI MEL sound system config file
{
   int16_t XMI_IO;
   int16_t XMI_IRQ;
   int16_t XMI_DMA;
   int16_t XMI_DRQ;
   int16_t XMI_CARDTYPE;

   int16_t PCM_IO;
   int16_t PCM_IRQ;
   int16_t PCM_DMA;
   int16_t PCM_DRQ;
   int16_t PCM_CARDTYPE;

   int16_t PCM_ENABLED;

   int8_t XMI_fn[14];
   int8_t PCM_fn[14];

   int8_t dummy[32];
} MEL;

/****************************************************************************/
//
// Load a .DLL sound driver
//
/****************************************************************************/

static void *load_driver(int8_t *filename)
{
   (void)filename;

   printf("[STUB] [sound32] load_driver: filename=\n"); // Tom: TODO add filename to printout

   return NULL;

   // void *DLL, *drvr;

   // DLL = FILE_read(filename, NULL);

   // if (DLL == NULL)
   //    return NULL;

   // drvr = DLL_load(DLL, DLLMEM_ALLOC | DLLSRC_MEM, NULL);

   // free(DLL);

   // if (drvr == NULL)
   //    return NULL;

   // return drvr;
}

/****************************************************************************/
//
// Standard routine for Global Timbre Library access
//
/****************************************************************************/

static void *load_global_timbre(uint32_t bank, uint32_t patch)
{
   (void)bank;
   (void)patch;

   printf("[STUB] [sound32] load_global_timbre: bank= patch=\n"); // Tom: TODO add bank and patch

   return NULL;

   // uint16_t *timb_ptr;
   // static uint16_t len;

   // static struct // GTL file header entry structure
   // {
   //    int8_t patch;
   //    int8_t bank;
   //    uint32_t offset;
   // } GTL_hdr;

   // if (GTL == -1)
   //    return NULL; // if no GTL, return failure

   // lseek(GTL, 0, SEEK_SET); // else rewind to GTL header

   // do // search file for requested timbre
   // {
   //    read(GTL, &GTL_hdr, sizeof(GTL_hdr));
   //    if ((int)(GTL_hdr.bank) == -1)
   //       return NULL; // timbre not found, return NULL
   // } while ((GTL_hdr.bank != bank) ||
   //          (GTL_hdr.patch != patch));

   // lseek(GTL, GTL_hdr.offset, SEEK_SET);
   // read(GTL, &len, 2); // timbre found, read its length

   // timb_ptr = mem_alloc(len); // allocate memory for timbre ..
   // *timb_ptr = len;
   // // and load it
   // read(GTL, (timb_ptr + 1), len - 2);

   // return timb_ptr; // else return pointer to timbre
}

/****************************************************************************/
//
// Load a block of sound resources into the EMS range specified as starting
// and ending block handles (0-14)
//
// In practice, the valid block handles are:
//
// 0: COMMON sounds take blocks 0-8
// 9: LEVEL sounds can use blocks 9-15
//
// *array -> list of sound resource names to be loaded, terminated with a
// null entry
//
// This function must not perform any resource cache manipulation, since
// it maintains a pointer to an array in movable memory!
//
/****************************************************************************/

void load_sound_block(int32_t argcnt, uint32_t first_block, uint32_t last_block, uint32_t *array)
{
   (void)argcnt;
   (void)first_block;
   (void)last_block;
   (void)array;

   printf("[STUB] [sound32] load_sound_block: argcnt= first_block= last_block= array=\n"); // Tom: TODO add parameters to print out

   // uint32_t index;
   // uint32_t i, cur;
   // uint32_t size;
   // uint32_t end;
   // uint32_t res;
   // (void)argcnt; // Tom: added

   // if (!PCM_active)
   //    return;

   // index = (first_block == BLK_COMMON) ? FIRST_COMMON : FIRST_LEVEL;

   // for (i = first_block; i <= last_block; i++)
   //    EMS_offset[i] = 0;

   // for (i = 0; (res = array[i]) != 0L; i++)
   // {
   //    size = RTR_seek(RTR, res);
   //    if (size == 0L)
   //       abend(MSG_SRNF);

   //    for (cur = first_block; cur <= last_block; cur++)
   //    {
   //       end = (uint32_t)EMS_offset[cur] + size - 1L;

   //       if (end < 65520L)
   //          break;
   //    }

   //    if (cur > last_block)
   //       abend(MSG_OOSSE);

   //    SND_blk[index] = cur; // EMS_handle[cur];
   //    SND_off[index] = EMS_offset[cur];
   //    SND_size[index] = (uint16_t)size;

   //    RTR_read_resource(RTR, PCM_storage + EMS_offset[cur] + (cur * 65536), size);

   //    EMS_offset[cur] += (uint16_t)((size + 15L) & ~15L);

   //    index++;
   // }
}

/****************************************************************************/
//
// Request a sound effect from the COMMON or LEVEL bank
//
/****************************************************************************/

void sound_effect(int32_t argcnt, uint32_t index)
{
   (void)argcnt;
   (void)index;

   printf("[STUB] [sound32] sound_effect: argcnt= index=\n"); // Tom: TODO add prints

   // int16_t ch;
   // (void)argcnt; // Tom: added

   // if (!PCM_active)
   //    return;
   // if (!sound_on)
   //    return;

   // for (ch = 0; ch < PHYSICAL; ch++)
   //    if (!PhysicalState(ch))
   //       break;

   // if (ch == PHYSICAL)
   //    return;

   // SetChannel(SND_blk[index], SND_off[index], SND_size[index], ch, 1);

   // SetActive(ch, ch);
   // ChannelOn(ch);
}

/****************************************************************************/
//
// Play an XMIDI sequence, stopping any currently active sequence first
//
/****************************************************************************/

void play_sequence(int32_t argcnt, uint32_t LA_version, uint32_t AD_version, uint32_t PC_version)
{
   (void)argcnt;
   (void)LA_version;
   (void)AD_version;
   (void)PC_version;

   printf("[STUB] [sound32] play_sequence: argcnt= LA_version= AD_version= PC_version=\n"); // Tom: TODO add prints

   // uint32_t XMI_res;
   // uint32_t size;
   // uint32_t bank, patch, treq;
   // void *timb;
   // (void)argcnt; // Tom: added

   // if (!XMI_active)
   //    return;
   // if (!music_resident)
   //    return;
   // if (!sound_on)
   //    return;

   // switch (XMI_device_type)
   // {
   // case XMID_LA:
   //    XMI_res = LA_version;
   //    break;
   // case XMID_PC:
   //    XMI_res = PC_version;
   //    break;
   // default:
   //    XMI_res = AD_version;
   //    break;
   // }

   // if (hSEQ != -1)
   // {
   //    if (AIL_sequence_status(hXMI, hSEQ) != SEQ_DONE)
   //       AIL_stop_sequence(hXMI, hSEQ);

   //    AIL_release_sequence_handle(hXMI, hSEQ);
   // }

   // size = RTR_seek(RTR, XMI_res);
   // RTR_read_resource(RTR, RTR_addr(hXMI_buffer), size);

   // hSEQ = AIL_register_sequence(hXMI, RTR_addr(hXMI_buffer), 0,
   //                              RTR_addr(hXMI_state), NULL);

   // while ((treq = AIL_timbre_request(hXMI, hSEQ)) != -1U)
   // {
   //    bank = treq / 256;
   //    patch = treq % 256;

   //    timb = load_global_timbre(bank, patch);
   //    if (timb != NULL)
   //    {
   //       AIL_install_timbre(hXMI, bank, patch, timb);
   //       mem_free(timb);
   //    }
   //    else
   //       abend(MSG_TPNF, bank, patch);
   // }

   // AIL_start_sequence(hXMI, hSEQ);
}

/****************************************************************************/
//
// Load and initialize music system in preparation for sequence playback
//
/****************************************************************************/

void load_music(void)
{
   printf("[STUB] [sound32] load_music\n");

   // int32_t tsize;

   // if ((!XMI_active) || (!sound_on) || (music_resident))
   //    return;

   // XMI_driver = load_driver(XMI_fn);

   // hXMI = AIL_register_driver(XMI_driver);

   // if (!AIL_detect_device(hXMI, XMI_desc.default_IO, XMI_desc.default_IRQ, XMI_desc.default_DMA, XMI_desc.default_DRQ))
   // {
   //    mem_free(XMI_driver);
   //    return;
   // }

   // AIL_init_driver(hXMI, XMI_desc.default_IO, XMI_desc.default_IRQ, XMI_desc.default_DMA, XMI_desc.default_DRQ);

   // hXMI_state = RTR_alloc(RTR, AIL_state_table_size(hXMI), DA_FIXED | DA_PRECIOUS);

   // hXMI_buffer = RTR_alloc(RTR, XMI_BUFSIZE, DA_FIXED | DA_PRECIOUS);

   // hXMI_cache = -1;
   // tsize = AIL_default_timbre_cache_size(hXMI);

   // if (tsize)
   // {
   //    hXMI_cache = RTR_alloc(RTR, tsize, DA_FIXED | DA_PRECIOUS);
   //    AIL_define_timbre_cache(hXMI, RTR_addr(hXMI_cache), (uint16_t)tsize);
   // }

   // hSEQ = -1;
   // music_resident = 1;
}

/****************************************************************************/
//
// Shut down and unload music system to conserve heap resources during game
//
/****************************************************************************/

void unload_music(void)
{
   printf("[STUB] [sound32] unload_music\n");

   // int32_t i;

   // if ((!XMI_active) || (!music_resident))
   //    return;

   // if (hSEQ != -1)
   // {
   //    if (AIL_sequence_status(hXMI, hSEQ) != SEQ_DONE)
   //    {
   //       AIL_stop_sequence(hXMI, hSEQ);

   //       if (XMI_device_type == XMID_LA)
   //          for (i = 0; i < 60; i++)
   //             VFX_wait_vblank_leading();
   //    }

   //    AIL_release_sequence_handle(hXMI, hSEQ);
   //    hSEQ = -1;
   // }

   // AIL_shutdown_driver(hXMI, MSG_AIL);
   // AIL_release_driver_handle(hXMI);

   // if (hXMI_cache != -1U)
   //    RTR_free(RTR, hXMI_cache);

   // RTR_free(RTR, hXMI_buffer);
   // RTR_free(RTR, hXMI_state);
   // mem_free(XMI_driver);

   // music_resident = 0;
}

/****************************************************************************/
//
// Turn sound effects & music on/off
//
/****************************************************************************/

void set_sound_status(int32_t argcnt, uint32_t status)
{
   (void)argcnt;
   (void)status;

   printf("[STUB] [sound32] set_sound_status: argcnt= status=\n"); // Tom: TODO add prints

   // (void)argcnt; // Tom: added

   // if (!(PCM_active || XMI_active))
   //    return;

   // if (status)
   //    sound_on = 1;
   // else
   // {
   //    if ((XMI_active) && (music_resident))
   //       if (hSEQ != -1)
   //       {
   //          if (AIL_sequence_status(hXMI, hSEQ) != SEQ_DONE)
   //             AIL_stop_sequence(hXMI, hSEQ);

   //          AIL_release_sequence_handle(hXMI, hSEQ);
   //          hSEQ = -1;
   //       }

   //    if (PCM_active)
   //    {
   //       InActive(0);
   //       InActive(1);
   //       InActive(2);
   //       InActive(3);
   //    }

   //    sound_on = 0;
   // }
}

/****************************************************************************/
//
// Shut down audio resources and release all EMS memory used
//
// Note: Does not release resource cache blocks used for sound drivers, etc.
//
/****************************************************************************/

void shutdown_sound(void)
{
   printf("[STUB] [sound32] shutdown_sound\n");

   // if (!(PCM_active || XMI_active))
   //    return;

   // if (PCM_active)
   // {
   //    StopMod();

   //    AIL_shutdown_driver(hPCM, MSG_AIL);
   //    AIL_release_driver_handle(hPCM);
   // }

   // if (XMI_active)
   // {
   //    if (music_resident)
   //    {
   //       AIL_shutdown_driver(hXMI, MSG_AIL);
   //       AIL_release_driver_handle(hXMI);
   //    }

   //    if (GTL != -1)
   //       close(GTL);
   // }

   // PCM_active = XMI_active = music_resident = 0;
}

/****************************************************************************/
//
// Initialize audio resources
//
// Load requested drivers (specified in config file) and set global flags
// to indicate presence of PCM / XMI sound
//
// This routine uses printf() to report nonfatal errors, so it should be
// called before the graphics system is initialized unless the errprompt
// argument is set to 0 to inhibit informational messages
//
/****************************************************************************/

void init_sound(int32_t argcnt, uint32_t errprompt)
{
   (void)argcnt;
   (void)errprompt;

   printf("[STUB] [sound32] init_sound: argcnt= errprompt=\n"); // Tom: TODO add prints

   // sound_on = 1; // Tom: TODO see if this needs setting

   // int16_t PCM_requested, XMI_requested;
   // int8_t PCM_fn[32];
   // int8_t GTL_fn[32];
   // int16_t PCM_IO;
   // int16_t PCM_IRQ;
   // int16_t XMI_IO;
   // int8_t *PCMdrvr;
   // drvr_desc *desc;
   // void *sndwrk;
   // (void)argcnt; // Tom: added

   // if (PCM_active || XMI_active)
   //    return;

   // PCM_requested = 0;
   // XMI_requested = 0;
   // music_resident = 0;
   // GTL = -1;

   // if (FILE_read(CFG_FN, &MEL) == NULL)
   // {
   //    if (errprompt)
   //    {
   //       printf(MSG_NO_CFG);
   //       printf(MSG_SND_F);
   //       getch();
   //    }
   //    return;
   // }

   // MEL.XMI_fn[13] = 0;
   // MEL.PCM_fn[13] = 0;

   // if (!strnicmp(MEL.PCM_fn, "SB", 2))
   //    strcpy(MEL.PCM_fn, SBDIG_DRV_NAME);

   // if (!stricmp(MEL.XMI_fn, "ADLIB.ADV"))
   //    strcpy(MEL.XMI_fn, ADLIB_DRV_NAME);
   // else if (!stricmp(MEL.XMI_fn, "MT32MPU.ADV"))
   //    strcpy(MEL.XMI_fn, ROLAND_DRV_NAME);
   // else
   //    strcpy(MEL.XMI_fn, PCSPKR_DRV_NAME);

   // if (MEL.XMI_CARDTYPE != 113)
   // {
   //    strcpy(XMI_fn, MEL.XMI_fn);
   //    XMI_IO = MEL.XMI_IO;
   //    XMI_requested = 1;
   // }

   // if ((MEL.PCM_CARDTYPE != 113) && (MEL.PCM_ENABLED))
   // {
   //    strcpy(PCM_fn, MEL.PCM_fn);
   //    PCM_IO = MEL.PCM_IO;
   //    PCM_IRQ = MEL.PCM_IRQ;
   //    PCM_requested = 1;
   // }

   // if (PCM_requested)
   // {
   //    if ((PCM_storage = malloc(15 * 65536)) == NULL) // memory avail?
   //    {
   //       if (errprompt)
   //          printf(MSG_NO_EMS);
   //    }
   //    else
   //    {
   //       if ((PCMdrvr = load_driver(PCM_fn)) != NULL)
   //       {
   //          hPCM = AIL_register_driver(PCMdrvr);
   //          desc = AIL_describe_driver(hPCM);

   //          desc->default_IO = PCM_IO;
   //          desc->default_IRQ = PCM_IRQ;

   //          if (AIL_detect_device(hPCM, desc->default_IO, desc->default_IRQ,
   //                                desc->default_DMA, desc->default_DRQ))
   //          {
   //             AIL_init_driver(hPCM, desc->default_IO, desc->default_IRQ,
   //                             desc->default_DMA, desc->default_DRQ);

   //             sndwrk = RTR_addr(RTR_alloc(RTR, ModSizeNeeded(),
   //                                         DA_FIXED | DA_PRECIOUS));

   //             if (StartMod(hPCM, sndwrk, (char *)PCM_storage) != -1)
   //             {
   //                InActive(0);
   //                InActive(1);
   //                InActive(2);
   //                InActive(3);

   //                PCM_active = 1;
   //             }
   //          }
   //       }
   //    }
   // }

   // if (XMI_requested)
   // {
   //    if (!stricmp(XMI_fn, ROLAND_DRV_NAME))
   //       XMI_device_type = XMID_LA;
   //    else if (!stricmp(XMI_fn, PCSPKR_DRV_NAME))
   //       XMI_device_type = XMID_PC;
   //    else
   //       XMI_device_type = XMID_AD;

   //    if ((XMI_driver = load_driver(XMI_fn)) != NULL)
   //    {
   //       hXMI = AIL_register_driver(XMI_driver);
   //       desc = AIL_describe_driver(hXMI);

   //       desc->default_IO = XMI_IO;

   //       if (AIL_detect_device(hXMI, desc->default_IO, desc->default_IRQ, desc->default_DMA, desc->default_DRQ))
   //       {
   //          XMI_desc = *desc;

   //          strcpy(GTL_fn, GTL_PFX);
   //          strcat(GTL_fn, XMI_desc.data_suffix);

   //          GTL = open(GTL_fn, O_RDONLY | O_BINARY);

   //          XMI_active = 1;
   //       }

   //       AIL_release_driver_handle(hXMI);
   //    }

   //    mem_free(XMI_driver);
   // }

   // sound_on = 1;

   // if (errprompt && XMI_requested && (!XMI_active))
   // {
   //    printf(MSG_NO_XMI);
   //    printf(MSG_SND_F);
   //    getch();
   // }

   // if (errprompt && PCM_requested && (!PCM_active))
   // {
   //    printf(MSG_NO_PCM);
   //    printf(MSG_SND_F);
   //    getch();
   // }
}
