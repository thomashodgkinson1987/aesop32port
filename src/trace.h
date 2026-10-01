//
// Shared hot-path trace gating.
//
// AESOP_TRACE=1 in the environment enables verbose hot-path logging:
// per-opcode interpreter traces, event dispatch spew, RCRS backtraces.
// These paths execute hundreds of thousands of times per second, so
// their logging is gated behind this flag; when it is unset they cost
// nothing but a predictable branch.
//
// Cold-path diagnostics ([STUB] markers, boot/link milestones) stay
// unconditional on purpose — they are the port's verification signal.
//

#ifndef TOM_TRACE
#define TOM_TRACE

#include <stdint.h>

extern int32_t aesop_trace;

// Call once from main() before anything logs.
void aesop_trace_init(void);

#endif