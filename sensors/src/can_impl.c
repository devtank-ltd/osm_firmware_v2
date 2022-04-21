#include "common.h"
#include "can_comm.h"

void can_impl_init(void)
{
    can_comm_init();
    can_comm_enable(true);
}


bool can_impl_send(uint32_t id, uint8_t* data, unsigned len)
{
    if (len > CAN_COMM_MAX_DATA_SIZE)
        return false;
    can_comm_packet_t pkt;
    pkt.header.id = id;
    pkt.header.ext = false;
    pkt.header.rtr = false;
    pkt.header.length = len;
    pkt.data = data;
    can_comm_send(&pkt);
    return true;
}


void can_impl_send_example(void)
{
    uint8_t some_data[] = { 1, 2, 3, 4, 5, 6 };
    can_impl_send(123, some_data, ARRAY_SIZE(some_data));
}


static void _can_impl_parse_pkt(can_comm_packet_t* pkt) { }


void can_drain_array(void)
{
    can_comm_packet_t pkt;
    can_comm_data_t data;
    pkt.data = data;
    unsigned n;
    unsigned header_size = sizeof(can_comm_header_t);
    n = ring_buf_read(&can_comm_ring_data, (char*)&pkt.header, header_size);
    if (n != header_size)
        // Not finished writing to buffer?
        return;
    n = ring_buf_read(&can_comm_ring_data, (char*)pkt.data, pkt.header.length);
    if (n != pkt.header.length)
        // Not finished writing to buffer?
        return;
    _can_impl_parse_pkt(&pkt);
}
