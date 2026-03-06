//
//  Run-time graphics subs
//

#ifndef GRAPHICS_H
#define GRAPHICS_H

#include <stdint.h>

extern uint8_t *fade_tables[5][16];
extern uint16_t first_color[5];
extern uint16_t in_GIL;

// #define MODE_X 1                 // 1 for VGA mode X, 0 for MCGA mode 13h

#define NTW 32 // # of text windows available

// void dprint(uint32_t argcnt, int8_t *format, ...); // Tom: TODO check argcnt int32_t and uint32_t
// void sprint(uint32_t argcnt, uint32_t wndnum, int8_t *format, ...); // Tom: TODO check argcnt int32_t and uint32_t
// void text_color(uint32_t argcnt, uint32_t wndnum, uint32_t current, uint32_t new); // Tom: TODO check argcnt int32_t and uint32_t
void release_owned_windows(uint32_t owner); // Tom: TODO check argcnt int32_t and uint32_t

void dprint(int32_t argcnt, int8_t *format, ...); // Tom: TODO check argcnt int32_t and uint32_t
void sprint(int32_t argcnt, uint32_t wndnum, int8_t *format, ...); // Tom: TODO check argcnt int32_t and uint32_t
void text_color(int32_t argcnt, uint32_t wndnum, uint32_t current, uint32_t new); // Tom: TODO check argcnt int32_t and uint32_t

#endif
