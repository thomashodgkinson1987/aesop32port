//
// Shared hot-path trace gating (see trace.h).
//

#include <stdlib.h>

#include "trace.h"

int32_t aesop_trace = 0;

void aesop_trace_init(void)
{
   aesop_trace = (getenv("AESOP_TRACE") != NULL);
}