//
//  Run-time linker
//

#ifndef RTLINK_H
#define RTLINK_H

#include <stdint.h>

uint32_t create_instance(RTR_class *RTR, uint32_t object);
void destroy_instance(RTR_class *RTR, uint32_t instance);

#endif
