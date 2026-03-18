/*
 *  RT.C - AESOP Runtime Interpreter
 *  Ported from RT.ASM
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <execinfo.h>

// Tom: added (SDL)
#include <SDL2/SDL.h>
#include "globals.h"

#include "defs.h"
#include "rt.h"
#include "rtres.h"
#include "rtsystem.h"
#include "rtcode.h"

/*
 * VALUE Union - Represents a generic value on the interpreter stack.
 * In the original ASM, this was 4 bytes.
 */
typedef union
{
    void *addr_flat;
    struct
    {
        uint16_t addr_off;
        uint16_t addr_seg;
    };
    int32_t val;
    struct
    {
        uint16_t val_l;
        uint16_t val_h;
    };
} VALUE;

/* Global Interpreter State */
uint32_t current_this;
uint32_t current_msg;
uint32_t current_index;

/* Local State */
static RTR_class *RTR_ptr;
static uint8_t *stk_base;
static uint8_t *stk_off;
static uint32_t *objlist_ptr;

const char *case_list_strings[] = {
    "do_BRT",
    "do_BRF",
    "do_BRA",
    "do_CASE",
    "do_PUSH",
    "do_DUP",
    "do_NOT",
    "do_SETB",
    "do_NEG",
    "do_ADD",
    "do_SUB",
    "do_MUL",
    "do_DIV",
    "do_MOD",
    "do_EXP",
    "do_BAND",
    "do_BOR",
    "do_XOR",
    "do_BNOT",
    "do_SHL",
    "do_SHR",
    "do_LT",
    "do_LE",
    "do_EQ",
    "do_NE",
    "do_GE",
    "do_GT",
    "do_INC",
    "do_DEC",
    "do_SHTC",
    "do_INTC",
    "do_LNGC",
    "do_RCRS",
    "do_CALL",
    "do_SEND",
    "do_PASS",
    "do_JSR",
    "do_RTS",
    "do_AIM",
    "do_AIS",
    "do_LTBA",
    "do_LTWA",
    "do_LTDA",
    "do_LETA",
    "do_LAB",
    "do_LAW",
    "do_LAD",
    "do_SAB",
    "do_SAW",
    "do_SAD",
    "do_LABA",
    "do_LAWA",
    "do_LADA",
    "do_SABA",
    "do_SAWA",
    "do_SADA",
    "do_LEAA",
    "do_LSB",
    "do_LSW",
    "do_LSD",
    "do_SSB",
    "do_SSW",
    "do_SSD",
    "do_LSBA",
    "do_LSWA",
    "do_LSDA",
    "do_SSBA",
    "do_SSWA",
    "do_SSDA",
    "do_LESA",
    "do_LXB",
    "do_LXW",
    "do_LXD",
    "do_SXB",
    "do_SXW",
    "do_SXD",
    "do_LXBA",
    "do_LXWA",
    "do_LXDA",
    "do_SXBA",
    "do_SXWA",
    "do_SXDA",
    "do_LEXA",
    "do_SXAS",
    "do_LECA",
    "do_SOLE",
    "do_END",
    "do_BRK"};

/* Dictionary Functions */

int8_t *RTD_lookup(uint32_t HRES, void *Key)
{
    uint8_t *dict = (uint8_t *)RTR_addr(HRES);
    uint32_t hash_size = *(uint16_t *)dict;

    uint8_t *esi = Key;
    uint32_t sum = 0;
    uint32_t tag_len = 1;

    /* Hash Function */
    while (1)
    {
        if (!esi[0])
            break;
        tag_len++;
        sum += esi[0];
        if (!esi[1])
            break;
        tag_len++;
        sum += esi[1];
        if (!esi[2])
            break;
        tag_len++;
        sum += esi[2];
        if (!esi[3])
            break;
        tag_len++;
        sum += esi[3];
        esi += 4;
    }

    uint32_t edx = sum % hash_size;
    uint32_t chain_offset = *(uint32_t *)(dict + 2 + edx * 4);

    if (chain_offset == 0)
    {
        printf("[rt] RTD_lookup: HRES=%u Key=%p return=NULL\n", HRES, Key);
        return NULL;
    }

    uint8_t *edi = dict + chain_offset;

    while (1)
    {
        uint32_t ecx = *(uint16_t *)edi;
        edi += 2;
        if (ecx == 0)
        {
            printf("[rt] RTD_lookup: HRES=%u Key=%p return=NULL\n", HRES, Key);
            return NULL;
        }

        if (ecx == tag_len)
        {
            if (memcmp(edi, Key, tag_len) == 0)
            {
                int8_t *def_ptr = (int8_t *)(edi + tag_len + 2);
                printf("[rt] RTD_lookup: HRES=%u Key=%s return=%s\n", HRES, (char *)Key, (char *)def_ptr);
                return def_ptr;
            }
        }

        edi += ecx;
        uint32_t def_len = *(uint16_t *)edi;
        edi += 2;
        edi += def_len;
    }
}

void *RTD_first(void *dictionary)
{
    uint8_t *esi = (uint8_t *)dictionary;
    uint32_t hash_size = *(uint16_t *)esi;

    esi += 2;
    esi += hash_size * 4;

    if (*(uint16_t *)esi == 0)
    {
        printf("[rt] RTD_first: dictionary=%p return=0\n", dictionary);
        return 0;
    }

    printf("[rt] RTD_first: dictionary=%p return=%p\n", dictionary, (void *)(esi - (uint8_t *)dictionary));
    return (void *)(esi - (uint8_t *)dictionary);
}

void *RTD_iterate(void *base, void *cur, int8_t **tag, int8_t **def)
{
    if (cur == 0)
    {
        printf("[rt] RTD_iterate: base=%p cur=%p tag=%p def=%p return=0\n", base, cur, (void *)tag, (void *)def);
        return 0;
    }

    uint8_t *esi = (uint8_t *)((uintptr_t)base + (uintptr_t)cur);
    uint32_t tag_len = *(uint16_t *)esi;

    esi += 2;

    if (tag_len == 0)
    {
        tag_len = *(uint16_t *)esi;
        esi += 2;
        if (tag_len == 0)
        {
            printf("[rt] RTD_iterate: base=%p cur=%p tag=%p def=%p return=0\n", base, cur, (void *)tag, (void *)def);
            return 0;
        }
    }

    *tag = (int8_t *)esi;
    esi += tag_len;

    uint32_t def_len = *(uint16_t *)esi;
    esi += 2;
    *def = (int8_t *)esi;
    esi += def_len;

    printf("[rt] RTD_iterate: base=%p cur=%p tag=%p def=%p return=%p\n", base, cur, (void *)tag, (void *)def, (void *)(esi - (uint8_t *)base));
    return (void *)(esi - (uint8_t *)base);
}

/* Subsystem Control Functions */

void RT_init(RTR_class *RTR, uint32_t StkSize, uint32_t *ObjectList)
{
    printf("[rt] RT_init: RTR=%p StkSize=%u ObjectList=%p\n", (void *)RTR, StkSize, (void *)ObjectList);

    objlist_ptr = ObjectList;
    RTR_ptr = RTR;
    stk_base = (uint8_t *)mem_alloc(StkSize);
    stk_off = stk_base + StkSize;
}

void RT_shutdown(void)
{
    printf("[rt] RT_shutdown\n");

    mem_free(stk_base);
}

void RT_arguments(void *Base, uint32_t Bytes)
{
    printf("[rt] RT_arguments: Base=%p Bytes=%u\n", Base, Bytes);

    stk_off = stk_off - Bytes;
    memcpy(stk_off, Base, Bytes);
}

/* AESOP Runtime execution unit */

int32_t RT_execute(uint32_t Index, uint32_t Message, uint32_t Vector)
{
    uint32_t h_prg;
    // uint8_t **h_thunk; // Tom: removed, not used?
    uint8_t *ptr_thunk;
    void **h_instance;
    uint8_t *fptr;
    uint32_t static_offset;
    uint32_t extern_offset;
    uint32_t cur_vector;
    uint8_t *ds32;
    uint8_t *esi;
    VALUE *edi;

    printf("[rt] RT_execute: Index=%u Message=%u Vector=%u\n", Index, Message, Vector);

    /* Call Stack Simulation */
    struct CallFrame
    {
        uint8_t *ds32;
        uint8_t *esi;
        VALUE *edi;
        uint8_t *fptr;
    } call_stack[256];
    int call_stack_ptr = 0;

    if ((uint16_t)Index == 0xFFFF)
        return -1;
    current_index = Index;

    h_instance = (void **)objlist_ptr[Index];
    if (h_instance == (void **)-1)
        return -1;

    IHDR *inst = (IHDR *)*h_instance;

    ptr_thunk = (uint8_t *)RTR_addr(inst->thunk);

    THDR *thdr = (THDR *)ptr_thunk;
    MV_entry *mv_list;

    if (Vector != UINT32_MAX && Vector != 0xFFFF)
    {
        esi = ptr_thunk + Vector;
        goto __handle_msg;
    }

    uint32_t max_msg = thdr->max_msg;
    mv_list = (MV_entry *)(ptr_thunk + thdr->MV_list);

    if ((int16_t)max_msg == -1)
        return -1;

    uint32_t min = 0;
    uint32_t max = max_msg;
    current_msg = Message;

    while (1)
    {
        uint32_t mid = (min + max) / 2;
        MV_entry *entry = &mv_list[mid];

        if (entry->msg > Message)
        {
            if (mid == 0)
                return -1;
            max = mid - 1;
        }
        else if (entry->msg < Message)
        {
            min = mid + 1;
        }
        else
        {
            while (mid > 0 && mv_list[mid - 1].msg == Message)
            {
                mid--;
            }
            esi = (uint8_t *)&mv_list[mid];
            goto __handle_msg;
        }
        if (min > max)
            return -1;
    }

__handle_msg:
    cur_vector = (uint32_t)((uint8_t *)esi - ptr_thunk);
    MV_entry *curr_mv = (MV_entry *)esi;

    SD_entry *sd = (SD_entry *)(ptr_thunk + curr_mv->SD_offset);
    uint32_t handler_offset = curr_mv->handler;

    h_prg = sd->SOP;
    static_offset = sd->static_base;
    extern_offset = sd->extern_base;

    RTR_lock(RTR_ptr, h_prg);

    ds32 = (uint8_t *)RTR_addr(h_prg);
    esi = ds32 + handler_offset;

    fptr = stk_off;

    // Initialize manifest THIS var at fptr - 2 (matches RT.ASM OFF_THIS = 2)
    *(uint16_t *)(fptr - 2) = (uint16_t)Index;

    uint16_t auto_size = ((MHDR *)esi)->auto_size;
    // Set edi to first free stack location: fptr - auto_size - SIZE VALUE
    edi = (VALUE *)(fptr - auto_size - sizeof(VALUE));
    esi += sizeof(MHDR);

#define GET_WORD()                     \
    do                                 \
    {                                  \
        esi = ds32 + *(uint16_t *)esi; \
    } while (0)

    static bool quit = false;

    while (1 && !quit)
    {
        SDL_Event e;
        while (SDL_PollEvent(&e) != 0)
        {
            if (e.type == SDL_QUIT)
            {
                quit = true;
            }
            else if (e.type == SDL_KEYDOWN)
            {
                if (e.key.keysym.sym == SDLK_ESCAPE)
                {
                    quit = true;
                }
            }
        }

        SDL_SetRenderDrawColor(sdl_renderer, 0, 255, 0, 255);
        SDL_RenderClear(sdl_renderer);

        SDL_RenderPresent(sdl_renderer);

        uint8_t opcode = *esi++;

        printf("[rt] RT_execute: opcode=%u %s\n", opcode, case_list_strings[opcode]);

        switch (opcode)
        {
        case 0: // do_BRT
            if (edi->val != 0)
            {
                GET_WORD();
            }
            else
            {
                esi += 2;
            }
            break;
        case 1: // do_BRF
            if (edi->val == 0)
            {
                GET_WORD();
            }
            else
            {
                esi += 2;
            }
            break;
        case 2: // do_BRA
            GET_WORD();
            break;
        case 3: // do_CASE
        {
            int32_t val = edi->val;
            uint16_t num_cases = *(uint16_t *)esi;
            esi += 2;
            int found = 0;
            for (uint16_t i = 0; i < num_cases; i++)
            {
                if (val == *(int32_t *)esi)
                {
                    esi += 4;
                    GET_WORD();
                    found = 1;
                    break;
                }
                esi += 6;
            }
            if (!found)
            {
                GET_WORD();
            }
            break;
        }
        case 4: // do_PUSH
            edi--;
            edi->val = 0;
            break;
        case 5: // do_DUP
            edi--;
            edi[0].val = edi[1].val;
            break;
        case 6: // do_NOT
            edi->val = !edi->val;
            break;
        case 7: // do_SETB
            edi->val = (edi->val != 0) ? 1 : 0;
            break;
        case 8: // do_NEG
            edi->val = -edi->val;
            break;
        case 9: // do_ADD
            edi[1].val += edi[0].val;
            edi++;
            break;
        case 10: // do_SUB
            edi[1].val -= edi[0].val;
            edi++;
            break;
        case 11: // do_MUL
            edi[1].val *= edi[0].val;
            edi++;
            break;
        case 12: // do_DIV
            if (edi[0].val != 0)
                edi[1].val /= edi[0].val;
            edi++;
            break;
        case 13: // do_MOD
            if (edi[0].val != 0)
                edi[1].val %= edi[0].val;
            edi++;
            break;
        case 14: // do_EXP
        {
            uint16_t exp = edi->val_l;
            edi++;
            int32_t base = edi->val;
            int32_t res = 1;
            for (uint16_t i = 0; i < exp; i++)
                res *= base;
            edi->val = res;
            break;
        }
        case 15: // do_BAND
            edi[1].val &= edi[0].val;
            edi++;
            break;
        case 16: // do_BOR
            edi[1].val |= edi[0].val;
            edi++;
            break;
        case 17: // do_XOR
            edi[1].val ^= edi[0].val;
            edi++;
            break;
        case 18: // do_BNOT
            edi->val = ~edi->val;
            break;
        case 19: // do_SHL
            edi[1].val <<= edi[0].val_l;
            edi++;
            break;
        case 20: // do_SHR
            edi[1].val = (int32_t)((uint32_t)edi[1].val >> edi[0].val_l);
            edi++;
            break;
        case 21: // do_LT
            edi[1].val = (edi[1].val < edi[0].val) ? 1 : 0;
            edi++;
            break;
        case 22: // do_LE
            edi[1].val = (edi[1].val <= edi[0].val) ? 1 : 0;
            edi++;
            break;
        case 23: // do_EQ
            edi[1].val = (edi[1].val == edi[0].val) ? 1 : 0;
            edi++;
            break;
        case 24: // do_NE
            edi[1].val = (edi[1].val != edi[0].val) ? 1 : 0;
            edi++;
            break;
        case 25: // do_GE
            edi[1].val = (edi[1].val >= edi[0].val) ? 1 : 0;
            edi++;
            break;
        case 26: // do_GT
            edi[1].val = (edi[1].val > edi[0].val) ? 1 : 0;
            edi++;
            break;
        case 27: // do_INC
            edi->val++;
            break;
        case 28: // do_DEC
            edi->val--;
            break;
        case 29: // do_SHTC
            edi->val = *esi++;
            break;
        case 30: // do_INTC
            edi->val = *(uint16_t *)esi;
            esi += 2;
            break;
        case 31: // do_LNGC
            edi->val = *(int32_t *)esi;
            esi += 4;
            break;
        case 32: // do_RCRS
        {
            uint16_t index = *(uint16_t *)esi;
            esi += 2;
            uint32_t ebx = index + extern_offset;
            void *code_addr = *(void **)(ptr_thunk + ebx);
            edi->addr_flat = code_addr;

            printf("  RCRS index=%u function: ", index);
            backtrace_symbols_fd(&code_addr, 1, 1);

            break;
        }
        case 33: // do_CALL
        {
            current_this = *(uint16_t *)(fptr - 2);
            uint8_t arg_count = *esi++;
            uint32_t args[256];
            for (int i = 0; i < arg_count; i++)
            {
                args[i] = edi[i].val;
            }

            void *func_ptr = edi[arg_count].addr_flat;

            // printf("  CALL function: ");
            // backtrace_symbols_fd(&func_ptr, 1, 1);

            int32_t ret_val = 0;

            /* Unrolled call dispatcher to emulate generic stack call */
            stk_off = (uint8_t *)edi;
            switch (arg_count)
            {
            case 0:
                // ret_val = ((int32_t (*)(int32_t))func_ptr)(arg_count); // Tom: broken version?
                ret_val = ((int32_t (*)(void))func_ptr)(); // Tom: new version?
                break;
            case 1:
                ret_val = ((int32_t (*)(int32_t, int32_t))func_ptr)(arg_count, args[0]);
                break;
            case 2:
                ret_val = ((int32_t (*)(int32_t, int32_t, int32_t))func_ptr)(arg_count, args[1], args[0]);
                break;
            case 3:
                ret_val = ((int32_t (*)(int32_t, int32_t, int32_t, int32_t))func_ptr)(arg_count, args[2], args[1], args[0]);
                break;
            case 4:
                ret_val = ((int32_t (*)(int32_t, int32_t, int32_t, int32_t, int32_t))func_ptr)(arg_count, args[3], args[2], args[1], args[0]);
                break;
            case 5:
                ret_val = ((int32_t (*)(int32_t, int32_t, int32_t, int32_t, int32_t, int32_t))func_ptr)(arg_count, args[4], args[3], args[2], args[1], args[0]);
                break;
            case 6:
                ret_val = ((int32_t (*)(int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t))func_ptr)(arg_count, args[5], args[4], args[3], args[2], args[1], args[0]);
                break;
            case 7:
                ret_val = ((int32_t (*)(int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t))func_ptr)(arg_count, args[6], args[5], args[4], args[3], args[2], args[1], args[0]);
                break;
            case 8:
                ret_val = ((int32_t (*)(int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t))func_ptr)(arg_count, args[7], args[6], args[5], args[4], args[3], args[2], args[1], args[0]);
                break;
            case 9:
                ret_val = ((int32_t (*)(int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t))func_ptr)(arg_count, args[8], args[7], args[6], args[5], args[4], args[3], args[2], args[1], args[0]);
                break;
            case 10:
                ret_val = ((int32_t (*)(int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t))func_ptr)(arg_count, args[9], args[8], args[7], args[6], args[5], args[4], args[3], args[2], args[1], args[0]);
                break;
            case 11:
                ret_val = ((int32_t (*)(int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t))func_ptr)(arg_count, args[10], args[9], args[8], args[7], args[6], args[5], args[4], args[3], args[2], args[1], args[0]);
                break;
            case 12:
                ret_val = ((int32_t (*)(int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t))func_ptr)(arg_count, args[11], args[10], args[9], args[8], args[7], args[6], args[5], args[4], args[3], args[2], args[1], args[0]);
                break;
            case 13:
                ret_val = ((int32_t (*)(int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t))func_ptr)(arg_count, args[12], args[11], args[10], args[9], args[8], args[7], args[6], args[5], args[4], args[3], args[2], args[1], args[0]);
                break;
            case 14:
                ret_val = ((int32_t (*)(int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t))func_ptr)(arg_count, args[13], args[12], args[11], args[10], args[9], args[8], args[7], args[6], args[5], args[4], args[3], args[2], args[1], args[0]);
                break;
            case 15:
                ret_val = ((int32_t (*)(int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t))func_ptr)(arg_count, args[14], args[13], args[12], args[11], args[10], args[9], args[8], args[7], args[6], args[5], args[4], args[3], args[2], args[1], args[0]);
                break;
            case 16:
                ret_val = ((int32_t (*)(int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t))func_ptr)(arg_count, args[15], args[14], args[13], args[12], args[11], args[10], args[9], args[8], args[7], args[6], args[5], args[4], args[3], args[2], args[1], args[0]);
                break;
            case 17:
                ret_val = ((int32_t (*)(int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t))func_ptr)(arg_count, args[16], args[15], args[14], args[13], args[12], args[11], args[10], args[9], args[8], args[7], args[6], args[5], args[4], args[3], args[2], args[1], args[0]);
                break;
            case 18:
                ret_val = ((int32_t (*)(int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t))func_ptr)(arg_count, args[17], args[16], args[15], args[14], args[13], args[12], args[11], args[10], args[9], args[8], args[7], args[6], args[5], args[4], args[3], args[2], args[1], args[0]);
                break;
            }
            stk_off = (uint8_t *)fptr;

            edi += arg_count;   // Correct: edi now points to func_ptr slot
            edi->val = ret_val; // Return value overwrites func_ptr slot

            /* Re-derive DS32 in case of resource move */
            uint8_t *new_ds32 = (uint8_t *)RTR_addr(h_prg);
            esi = esi - ds32 + new_ds32;
            ds32 = new_ds32;
            break;
        }
        case 34: // do_SEND
        {
            uint8_t arg_count = *esi;
            VALUE *new_stack = edi - arg_count;
            for (int i = 0; i < arg_count; i++)
            {
                new_stack[arg_count - 1 - i].val = edi[i].val;
            }
            edi += arg_count;

            stk_off = (uint8_t *)new_stack;
            uint32_t instance_handle = edi->val;
            uint16_t msg_num = *(uint16_t *)(esi + 1);

            int32_t ret_val = RT_execute(instance_handle, msg_num, UINT32_MAX);

            stk_off = (uint8_t *)fptr;
            edi->val = ret_val; // Overwrite instance handle with return value
            esi += 3;

            uint8_t *new_ds32 = (uint8_t *)RTR_addr(h_prg);
            esi = esi - ds32 + new_ds32;
            ds32 = new_ds32;
            break;
        }
        case 35: // do_PASS
        {
            uint8_t arg_count = *esi++;
            uint32_t next_vector = cur_vector + sizeof(MV_entry);
            int32_t ret_val = 0;
            VALUE *ebx = edi;

            if (next_vector < thdr->SD_list)
            {
                MV_entry *next_mv = (MV_entry *)(ptr_thunk + next_vector);
                if (next_mv->msg == Message)
                {
                    for (int i = 0; i < arg_count; i++)
                    {
                        ebx--;
                        ebx->val = edi->val;
                        edi++;
                    }
                    stk_off = (uint8_t *)ebx;
                    ret_val = RT_execute(Index, Message, next_vector);
                    stk_off = (uint8_t *)fptr;
                }
                else
                {
                    edi += arg_count;
                }
            }
            else
            {
                edi += arg_count;
            }

            edi->val = ret_val;

            uint8_t *new_ds32 = (uint8_t *)RTR_addr(h_prg);
            esi = esi - ds32 + new_ds32;
            ds32 = new_ds32;
            break;
        }
        case 36: // do_JSR
        {
            uint16_t target = *(uint16_t *)esi;
            esi += 2;

            call_stack[call_stack_ptr].ds32 = ds32;
            call_stack[call_stack_ptr].esi = esi;
            call_stack[call_stack_ptr].edi = edi;
            call_stack[call_stack_ptr].fptr = fptr;
            call_stack_ptr++;

            esi = ds32 + target;
            fptr = (uint8_t *)edi;
            *(uint16_t *)(fptr - 2) = (uint16_t)Index;

            uint16_t auto_sz = ((MHDR *)esi)->auto_size;
            edi = (VALUE *)(fptr - auto_sz - sizeof(VALUE));
            esi += sizeof(MHDR);
            break;
        }
        case 37: // do_RTS
        {
            int32_t ret_val = edi->val;

            call_stack_ptr--;
            fptr = call_stack[call_stack_ptr].fptr;
            edi = call_stack[call_stack_ptr].edi;
            esi = call_stack[call_stack_ptr].esi;
            ds32 = call_stack[call_stack_ptr].ds32;

            edi->val = ret_val;

            /* Re-derive DS32 in case of resource move */
            uint8_t *new_ds32 = (uint8_t *)RTR_addr(h_prg);
            esi = esi - ds32 + new_ds32;
            ds32 = new_ds32;
            break;
        }
        case 38: // do_AIM
        {
            uint32_t max_index = edi->val_l;
            uint16_t mul_val = *(uint16_t *)esi;
            esi += 2;
            edi++;
            edi->val += max_index * mul_val;
            break;
        }
        case 39: // do_AIS
        {
            uint32_t max_index = edi->val_l;
            uint8_t shift_val = *esi++;
            edi++;
            edi->val += (max_index << shift_val);
            break;
        }
        case 40: // do_LTBA
        {
            uint16_t offset = *(uint16_t *)esi;
            esi += 2;
            edi->val = (int8_t)ds32[offset + (int16_t)edi->val_l];
            break;
        }
        case 41: // do_LTWA
        {
            uint16_t offset = *(uint16_t *)esi;
            esi += 2;
            edi->val = (int16_t)*(uint16_t *)(ds32 + offset + (int16_t)edi->val_l);
            break;
        }
        case 42: // do_LTDA
        {
            uint16_t offset = *(uint16_t *)esi;
            esi += 2;
            edi->val = *(int32_t *)(ds32 + offset + (int16_t)edi->val_l);
            break;
        }
        case 43: // do_LETA
        {
            uint16_t offset = *(uint16_t *)esi;
            esi += 2;
            edi->addr_flat = ds32 + offset;
            break;
        }
        case 44: // do_LAB
        {
            int16_t offset = *(int16_t *)esi;
            esi += 2;
            edi->val = (int8_t)*((uint8_t *)fptr - offset);
            break;
        }
        case 45: // do_LAW
        {
            int16_t offset = *(int16_t *)esi;
            esi += 2;
            edi->val = (int16_t)*(uint16_t *)((uint8_t *)fptr - offset);
            break;
        }
        case 46: // do_LAD
        {
            int16_t offset = *(int16_t *)esi;
            esi += 2;
            edi->val = *(int32_t *)((uint8_t *)fptr - offset);
            break;
        }
        case 47: // do_SAB
        {
            int16_t offset = *(int16_t *)esi;
            esi += 2;
            *((uint8_t *)fptr - offset) = (uint8_t)edi->val_l;
            break;
        }
        case 48: // do_SAW
        {
            int16_t offset = *(int16_t *)esi;
            esi += 2;
            *(uint16_t *)((uint8_t *)fptr - offset) = edi->val_l;
            break;
        }
        case 49: // do_SAD
        {
            int16_t offset = *(int16_t *)esi;
            esi += 2;
            *(uint32_t *)((uint8_t *)fptr - offset) = edi->val;
            break;
        }
        case 50: // do_LABA
        {
            int16_t offset = *(int16_t *)esi;
            esi += 2;
            edi->val = (int8_t)*((uint8_t *)fptr - offset + (int16_t)edi->val_l);
            break;
        }
        case 51: // do_LAWA
        {
            int16_t offset = *(int16_t *)esi;
            esi += 2;
            edi->val = (int16_t)*(uint16_t *)((uint8_t *)fptr - offset + (int16_t)edi->val_l);
            break;
        }
        case 52: // do_LADA
        {
            int16_t offset = *(int16_t *)esi;
            esi += 2;
            edi->val = *(int32_t *)((uint8_t *)fptr - offset + (int16_t)edi->val_l);
            break;
        }
        case 53: // do_SABA
        {
            int32_t val = edi->val;
            edi++;
            int16_t offset = *(int16_t *)esi;
            esi += 2;
            *((uint8_t *)fptr - offset + (int16_t)edi->val_l) = (uint8_t)val;
            edi->val = val;
            break;
        }
        case 54: // do_SAWA
        {
            int32_t val = edi->val;
            edi++;
            int16_t offset = *(int16_t *)esi;
            esi += 2;
            *(uint16_t *)((uint8_t *)fptr - offset + (int16_t)edi->val_l) = (uint16_t)val;
            edi->val = val;
            break;
        }
        case 55: // do_SADA
        {
            int32_t val = edi->val;
            edi++;
            int16_t offset = *(int16_t *)esi;
            esi += 2;
            *(int32_t *)((uint8_t *)fptr - offset + (int16_t)edi->val_l) = val;
            edi->val = val;
            break;
        }
        case 56: // do_LEAA
        {
            int16_t offset = *(int16_t *)esi;
            esi += 2;
            edi->addr_flat = (uint8_t *)fptr - offset;
            break;
        }
        case 57: // do_LSB
        {
            uint16_t offset = *(uint16_t *)esi;
            esi += 2;
            uint8_t *var_addr = (uint8_t *)RTR_addr(h_instance) + static_offset + offset;
            edi->val = (int8_t)*var_addr;
            break;
        }
        case 58: // do_LSW
        {
            uint16_t offset = *(uint16_t *)esi;
            esi += 2;
            uint8_t *var_addr = (uint8_t *)RTR_addr(h_instance) + static_offset + offset;
            edi->val = (int16_t)*(uint16_t *)var_addr;
            break;
        }
        case 59: // do_LSD
        {
            uint16_t offset = *(uint16_t *)esi;
            esi += 2;
            uint8_t *var_addr = (uint8_t *)RTR_addr(h_instance) + static_offset + offset;
            edi->val = *(int32_t *)var_addr;
            break;
        }
        case 60: // do_SSB
        {
            uint16_t offset = *(uint16_t *)esi;
            esi += 2;
            uint8_t *var_addr = (uint8_t *)RTR_addr(h_instance) + static_offset + offset;
            *var_addr = (uint8_t)edi->val_l;
            break;
        }
        case 61: // do_SSW
        {
            uint16_t offset = *(uint16_t *)esi;
            esi += 2;
            uint8_t *var_addr = (uint8_t *)RTR_addr(h_instance) + static_offset + offset;
            *(uint16_t *)var_addr = edi->val_l;
            break;
        }
        case 62: // do_SSD
        {
            uint16_t offset = *(uint16_t *)esi;
            esi += 2;
            uint8_t *var_addr = (uint8_t *)RTR_addr(h_instance) + static_offset + offset;
            *(int32_t *)var_addr = edi->val;
            break;
        }
        case 63: // do_LSBA
        {
            uint16_t offset = *(uint16_t *)esi;
            esi += 2;
            uint8_t *var_addr = (uint8_t *)RTR_addr(h_instance) + static_offset + offset;
            edi->val = (int8_t)var_addr[edi->val_l];
            break;
        }
        case 64: // do_LSWA
        {
            uint16_t offset = *(uint16_t *)esi;
            esi += 2;
            uint8_t *var_addr = (uint8_t *)RTR_addr(h_instance) + static_offset + offset;
            edi->val = (int16_t)*(uint16_t *)(var_addr + edi->val_l);
            break;
        }
        case 65: // do_LSDA
        {
            uint16_t offset = *(uint16_t *)esi;
            esi += 2;
            uint8_t *var_addr = (uint8_t *)RTR_addr(h_instance) + static_offset + offset;
            edi->val = *(int32_t *)(var_addr + edi->val_l);
            break;
        }
        case 66: // do_SSBA
        {
            int32_t val = edi->val;
            edi++;
            uint16_t offset = *(uint16_t *)esi;
            esi += 2;
            uint8_t *var_addr = (uint8_t *)RTR_addr(h_instance) + static_offset + offset;
            var_addr[edi->val_l] = (uint8_t)val;
            edi->val = val;
            break;
        }
        case 67: // do_SSWA
        {
            int32_t val = edi->val;
            edi++;
            uint16_t offset = *(uint16_t *)esi;
            esi += 2;
            uint8_t *var_addr = (uint8_t *)RTR_addr(h_instance) + static_offset + offset;
            *(uint16_t *)(var_addr + edi->val_l) = (uint16_t)val;
            edi->val = val;
            break;
        }
        case 68: // do_SSDA
        {
            int32_t val = edi->val;
            edi++;
            uint16_t offset = *(uint16_t *)esi;
            esi += 2;
            uint8_t *var_addr = (uint8_t *)RTR_addr(h_instance) + static_offset + offset;
            *(int32_t *)(var_addr + edi->val_l) = val;
            edi->val = val;
            break;
        }
        case 69: // do_LESA
        {
            uint16_t offset = *(uint16_t *)esi;
            esi += 2;
            edi->addr_flat = (uint8_t *)RTR_addr(h_instance) + static_offset + offset;
            break;
        }
        case 70: // do_LXB
        {
            uint16_t index = *(uint16_t *)esi;
            esi += 2;
            uint16_t ecx = *(uint16_t *)(ptr_thunk + index + extern_offset);
            void **ref_handle = (void **)objlist_ptr[edi->val_l];
            uint8_t *ref_inst = (uint8_t *)*ref_handle;
            edi->val = (int8_t)*(ref_inst + ecx);
            break;
        }
        case 71: // do_LXW
        {
            uint16_t index = *(uint16_t *)esi;
            esi += 2;
            uint16_t ecx = *(uint16_t *)(ptr_thunk + index + extern_offset);
            void **ref_handle = (void **)objlist_ptr[edi->val_l];
            uint8_t *ref_inst = (uint8_t *)*ref_handle;
            edi->val = (int16_t)*(uint16_t *)(ref_inst + ecx);
            break;
        }
        case 72: // do_LXD
        {
            uint16_t index = *(uint16_t *)esi;
            esi += 2;
            uint16_t ecx = *(uint16_t *)(ptr_thunk + index + extern_offset);
            void **ref_handle = (void **)objlist_ptr[edi->val_l];
            uint8_t *ref_inst = (uint8_t *)*ref_handle;
            edi->val = *(int32_t *)(ref_inst + ecx);
            break;
        }
        case 73: // do_SXB
        {
            int32_t val = edi->val;
            edi++;
            uint16_t index = *(uint16_t *)esi;
            esi += 2;
            uint16_t ecx = *(uint16_t *)(ptr_thunk + index + extern_offset);
            void **ref_handle = (void **)objlist_ptr[edi->val_l];
            uint8_t *ref_inst = (uint8_t *)*ref_handle;
            *(ref_inst + ecx) = (uint8_t)val;
            edi->val = val;
            break;
        }
        case 74: // do_SXW
        {
            int32_t val = edi->val;
            edi++;
            uint16_t index = *(uint16_t *)esi;
            esi += 2;
            uint16_t ecx = *(uint16_t *)(ptr_thunk + index + extern_offset);
            void **ref_handle = (void **)objlist_ptr[edi->val_l];
            uint8_t *ref_inst = (uint8_t *)*ref_handle;
            *(uint16_t *)(ref_inst + ecx) = (uint16_t)val;
            edi->val = val;
            break;
        }
        case 75: // do_SXD
        {
            int32_t val = edi->val;
            edi++;
            uint16_t index = *(uint16_t *)esi;
            esi += 2;
            uint16_t ecx = *(uint16_t *)(ptr_thunk + index + extern_offset);
            void **ref_handle = (void **)objlist_ptr[edi->val_l];
            uint8_t *ref_inst = (uint8_t *)*ref_handle;
            *(int32_t *)(ref_inst + ecx) = val;
            edi->val = val;
            break;
        }
        case 76: // do_LXBA
        {
            uint16_t index = *(uint16_t *)esi;
            esi += 2;
            uint16_t ecx = *(uint16_t *)(ptr_thunk + index + extern_offset) + edi->val_h;
            void **ref_handle = (void **)objlist_ptr[edi->val_l];
            uint8_t *ref_inst = (uint8_t *)*ref_handle;
            edi->val = (int8_t)*(ref_inst + ecx);
            break;
        }
        case 77: // do_LXWA
        {
            uint16_t index = *(uint16_t *)esi;
            esi += 2;
            uint16_t ecx = *(uint16_t *)(ptr_thunk + index + extern_offset) + edi->val_h;
            void **ref_handle = (void **)objlist_ptr[edi->val_l];
            uint8_t *ref_inst = (uint8_t *)*ref_handle;
            edi->val = (int16_t)*(uint16_t *)(ref_inst + ecx);
            break;
        }
        case 78: // do_LXDA
        {
            uint16_t index = *(uint16_t *)esi;
            esi += 2;
            uint16_t ecx = *(uint16_t *)(ptr_thunk + index + extern_offset) + edi->val_h;
            void **ref_handle = (void **)objlist_ptr[edi->val_l];
            uint8_t *ref_inst = (uint8_t *)*ref_handle;
            edi->val = *(int32_t *)(ref_inst + ecx);
            break;
        }
        case 79: // do_SXBA
        {
            int32_t val = edi->val;
            edi++;
            uint16_t index = *(uint16_t *)esi;
            esi += 2;
            uint16_t ecx = *(uint16_t *)(ptr_thunk + index + extern_offset) + edi->val_h;
            void **ref_handle = (void **)objlist_ptr[edi->val_l];
            uint8_t *ref_inst = (uint8_t *)*ref_handle;
            *(ref_inst + ecx) = (uint8_t)val;
            edi->val = val;
            break;
        }
        case 80: // do_SXWA
        {
            int32_t val = edi->val;
            edi++;
            uint16_t index = *(uint16_t *)esi;
            esi += 2;
            uint16_t ecx = *(uint16_t *)(ptr_thunk + index + extern_offset) + edi->val_h;
            void **ref_handle = (void **)objlist_ptr[edi->val_l];
            uint8_t *ref_inst = (uint8_t *)*ref_handle;
            *(uint16_t *)(ref_inst + ecx) = (uint16_t)val;
            edi->val = val;
            break;
        }
        case 81: // do_SXDA
        {
            int32_t val = edi->val;
            edi++;
            uint16_t index = *(uint16_t *)esi;
            esi += 2;
            uint16_t ecx = *(uint16_t *)(ptr_thunk + index + extern_offset) + edi->val_h;
            void **ref_handle = (void **)objlist_ptr[edi->val_l];
            uint8_t *ref_inst = (uint8_t *)*ref_handle;
            *(int32_t *)(ref_inst + ecx) = val;
            edi->val = val;
            break;
        }
        case 82: // do_LEXA
        {
            uint16_t index = *(uint16_t *)esi;
            esi += 2;
            uint16_t ecx = *(uint16_t *)(ptr_thunk + index + extern_offset);
            void **ref_handle = (void **)objlist_ptr[edi->val_l];
            uint8_t *ref_inst = (uint8_t *)*ref_handle;
            edi->addr_flat = ref_inst + ecx;
            break;
        }
        case 83: // do_SXAS
        {
            uint16_t index = edi->val_l;
            edi++;
            edi->val_h = index;
            break;
        }
        case 84: // do_LECA
        {
            uint16_t target = *(uint16_t *)esi;
            esi += 2;
            edi->val = (int32_t)(uintptr_t)(ds32 + target);
            break;
        }
        case 85: // do_SOLE
        {
            void *ref_handle = (void *)objlist_ptr[edi->val_l];
            edi->val = (int32_t)(uintptr_t)ref_handle;
            break;
        }
        case 86: // do_END
        {
            RTR_unlock(h_prg);
            return edi->val;
        }
        case 87: // do_BRK
            break;
        default:
            opcode_fault((void *)(esi - 1), (void *)edi);
            break;
        }
    }
}
