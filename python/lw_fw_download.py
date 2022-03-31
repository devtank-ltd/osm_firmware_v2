#! /usr/bin/env python3
from crccheck.crc import CrcModbus
import os
import sys
import struct

import grpc
from chirpstack_api.as_pb.external import api


def _send_command(client, dev_eui, port, json):
    print("JSON:",json)
    req = api.EnqueueDeviceQueueItemRequest()
    req.device_queue_item.confirmed = True
    req.device_queue_item.json_object = json
    req.device_queue_item.dev_eui = dev_eui
    req.device_queue_item.f_port = port
    resp = client.Enqueue(req, metadata=auth_token)
    print(resp.f_cnt)


if __name__ == "__main__":
    if len(sys.argv) < 5:
        print("<server> <dev_eui> <api_token> <firmware>")
        exit(-1)

    server    = sys.argv[1]
    dev_eui   = sys.argv[2]
    api_token = sys.argv[3]
    fw_path   = sys.argv[4]

    channel = grpc.insecure_channel(server)

    client = api.DeviceQueueServiceStub(channel)

    auth_token = [("authorization", "Bearer %s" % api_token)]

    data = open(fw_path, "rb").read()
    crc = CrcModbus.calc(data)

    mtu = 64
    port = 1

    print("Update chunks:", len(data)/mtu)

    for n in range(0, len(data), mtu):
        chunk = data[n:n + mtu]
        chunk = chunk[::-1]
        json = b'{"FW+":"%s"}' % chunk
        _send_command(client, dev_eui, port, json)
        port += 1
        if port > 127:
            port = 1

    print("Closing CRC:", crc)
    json = b'{"FW@":"%s"}' % struct.pack(">H", crc)
    _send_command(client, dev_eui, port, json)
