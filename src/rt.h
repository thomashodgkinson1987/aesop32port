//
//  AESOP runtime ASM calls
//

#ifndef RT_H
#define RT_H

#include <stdint.h>
#include <string.h>

#include "rtres.h"

typedef struct // fundamental stack value structure
{
   uint32_t val;
   uint16_t type;
} STKVAL;

enum
{
   TYP_CRES, // data type: code resource address
   TYP_SRES, // data type: string resource
   TYP_VSHR, // data type: short integer variable
   TYP_VLNG, // data type: long integer variable
   TYP_SVAR, // data type: string variable
};

extern uint32_t diag_flag;
extern uint32_t current_this;
extern uint32_t current_msg;
extern uint32_t current_index;

// Pointer and memory block management

#define norm(x) ((void *)(x))

#define add_offset(s, o) ((void *)((uint32_t)(s) + (uint32_t)(o)))

#define add_ptr(base, offset) ((void *)((uint32_t)(base) + (uint32_t)(offset)))

#define ptr_dif(top, bot) (((int8_t *)(top) - (int8_t *)(bot)))

#define far_memmove(dest, src, len) ((void *)memmove((dest), (src), (len)))

// Assorted speed-critical .ASM routines

void *RTD_first(void *dictionary, uint32_t dict_size); // Tom: original
// uint32_t *RTD_first(void *dictionary);

void *RTD_iterate(void *base, void *cur, int8_t **tag, int8_t **def, uint32_t base_size); // Tom: original
// uint32_t *RTD_iterate(void *base, uint32_t cur, int8_t **tag, int8_t **def); // Tom: new

int8_t *RTD_lookup(uint32_t dictionary, void *key); // Tom: original
// int8_t *RTD_lookup(uint32_t dictionary, uint8_t *key); // Tom: new

// Runtime interpreter calls

void RT_init(RTR_class *RTR, uint32_t stack_size, uint32_t *objlist);
void RT_shutdown(void);
void RT_arguments(void *base, uint32_t size);
void RT_drop_arguments(uint32_t size);
int32_t RT_execute(uint32_t index, uint32_t msg_num, uint32_t vector);

//
// Host-provided yield hook, called periodically from the interpreter
// loop so the host can pump events etc. (the original RT.ASM was
// video-agnostic; this keeps it that way). Returns nonzero to abort
// execution (ESC / window close).
//
extern int32_t (*RT_yield)(void);

// The interpreter loop calls the yield hook every this-many opcodes
#define RT_YIELD_INTERVAL 1024

#endif
