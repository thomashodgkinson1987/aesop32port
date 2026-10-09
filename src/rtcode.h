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

//
// Virtual BIOS Data Area cell 0x04F0 (module-routing magic). The DOS
// shell loop re-ran AESOP.EXE after each external module; here
// launch() re-execs this process with the poked magic carried in
// AESOP_BDA, and dos_bda_init() picks it up at startup (defaulting
// to 'INTR' cold boot when unset).
//
void dos_bda_init(void);
int32_t dos_bda_value(void);

void beep(void);

#endif
