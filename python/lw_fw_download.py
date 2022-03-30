#! /usr/bin/env python3
import os
import sys

import grpc
from chirpstack_api.as_pb.external import api


if __name__ == "__main__":
    if len(sys.argv) < 4:
        print("<server> <dev_eui> <api_token>")
        exit(-1)

    server = sys.argv[1]
    dev_eui = sys.argv[2]
    api_token = sys.argv[3]

    channel = grpc.insecure_channel(server)

    client = api.DeviceQueueServiceStub(channel)

    auth_token = [("authorization", "Bearer %s" % api_token)]

    req = api.EnqueueDeviceQueueItemRequest()
    req.device_queue_item.confirmed = True
    req.device_queue_item.json_object = '{"CMD":"debug"}'
    req.device_queue_item.dev_eui = dev_eui
    req.device_queue_item.f_port = 10

    resp = client.Enqueue(req, metadata=auth_token)

    # Print the downlink frame-counter value.
    print(resp.f_cnt)

