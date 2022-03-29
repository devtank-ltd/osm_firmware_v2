#include "common.h"
#include "can_comm.h"

void can_impl_init(void)
{
    can_comm_init();
}


bool can_impl_send(uint32_t id, uint8_t* data, unsigned len)
{
    if (len > CAN_COMM_MAX_DATA_SIZE)
        return false;
    can_comm_packet_t pkt;
    pkt.header.id = id;
    pkt.header.ext = false;
    pkt.header.rtr = false;
    pkt.header.length = len + sizeof(can_comm_header_t);
    pkt.data = (can_comm_data_t*)&data;
    can_comm_send(&pkt);
    return true;
}


void can_impl_send_example(void)
{
    uint8_t some_data[] = { 1, 2, 3, 4, 5, 6 };
    can_impl_send(12345, some_data, ARRAY_SIZE(some_data));
}
