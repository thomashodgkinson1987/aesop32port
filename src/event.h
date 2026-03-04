//
//  Run-time event handler calls
//

#ifndef EVENT_H
#define EVENT_H

#include <stdint.h> // Tom: added (I think)

#define EV_QSIZE 128 // max # of queued events (circular)
#define NR_LSIZE 768 // max # of event notification requests

#define NSX_IN_REGION 0x100 // notification status flags (high word)
#define NSX_OUT_REGION 0x200
#define NSX_TYPE 0x00FF // notification event type mask (low word)

typedef struct NREQ
{
   int32_t next;
   int32_t prev;
   int32_t client;
   uint32_t message;
   int32_t parameter;
   int32_t status;
} NREQ; // notification request list entry

typedef struct
{
   int32_t type;
   int32_t owner;
   int32_t parameter;
} EVENT;

extern int32_t ENABLED;

extern NREQ NR_list[NR_LSIZE];
extern int32_t NR_first[NUM_EVTYPES];

extern int32_t current_event_type;

//
// Internal calls
//

void init_notify_list(void);
void add_notify_request(int32_t client, int32_t message, int32_t event, int32_t parameter);
void delete_notify_request(int32_t client, int32_t message, int32_t event, int32_t parameter);
void cancel_entity_requests(int32_t client);
void init_event_queue(void);
EVENT *find_event(int32_t type, int32_t parameter);
void remove_event(int32_t type, int32_t parameter, int32_t owner);
void add_event(int32_t type, int32_t parameter, int32_t owner);
EVENT *next_event(void);
EVENT *fetch_event(void);
void dump_event_queue(void);

void DISABLE(void);
void ENABLE(void);

//
// AESOP code resource calls
//

void notify(int32_t argcnt, uint32_t index, uint32_t message, int32_t event, int32_t parameter);
void cancel(int32_t argcnt, uint32_t index, uint32_t message, int32_t event, int32_t parameter);
void drain_event_queue(void);
void post_event(int32_t argcnt, uint32_t owner, int32_t event, int32_t parameter);
void send_event(int32_t argcnt, uint32_t owner, int32_t event, int32_t parameter);
uint32_t peek_event(void);
void dispatch_event(void);
void flush_event_queue(int32_t argcnt, int32_t owner, int32_t event, int32_t parameter);

#endif
