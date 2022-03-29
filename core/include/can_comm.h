#pragma once


#include "ring.h"


#define CAN_COMM_MAX_DATA_SIZE                                      8


typedef uint8_t can_comm_data_t[CAN_COMM_MAX_DATA_SIZE];

typedef struct
{
    uint32_t            id;
    bool                ext;
    bool                rtr;
    uint8_t             fmi;
    uint8_t             length;
} can_comm_header_t;

typedef struct
{
    can_comm_header_t   header;
    can_comm_data_t*    data;
} can_comm_packet_t;


extern ring_buf_t can_comm_ring_data;


extern void can_comm_init(void);
extern void can_comm_send(can_comm_packet_t* pkt);
extern void can_comm_enable(bool enabled);

/* To be implemented by caller.*/
extern void can_drain_array(void) __attribute__((weak));
