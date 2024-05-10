#! /usr/bin/env python3
from crccheck.crc import CrcModbus
import os
import sys
import ssl
import math
import json
import time
import random
import struct
import logging
import datetime

import paho.mqtt.client as mqtt

protocol_version = b'\x01'

logging.basicConfig(level=logging.INFO, datefmt="%Y-%m-%dT%H:%M:%S", format="[%(asctime)s.%(msecs)03d] %(filename)s:%(funcName)s [%(levelname)s]: %(message)s")
_logger = logging.getLogger(__name__)


MTU = 32


COMMAND_RESP_NONE   = 0x00
COMMAND_RESP_OK     = 0x01
COMMAND_RESP_ERR    = 0x02


def _make_payload(cmd):
    return json.dumps({"CCMD": cmd})


def _send_packet(client, userdata):
    pos = userdata.get("pos", 0)

    firmware = userdata["firmware"]

    if pos == len(firmware):
        # FINISH TRANSMISSION
        _finish_fw(client, userdata)
        payload = _make_payload()
        client.publish(userdata["pub_topic"], "fw@")
        _logger.info("FINISHED TRANSMISSION")
        done = False
        return

    chunk = firmware[pos : pos + MTU].hex()
    if len(chunk) % 2:
        chunk = "0" + chunk
    client.publish(userdata["pub_topic"], f"fw+ {chunk}")
    userdata.update({"pos": pos + MTU })
    client.user_data_set(userdata)

def _on_connect(client, userdata, flags, reason_code, properties):
    if reason_code ==0:
        _logger.info("CONNECTED")
        _logger.info(f"SUBSCRIBING TO '{userdata['sub_topic']}'")
        client.subscribe(userdata["sub_topic"])
    if reason_code > 0:
        _logger.error(f"FAILED TO CONNECT: '{reason_code}'")

def _on_disconnect(client, userdata, disconnect_flags, reason_code, properties):
    _logger.info(f"DISCONNECTED: '{reason_code}'")

def _on_subscribe(client, userdata, mid, reason_code_list, properties):
    for reason_code in reason_code_list:
        if reason_code < 128:
            _logger.info("SUBSCRIBED")
            # SENDING START / KICKOFF
            _send_packet(client, userdata)
        else:
            _logger.error(f"FAILED TO SUBSCRIBE: '{reason_code}'")

def _on_message(client, userdata, message):
    _logger.info(f"RECEIVED MESSAGE << '{message.topic}':{message.payload}")
    data = json.loads(message.payload.decode())
    print(data)
    ret_code_str = data.get("ret_code")
    if ret_code_str:
        ret_code = int(ret_code_str[2:], 16)
        print(f"{ret_code = }")
        print(f"{type(ret_code) = }")
        if ret_code == COMMAND_RESP_OK:
            _send_packet(client, userdata)
        else:
            time.sleep(1)
            _logger.error(f"UNEXPECTED RETURN CODE: {ret_code}")
            done = True


def main(args):
    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)

    client.enable_logger(_logger)
    if args.verbose:
        _logger.info("ENABLED DEBUG")
        _logger.setLevel(logging.DEBUG)

    client.on_connect = _on_connect
    client.on_disconnect = _on_disconnect
    client.on_subscribe = _on_subscribe
    client.on_message = _on_message

    hwid = args.hwid
    if len(hwid) > 2 and hwid[:2].lower() == "0x":
        hwid = hwid[2:]
    if len(hwid) < 8:
        hwid = "0" * (8 - len(hwid)) + hwid
    sub_topic = f"osm/{hwid}/cmd/resp"
    pub_topic = f"osm/{hwid}/cmd"

    firmware = args.firmware.read()

    client.user_data_set({
        "sub_topic": sub_topic,
        "pub_topic": pub_topic,
        "firmware": firmware,
        "pos": 0,
        "crc": CrcModbus.calc(firmware),
        "mtu": 32,
    })

    if args.websockets:
        client.transport = "websockets"
        _logger.debug("USING WEBSOCKETS")

    if args.ssl:
        if args.insecure:
            _logger.debug("USING SSL NO VERIFY")
            client.tls_set(cert_reqs=ssl.CERT_NONE)
            client.tls_insecure_set(True)
        else:
            _logger.debug(f"USING SSL WITH CA: {args.ca}")
            client.tls_set(ca_certs=args.ca)

    if args.user:
        _logger.debug(f"USING AUTHORISATION FOR '{args.user}'")
        client.username_pw_set(args.user, password=args.password)

    _logger.info("STARTING CONNECT")
    client.connect(args.address, args.port)

    global done
    done = False
    while not done:
        client.loop()
    return 0


if __name__ == '__main__':
    import argparse
    def get_args():
        parser = argparse.ArgumentParser(description="OSM WiFI OTA Updater.")
        parser.add_argument("-v", "--verbose"       , help="Show debug"         , action="store_true"   )
        parser.add_argument("-w", "--websockets"    , help="Use websockets"     , action="store_true"   )
        parser.add_argument("-s", "--ssl"           , help="Use SSL"            , action="store_true"   )
        parser.add_argument("-i", "--insecure"      , help="Don't verify SSL"   , action="store_true"   )
        parser.add_argument("-a", "--address"   , metavar="HOST"    , type=str  , help="Address to the MQTT broker" , default="localhost"   )
        parser.add_argument("-p", "--port"      , metavar="PORT"    , type=int  , help="Port of the MQTT broker"    , default=1883          )
        parser.add_argument("-u", "--user"      , metavar="USER"    , type=str  , help="Username for MQTT"          , default=None          )
        parser.add_argument("-P", "--password"  , metavar="PWD"     , type=str  , help="Password for MQTT USER"     , default=None          )
        parser.add_argument("-c", "--ca"        , metavar="CA"      , type=str  , help="Central Authority"          , default=None          )
        parser.add_argument("firmware"          , metavar="FW"      , type=argparse.FileType('rb')  , help="Firmware"                           )
        parser.add_argument("hwid"              , metavar="HWID"    , type=str                      , help="Hardware ID of the OSM to update"   )
        return parser.parse_args()

    args = get_args()
    sys.exit(main(args))
