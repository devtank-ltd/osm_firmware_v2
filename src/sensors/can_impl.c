#include <stddef.h>

#include <osm/core/common.h>
#include <osm/core/can_comm.h>

void osm_can_impl_init(void)
{
    osm_can_comm_init();
    osm_can_comm_enable(true);
}


bool osm_can_impl_send(uint32_t id, uint8_t* data, unsigned len)
{
    if (len > OSM_CAN_COMM_MAX_DATA_SIZE)
        return false;
    can_comm_packet_t pkt;
    pkt.header.id = id;
    pkt.header.ext = false;
    pkt.header.rtr = false;
    pkt.header.length = len;
    pkt.data = data;
    osm_can_comm_send(&pkt);
    return true;
}


void osm_can_impl_send_example(void)
{
    uint8_t some_data[] = { 1, 2, 3, 4, 5, 6 };
    osm_can_impl_send(123, some_data, OSM_ARRAY_SIZE(some_data));
}


static void _can_impl_parse_pkt(can_comm_packet_t* pkt) { }


void osm_can_drain_array(void)
{
    can_comm_packet_t pkt;
    can_comm_data_t data;
    pkt.data = data;
    unsigned n;
    unsigned header_size = sizeof(can_comm_header_t);
    n = osm_ring_buf_read(&can_comm_ring_data, (char*)&pkt.header, header_size);
    if (n != header_size)
        // Not finished writing to buffer?
        return;
    n = osm_ring_buf_read(&can_comm_ring_data, (char*)pkt.data, pkt.header.length);
    if (n != pkt.header.length)
        // Not finished writing to buffer?
        return;
    _can_impl_parse_pkt(&pkt);
}


static osm_command_response_t _can_impl_cb(char* args, cmd_ctx_t * ctx)
{
    osm_can_impl_send_example();
    return OSM_COMMAND_RESP_OK;
}


struct cmd_link_t* osm_can_impl_add_commands(struct cmd_link_t* tail)
{
    static struct cmd_link_t cmds[] = {{ "can_impl",     "Send example CAN message", _can_impl_cb                  , false , NULL }};
    return osm_add_commands(tail, cmds, OSM_ARRAY_SIZE(cmds));
}
