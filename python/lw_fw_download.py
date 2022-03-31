#! /usr/bin/env python3
from crccheck.crc import CrcModbus
import os
import sys
import time
import struct

import grpc
from chirpstack_api.as_pb.external import api

protocol_version = b'\x01'


def _send_command_json(port, json):
    req = api.EnqueueDeviceQueueItemRequest()
    req.device_queue_item.confirmed = True
    req.device_queue_item.json_object = json
    req.device_queue_item.dev_eui = dev_eui
    req.device_queue_item.f_port = port
    resp = client.Enqueue(req, metadata=auth_token)
    print("resp", resp.f_cnt)


def _send_command_bin(port, cmd, bin_payload):
    data = protocol_version + cmd[0:4].encode()
    data += int(5 - len(data)) * b'\x00'
    data += bin_payload
    req = api.EnqueueDeviceQueueItemRequest()
    req.device_queue_item.confirmed = True
    req.device_queue_item.data = data
    req.device_queue_item.dev_eui = dev_eui
    req.device_queue_item.f_port = port
    resp = client.Enqueue(req, metadata=auth_token)
    print("resp", resp.f_cnt)


def _send_firmware(port, fw_path):

    data = open(fw_path, "rb").read()
    crc = CrcModbus.calc(data)

    mtu = 4

    count = len(data)/mtu

    print("Update chunks:", count)

    _send_command_bin(port, "FW-", struct.pack("<H", count))
    port+=1

    for n in range(0, len(data), mtu):
        chunk = data[n:n + mtu]
        _send_command_bin(port, "FW+", chunk)
        port += 1
        if port > 127:
            port = 1
        time.sleep(0.25)

    print("Closing CRC:", crc)
    _send_command_bin(port, "FW@", struct.pack("<H", crc))


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

    _send_command_json(1, '{"CMD":"debug 804"}')
    _send_firmware(2, fw_path)
