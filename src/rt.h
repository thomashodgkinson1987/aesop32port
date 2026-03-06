//
//  AESOP runtime ASM calls
//

#ifndef RT_H
#define RT_H

#include <stdint.h> // Tom: added
#include <string.h> // Tom: added

#include "rtres.h" // Tom: added

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

void *RTD_first(void *dictionary); // Tom: original
// uint32_t *RTD_first(void *dictionary);

void *RTD_iterate(void *base, void *cur, int8_t **tag, int8_t **def); // Tom: original
// uint32_t *RTD_iterate(void *base, uint32_t cur, int8_t **tag, int8_t **def); // Tom: new

int8_t *RTD_lookup(uint32_t dictionary, void *key); // Tom: original
// int8_t *RTD_lookup(uint32_t dictionary, uint8_t *key); // Tom: new

// Runtime interpreter calls

void RT_init(RTR_class *RTR, uint32_t stack_size, uint32_t *objlist);
void RT_shutdown(void);
void RT_arguments(void *base, uint32_t size);
int32_t RT_execute(uint32_t index, uint32_t msg_num, uint32_t vector);

#endif
