//
//  Run-time code resources
//

#ifndef RTCODE_H
#define RTCODE_H

#include <stdint.h>

void diagnose(int32_t argcnt, uint32_t dtype, uint32_t parm);

// typedef void(cdecl *FARPROC)(); // Tom: commented out, new version below
typedef void (*FARPROC)(void); // Tom: added

extern FARPROC code_resources[];

uint32_t absv(int32_t argcnt, int32_t val);
uint32_t rnd(int32_t argcnt, uint32_t low, uint32_t high);
int32_t envval(int32_t argcnt, int8_t *name);

void beep(void);

#endif
