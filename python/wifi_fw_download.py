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
import traceback

import paho.mqtt.client as mqtt

protocol_version = b'\x01'

logging.basicConfig(level=logging.INFO, datefmt="%Y-%m-%dT%H:%M:%S", format="[%(asctime)s.%(msecs)03d] %(filename)s:%(funcName)s [%(levelname)s]: %(message)s")



class wifi_fw_dl_t(object):
    MTU                 = 90
    COMMAND_RESP_NONE   = 0x00
    COMMAND_RESP_OK     = 0x01
    COMMAND_RESP_ERR    = 0x02

    MAX_LINE_LEN        = 256

    EOL                 = "\r\n"



    def __init__(self, address, port, firmware_file, hwid, user=None, password=None, ca=None, websockets=False, use_ssl=False, insecure=False, verbose=False):

        self.client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
        self._logger = logging.getLogger(__name__)
        self.client.enable_logger(self._logger)
        if verbose:
            self._logger.info("ENABLED DEBUG")
            self._logger.setLevel(logging.DEBUG)

        self.client.on_connect = self._on_connect
        self.client.on_disconnect = self._on_disconnect
        self.client.on_subscribe = self._on_subscribe
        self.client.on_message = self._on_message


        if len(hwid) > 2 and hwid[:2].lower() == "0x":
            hwid = hwid[2:]
        if len(hwid) < 8:
            hwid = "0" * (8 - len(hwid)) + hwid
        self.hwid = hwid

        sub_topic = f"osm/{hwid}/cmd/resp"
        self.pub_topic = f"osm/{hwid}/cmd"

        firmware = firmware_file.read()

        at_preamble = f"+MQTTSUBRECV:0,\"{self.pub_topic}\",{self.MAX_LINE_LEN},"
        # Using MTU as Max line length - overhead for AT MQTT messages - command length - line ending length -1 for null termination
        self.MTU = self.MAX_LINE_LEN - len(at_preamble) - len("fw+ ") - len(self.EOL) - 1
        # If MTU is odd, make even my -1
        if self.MTU % 2:
            self.MTU -= 1
        self._logger.debug(f"USING MTU = {self.MTU}")

        bin_mtu = math.floor(self.MTU/2)
        chunks = []
        for i in range(0, len(firmware), bin_mtu):
            chunk = firmware[i:i+bin_mtu].hex()
            chunks.append(chunk)

        crc = hex(CrcModbus.calc(firmware))[2:].rjust(4,"0")

        self.commands = ["meas_enable 0"]
        self.commands += [f"fw+ {chunk}" for chunk in chunks]
        self.commands += [f"fw@ {crc}", "fw_reset"]
        self.command_index = 0

        self.client.user_data_set({
            "sub_topic": sub_topic,
            "pub_topic": self.pub_topic,
            "firmware": firmware,
            "pos": 0,
            "mtu": 32,
        })

        if websockets:
            self.client.transport = "websockets"
            self._logger.debug("USING WEBSOCKETS")

        if use_ssl:
            if insecure:
                self._logger.debug("USING SSL NO VERIFY")
                self.client.tls_set(cert_reqs=ssl.CERT_NONE)
                self.client.tls_insecure_set(True)
            else:
                self._logger.debug(f"USING SSL WITH CA: {ca}")
                self.client.tls_set(ca_certs=ca)

        if user:
            self._logger.debug(f"USING AUTHORISATION FOR '{user}'")
            self.client.username_pw_set(user, password=password)

        self._logger.info("STARTING CONNECT")
        self.client.connect(address, port)
        self.done = False

        self._is_measuring = True

    def close(self):
        if self.client:
            if not self._is_measuring:
                self.client.publish(self.pub_topic, "meas_enable 1")
                time.sleep(0.3)
            self.client.disconnect()
            self.client = None

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        self.close()

    def __del__(self):
        self.close()

    def _publish(self, cmd):
        time.sleep(0.25)
        self.client.publish(self.pub_topic, cmd)
        self._logger.debug(f"CMD: {cmd}")

    def _publish_command(self):
        self._logger.debug(f"PUBLISHING {self.command_index}/{len(self.commands)}")
        self._logger.info(f"{100. * self.command_index/len(self.commands): .02f} %")
        self._publish(self.commands[self.command_index])
        self.command_index += 1

    def _send_packet(self, client, userdata):
        if self.commands[self.command_index] == "fw_reset":
            # Dont expect reply and give a timeout for before sending next
            self._publish_command()
            time.sleep(2)
        if self.command_index >= len(self.commands):
            self._logger.info("NO MORE TO SEND")
            self.done = True
            return
        self._publish_command()
        if self.command_index > 1:
            self._is_measuring = False

    def _on_connect(self, client, userdata, flags, reason_code, properties):
        if reason_code ==0:
            self._logger.info("CONNECTED")
            self._logger.info(f"SUBSCRIBING TO '{userdata['sub_topic']}'")
            client.subscribe(userdata["sub_topic"])
        if reason_code > 0:
            self._logger.error(f"FAILED TO CONNECT: '{reason_code}'")

    def _on_disconnect(self, client, userdata, disconnect_flags, reason_code, properties):
        self._logger.info(f"DISCONNECTED: '{reason_code}'")

    def _on_subscribe(self, client, userdata, mid, reason_code_list, properties):
        for reason_code in reason_code_list:
            if reason_code < 128:
                self._logger.info("SUBSCRIBED")
                # SENDING START / KICKOFF
                self._send_packet(client, userdata)
            else:
                self._logger.error(f"FAILED TO SUBSCRIBE: '{reason_code}'")

    def _on_message(self, client, userdata, message):
        self._logger.info(f"RECEIVED MESSAGE << '{message.topic}':{message.payload}")
        data = json.loads(message.payload.decode())
        ret_code_str = data.get("ret_code")
        if ret_code_str:
            ret_code = int(ret_code_str[2:], 16)
            if ret_code == self.COMMAND_RESP_OK:
                self._send_packet(client, userdata)
            else:
                self._logger.error(f"UNEXPECTED RETURN CODE: {ret_code}")
                self.done = True

    def run(self):
        while not self.done:
            self.client.loop()


def main(args):
    return_val = 0
    start_time = time.monotonic()
    with wifi_fw_dl_t(args.address, args.port, args.firmware, args.hwid, user=args.user, password=args.password, ca=args.ca, websockets=args.websockets, use_ssl=args.ssl, insecure=args.insecure, verbose=args.verbose) as wifi_fw_dl:
        try:
            wifi_fw_dl.run()
        except KeyboardInterrupt:
            return_val = 1
            print("Caught keyboard interrupt", file=sys.stderr, flush=True)
        except:
            return_val = -1
            traceback.print_exc()
    print(f"Time taken = {time.monotonic() - start_time: .03f}s")
    return return_val


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
