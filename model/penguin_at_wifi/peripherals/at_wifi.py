#!/usr/bin/env python3

import os
import sys
import tty
import ssl
import time
import enum
import logging
import paho.mqtt as paho
import paho.mqtt.client as mqtt
import select
import serial
import weakref
import selectors
import re
import ast

import timerfd

logging.basicConfig(level=logging.ERROR)

logger = logging.getLogger("at_wifi.py")


class base_at_commands_t(object):
    OK          = "OK"
    PARAM_ERROR = "ERROR: PARAM"
    STATE_ERROR = "ERROR: STATE"

    TEST    = "TEST"
    QUERY   = "QUERY"
    SET     = "SET"
    EXECUTE = "EXECUTE"

    def __init__(self, device, command_controller, commands: dict):
        self.COMMAND_TYPES = enum.Enum(
            "COMMAND_TYPES", [
                self.TEST,
                self.QUERY,
                self.SET,
                self.EXECUTE,
            ])
        self._device = weakref.ref(device)
        self._command_controller = weakref.ref(command_controller)
        self.commands = commands

    @property
    def device(self):
        return self._device()

    @property
    def command_controller(self):
        return self._command_controller()

    def reply(self, reply: bytes):
        return self.command_controller.reply(reply)

    def reply_raw(self, reply: bytes):
        return self.command_controller.reply_raw(reply)

    def reply_ok(self):
        return self.reply(self.OK)

    def reply_param_error(self):
        return self.reply(self.PARAM_ERROR)

    def reply_state_error(self):
        return self.reply(self.STATE_ERROR)


class at_wifi_basic_at_commands_t(base_at_commands_t):
    def __init__(self, device, command_controller):
        commands = \
        {
            b"AT"                   : { base_at_commands_t.EXECUTE:     self.ping},                     # Test at Startup
            b"AT+RST"               : { base_at_commands_t.EXECUTE:     self.reset},                    # Restart a module
            b"AT+GMR"               : { base_at_commands_t.EXECUTE:     self.version},                  # Check version information
            b"AT+CMD"               : { base_at_commands_t.QUERY:       self.list},                     # List all AT commands and types supported in current firmware
            b"AT+RESTORE"           : { base_at_commands_t.EXECUTE:     self.restore},                  # Restore factory default settings of the module
            b"ATE0"                 : { base_at_commands_t.EXECUTE:     lambda args: self.echo(False)}, # Turn echo off
            b"ATE1"                 : { base_at_commands_t.EXECUTE:     lambda args: self.echo(True)},  # Turn echo on
        }
        super().__init__(device, command_controller, commands)

    def ping(self, args):
        self.reply_ok()

    def reset(self, args):
        self.reply_ok()
        self.device.restart()

    def version(self, args):
        self.reply(self.device.version)
        self.reply_ok()

    def list(self, args):
        lines = []
        for index, (cmd_name, funcs) in enumerate(self.command_controller.commands.items()):
            support_test_command    = 1 if funcs.get(base_at_commands_t.TEST)    else 0
            support_query_command   = 1 if funcs.get(base_at_commands_t.QUERY)   else 0
            support_set_command     = 1 if funcs.get(base_at_commands_t.SET)     else 0
            support_execute_command = 1 if funcs.get(base_at_commands_t.EXECUTE) else 0
            line = f"+CMD:{index},{cmd_name.decode()},{support_test_command},{support_query_command},{support_set_command},{support_execute_command}"
            lines.append(line.encode())
        lines_str = self.device.EOL.join(lines)
        self.reply(lines_str)
        self.reply_ok()

    def restore(self, args):
        self.reply_ok()
        self.device.restore()
        self.device.restart()
        time.sleep(0.5)
        self.reply(b"ready")

    def echo(self, is_echo:bool):
        self.device.is_echo = bool(is_echo)
        self.reply_ok()


class at_wifi_at_commands_t(base_at_commands_t):

    class ECN(enum.Enum):
        OPEN            = 0
        WEP             = 1
        WPA_PSK         = 2
        WPA2_PSK        = 3
        WPA_WPA2_PSK    = 4
        WPA2_ENTERPRISE = 5
        WPA3_PSK        = 6
        WPA2_WPA3_PSK   = 7
        WAPI_PSK        = 8
        OWE             = 9

    class CIPHER(enum.Enum):
        NONE         = 0
        WEP40        = 1
        WEP104       = 2
        TKIP         = 3
        CCMP         = 4
        TKIP_CCMP    = 5
        AES_CMAC_128 = 6
        UNKNOWN      = 7

    class BGN(enum.Enum):
        B = (1<<0)
        G = (1<<1)
        N = (1<<2)

    def __init__(self, device, command_controller):
        commands = \
        {
            b"AT+CWINIT"            : { base_at_commands_t.QUERY:       self.is_init,
                                        base_at_commands_t.SET:         self.init},         # Initialize/Deinitialize Wi-Fi driver
            b"AT+CWMODE"            : { base_at_commands_t.QUERY:       self.is_mode,
                                        base_at_commands_t.SET:         self.mode},
            b"AT+CWSTATE"           : { base_at_commands_t.QUERY:       self.info},         # Query the Wi-Fi state and Wi-Fi information
            b"AT+CWJAP"             : { base_at_commands_t.QUERY:       self.is_connect,
                                        base_at_commands_t.SET:         self.connect_info,
                                        base_at_commands_t.EXECUTE:     self.connect},      # Connect to an AP
            b"AT+CWQAP"             : { base_at_commands_t.EXECUTE:     self.disconnect},   # Disconnect from an AP
            b"AT+CWCOUNTRY"         : { base_at_commands_t.QUERY:       self.get_country,
                                        base_at_commands_t.SET:         self.set_country},
            b"AT+SYSTIMESTAMP"      : { base_at_commands_t.QUERY:       self.systime },     # Get current time
            b"AT+CWLAP"             : { base_at_commands_t.EXECUTE:     self.list_ap },     # List wifi stations
        }
        super().__init__(device, command_controller, commands)

    def is_init(self, args):
        self.reply(b"+CWINIT:%u"% (1 if self.device.wifi.init else 0))
        self.reply_ok()

    def init(self, args):
        try:
            val = int(args)
        except ValueError:
            self.reply_param_error()
            return
        if val < 0 or val > 1:
            self.reply_param_error()
            return
        self.device.wifi.init = bool(val)
        self.reply_ok()

    def is_mode(self, args):
        self.reply(b"+CWMODE:%u"% (self.device.wifi.mode.value))

    def mode(self, args):
        list_args = args.split(b",")
        if len(list_args) < 1:
            self.reply_param_error()
            return
        mode,*_ = list_args
        try:
            mode = int(mode)
        except ValueError:
            self.reply_param_error()
            return
        if mode > len(at_wifi_info_t.MODES):
            self.reply_param_error()
            return
        self.device.wifi.mode = at_wifi_info_t.MODES(mode)
        self.reply_ok()

    def info(self, args):
        info = f"+CWSTATE:{self.device.wifi.state.value},{self.device.wifi.ssid}"
        self.reply(info.encode())
        self.reply_ok()

    def is_connect(self, args):
        wifi = self.device.wifi
        if wifi.state == wifi.STATES.NOT_CONN or wifi.state == wifi.STATES.DISCONNECTED:
            self.reply_state_error()
            return
        info = f"CWJAP:{wifi.ssid},{wifi.bssid},<channel>,<rssi>,{wifi.pci_en},{wifi.reconn_interval},{wifi.listen_interval},{wifi.scan_mode},{wifi.pmf}"
        self.reply(info.encode())
        self.reply_ok()

    def connect_info(self, args):
        list_args = args.split(b",")
        wifi = self.device.wifi
        if wifi.state != wifi.STATES.NOT_CONN and wifi.state != wifi.STATES.DISCONNECTED:
            self.reply_state_error()
            return
        if len(list_args) < 2:
            self.reply_param_error()
            return
        ssid,pwd,*_ = list_args
        if not ssid.startswith(b"\"") or not ssid.endswith(b"\""):
            self.reply_param_error()
            return
        ssid = ssid[1:-1].decode()
        if not pwd.startswith(b"\"") or not pwd.endswith(b"\""):
            self.reply_param_error()
            return
        pwd = pwd[1:-1].decode()
        logger.info(f"Connecting to SSID: {ssid} with PWD: {pwd}")
        """ Will now block """
        time.sleep(2)
        self.reply(b"WIFI CONNECTED")
        time.sleep(0.5)
        self.reply(b"WIFI GOT IP")
        time.sleep(0.5)
        self.reply_ok()

    def connect(self, args):
        wifi = self.device.wifi
        if not wifi.init or not len(wifi.ssid):
            self.reply_state_error()
            return
        self.device.wifi_connect(True)
        self.reply(b"WIFI CONNECTED")
        self.reply(b"WIFI GOT IP")
        self.reply_ok()

    def disconnect(self, args):
        wifi = self.device.wifi
        if wifi.state != wifi.STATES.CONNECTED and wifi.state != wifi.STATES.CONNECTING and wifi.state != wifi.STATES.NO_IP:
            self.reply_state_error()
            return
        self.device.wifi_connect(False)
        self.reply_ok()

    def get_country(self, args):
        wifi = self.device.wifi
        info = f"+CWCOUNTRY:{wifi.country_policy},{wifi.country_code},{wifi.start_channel},{wifi.total_channel_count}"
        self.reply(info.encode())
        self.reply_ok()

    def set_country(self, args):
        list_args = args.split(b",")
        logger.info(f"{list_args = }")
        if len(list_args) != 4:
            self.reply_param_error()
            logger.info(f"Wrong argument count to set country {list_args}")
            return
        wifi = self.device.wifi
        country_policy, country_code, start_channel, total_channel_count = list_args
        try:
            country_policy = int(country_policy)
        except ValueError:
            logger.info(f"Country Policy is not an integer '{country_policy}'")
            self.reply_param_error()
            return
        if country_policy not in [0, 1]:
            logger.info(f"Country Policy is invalid '{country_policy}'")
            self.reply_param_error()
            return
        country_code = country_code.decode()
        country_code = country_code.replace('"','')
        if country_code not in wifi.COUNTRY_CODES:
            logger.info(f"Country Code is invalid '{country_code}'")
            self.reply_param_error()
            return
        try:
            start_channel = int(start_channel)
        except ValueError:
            logger.info(f"Start channel is not an integer '{start_channel}'")
            self.reply_param_error()
            return
        if start_channel < wifi.MIN_CHANNEL:
            logger.info(f"Start channel is lower than minimum channel {start_channel} < {wifi.MIN_CHANNEL}")
            self.reply_param_error()
            return
        elif start_channel > wifi.MAX_CHANNEL:
            logger.info(f"Start channel is higher than minimum channel {start_channel} > {wifi.MAX_CHANNEL}")
            self.reply_param_error()
            return
        try:
            total_channel_count = int(total_channel_count)
        except ValueError:
            logger.info(f"Channel count is not an integer '{total_channel_count}'")
            self.reply_param_error()
            return
        if total_channel_count < 1:
            logger.info(f"Channel count is not a positive integer '{total_channel_count}'")
            self.reply_param_error()
            return
        maximum_channel = start_channel + total_channel_count
        if maximum_channel > wifi.MAX_CHANNEL:
            logger.info(f"Maximum channel is higher than minimum channel {start_channel} {total_channel_count} = {maximum_channel} > {wifi.MAX_CHANNEL}")
            self.reply_param_error()
            return
        wifi.country_policy = country_policy
        wifi.country_code = country_code
        wifi.start_channel = start_channel
        wifi.total_channel_count = total_channel_count
        self.reply_ok()


    def systime(self, args):
        self.reply(f"+SYSTIMESTAMP:{int(time.time())}".encode())
        self.reply_ok()

    def list_ap(self, args):
        ap_stations = [
            {
            "encryption"        : at_wifi_at_commands_t.ECN.WPA2_WPA3_PSK.value,
            "ssid"              : "Devtank Wifi",
            "rssi"              : -71,
            "mac"               : "08:65:87:ec:a0:e8",
            "channel"           : 1,
            "freq_offset"       : -1,
            "freqcal_val"       :-1,
            "pairwise_cipher"   : at_wifi_at_commands_t.CIPHER.CCMP.value,
            "group_cipher"      : at_wifi_at_commands_t.CIPHER.CCMP.value,
            "bgn"               : at_wifi_at_commands_t.BGN.B.value | at_wifi_at_commands_t.BGN.G.value | at_wifi_at_commands_t.BGN.N.value,
            "wps"               : 0
            },
        ]
        time.sleep(0.5)
        for ap in ap_stations:
            self.reply(f"+CWLAP:({ap['encryption']},\"{ap['ssid']}\",{ap['rssi']},\"{ap['mac']}\",{ap['channel']},{ap['freq_offset']},{ap['freqcal_val']},{ap['pairwise_cipher']},{ap['group_cipher']},{ap['bgn']},{ap['wps']})".encode())
            time.sleep(0.1)
        self.reply_ok()


class at_wifi_cips_at_commands_t(base_at_commands_t):
    def __init__(self, device, command_controller):
        commands = \
        {
            b"AT+CIPSNTPCFG"        : { base_at_commands_t.SET:         self.sntp_set,
                                        base_at_commands_t.QUERY:       self.sntp_query},   # Query/Set the time zone and SNTP server
        }
        super().__init__(device, command_controller, commands)

    def sntp_set(self, args):
        list_args = args.split(b",")
        if len(list_args) < 3:
            self.reply_param_error()
            return
        _,_,*sntp_servers = list_args
        for server in sntp_servers:
            if not server.startswith(b"\"") or not server.endswith(b"\""):
                self.reply_param_error()
                return
        self.device.sntp.servers = [serv[1:-1].decode() for serv in sntp_servers]
        self.reply_ok()
        time.sleep(3)
        self.reply(b"+TIME_UPDATED")

    def sntp_query(self, args):
        sntp = self.device.sntp
        servers = ",".join(sntp.servers)
        info = f"+CIPSNTPCFG:{sntp.enable},{sntp.timezone},{sntp.servers}"
        self.reply(info.encode())
        self.reply_ok()


class at_wifi_mqtt_at_commands_t(base_at_commands_t):
    def __init__(self, device, command_controller):
        commands = \
        {
            b"AT+MQTTUSERCFG"       : { base_at_commands_t.SET:         self.user_config},  # Set MQTT User Configuration
            b"AT+MQTTCONNCFG"       : { base_at_commands_t.SET:         self.conn_config},  # Set Configuration of MQTT Connection
            b"AT+MQTTCONN"          : { base_at_commands_t.QUERY:       self.is_conn,
                                        base_at_commands_t.SET:         self.conn},         # Connect to MQTT Brokers
            b"AT+MQTTPUB"           : { base_at_commands_t.SET:         self.publish},      # Publish MQTT Messages in String
            b"AT+MQTTSUB"           : { base_at_commands_t.QUERY:       self.is_subscribed,
                                        base_at_commands_t.SET:         self.subscribe},    # Connect to MQTT Brokers
            b"AT+MQTTPUBRAW"        : { base_at_commands_t.SET:         self.publish_raw},  # Publish MQTT Messages in String
        }
        super().__init__(device, command_controller, commands)

    def user_config(self, args):
        pattern = r'\".*?\"|\S+'
        matches = re.split(r'{pattern}', args.decode())
        x = str(matches).replace("'", "")
        a = ast.literal_eval(x)
        arg_list = [i.encode() if type(i) == str else i for i in a]
        if len(arg_list) < 8:
            self.reply_param_error()
            return
        _, scheme, client_id, username, password, _, _, ca_path = arg_list
        try:
            scheme = int(scheme)
        except ValueError:
            self.reply_param_error()
            return
        if scheme not in set(s.value for s in at_wifi_mqtt_t.SCHEMES):
            # Unknown scheme
            self.reply_param_error()
            return
        mqtt_obj = self.device.mqtt
        mqtt_obj.scheme = at_wifi_mqtt_t.SCHEMES(scheme)
        mqtt_obj.client_id = client_id.decode()
        mqtt_obj.user = username.decode()
        mqtt_obj.pwd = password.decode()
        mqtt_obj.ca = ca_path.decode()
        mqtt_obj.reinit_client()
        self.reply_ok()

    def conn_config(self, args):
        arg_list = args.split(b",")
        if len(arg_list) < 7:
            self.reply_param_error()
            return
        _, _, _, lwt_topic, lwt_msg, _, _ = arg_list
        self.reply_ok()

    def is_conn(self, args):
        mqtt_obj = self.device.mqtt
        info = f"+MQTTCONN:0,{mqtt_obj.state},{mqtt_obj.state},\"{mqtt_obj.addr}\",{mqtt_obj.port},\"{mqtt_obj.ca}\",1"
        self.reply(info.encode())
        self.reply_ok()

    def conn(self, args):
        arg_list = args.split(b",")
        if len(arg_list) < 4:
            self.reply_param_error()
            return
        _, addr, port, _ = arg_list
        try:
            port = int(port)
        except ValueError:
            self.reply_param_error()
            return
        mqtt_obj = self.device.mqtt
        mqtt_obj.addr = addr.decode()[1:-1]
        mqtt_obj.port = port
        try:
            r = mqtt_obj.connect()
        except ValueError as e:
            logger.error(e)
            self.reply_param_error()
            return
        if not r:
            self.reply_param_error()
            return
        self.reply_ok()

    def publish(self, topic, payload):
        if not self.device.mqtt.publish(topic, payload):
            self.reply_state_error()
            return
        self.reply_ok()

    def is_subscribed(self, args):
        mqtt_obj = self.device.mqtt
        for sub in mqtt_obj.subscriptions:
            sub_info = f"+MQTTSUB:0,{mqtt_obj.state},\"{sub}\",0"
            self.reply(sub_info.encode())
        self.reply_ok()

    def subscribe(self, args):
        arg_list = args.split(b",")
        if len(arg_list) != 3:
            self.reply_param_error()
            return
        _, topic, _ = arg_list
        curr_subs = self.device.mqtt.subscriptions
        topic = topic[1:-1]
        if topic in curr_subs:
            self.reply_param_error()
            return
        if not self.device.mqtt.subscribe(topic):
            self.reply_param_error()
            return
        self.reply_ok()

    def publish_raw(self, args):
        arg_list = args.split(b",")
        if len(arg_list) != 5:
            self.reply_param_error()
            return
        _,topic,length,_,_ = arg_list
        try:
            length = int(length)
        except ValueError:
            self.reply_param_error()
            return
        self.reply_ok()
        self.reply_raw(b">")
        logger.info("START READ")
        payload = self.device.read_blocking(length, timeout=1)
        topic = topic.decode()
        if topic.startswith("\""):
            topic = topic[1:]
        if topic.endswith("\""):
            topic = topic[:-1]
        self.device.mqtt.publish(topic, payload)
        self.reply(b"+MQTTPUB:OK")


class at_commands_t(object):

    NO_COMMAND      = "AT+ERROR: CMD"
    INVALID_TYPE    = "AT+ERROR: TYPE"

    def __init__(self, device, types:list):
        self._device = weakref.ref(device)
        self.commands = {}
        for type_ in types:
            new_obj = type_(device, self)
            self.commands.update(new_obj.commands)

    @property
    def device(self):
        return self._device()

    def reply(self, reply: bytes):
        return self.device.send(reply)

    def reply_raw(self, reply: bytes):
        return self.device.send_raw(reply)

    def _no_command(self):
        self.reply(self.NO_COMMAND)

    def _invalid_type(self):
        self.reply(self.INVALID_TYPE)

    def command(self, command):
        # Get type of function
        type_ = base_at_commands_t.EXECUTE
        keyword = command
        args = b""
        if b"=" in command:
            # Is set
            type_ = base_at_commands_t.SET
            keyword,args = command.split(b"=")
        elif command.endswith(b"?"):
            type_ = base_at_commands_t.QUERY
            keyword,*_ = command.split(b"?")
            args = b""
        funcs = self.commands.get(keyword)
        if funcs:
            cb = funcs.get(type_)
            if cb:
                cb(args)
            else:
                logger.error(f"Unknown type {type_} for command {command}")
                self._invalid_type()
        else:
            # Not found in commands
            logger.error(f"Unknown command {command}")
            self._no_command()


class at_wifi_info_t(object):

    STATES = enum.Enum(
        "STATES", [
            "NOT_CONN",
            "NO_IP",
            "CONNECTED",
            "CONNECTING",
            "DISCONNECTED",
        ])

    MODES = enum.Enum(
        "MODES", [
            "Station",
            "SoftAP",
            "SoftAP_Station",
        ])

    COUNTRY_CODES = [
        "AD", "AE", "AF", "AG", "AI", "AL", "AM", "AO", "AQ", "AR",
        "AS", "AT", "AU", "AW", "AX", "AZ", "BA", "BB", "BD", "BE",
        "BF", "BG", "BH", "BI", "BJ", "BL", "BM", "BN", "BO", "BQ",
        "BR", "BS", "BT", "BV", "BW", "BY", "BZ", "CA", "CC", "CD",
        "CF", "CG", "CH", "CI", "CK", "CL", "CM", "CN", "CO", "CR",
        "CU", "CV", "CW", "CX", "CY", "CZ", "DE", "DJ", "DK", "DM",
        "DO", "DZ", "EC", "EE", "EG", "EH", "ER", "ES", "ET", "FI",
        "FJ", "FK", "FM", "FO", "FR", "GA", "GB", "GD", "GE", "GF",
        "GG", "GH", "GI", "GL", "GM", "GN", "GP", "GQ", "GR", "GS",
        "GT", "GU", "GW", "GY", "HK", "HM", "HN", "HR", "HT", "HU",
        "ID", "IE", "IL", "IM", "IN", "IO", "IQ", "IR", "IS", "IT",
        "JE", "JM", "JO", "JP", "KE", "KG", "KH", "KI", "KM", "KN",
        "KP", "KR", "KW", "KY", "KZ", "LA", "LB", "LC", "LI", "LK",
        "LR", "LS", "LT", "LU", "LV", "LY", "MA", "MC", "MD", "ME",
        "MF", "MG", "MH", "MK", "ML", "MM", "MN", "MO", "MP", "MQ",
        "MR", "MS", "MT", "MU", "MV", "MW", "MX", "MY", "MZ", "NA",
        "NC", "NE", "NF", "NG", "NI", "NL", "NO", "NP", "NR", "NU",
        "NZ", "OM", "PA", "PE", "PF", "PG", "PH", "PK", "PL", "PM",
        "PN", "PR", "PS", "PT", "PW", "PY", "QA", "RE", "RO", "RS",
        "RU", "RW", "SA", "SB", "SC", "SD", "SE", "SG", "SH", "SI",
        "SJ", "SK", "SL", "SM", "SN", "SO", "SR", "SS", "ST", "SV",
        "SX", "SY", "SZ", "TC", "TD", "TF", "TG", "TH", "TJ", "TK",
        "TL", "TM", "TN", "TO", "TR", "TT", "TV", "TW", "TZ", "UA",
        "UG", "UM", "US", "UY", "UZ", "VA", "VC", "VE", "VG", "VI",
        "VN", "VU", "WF", "WS", "YE", "YT", "ZA", "ZM", "ZW",
    ]

    MIN_CHANNEL = 1
    MAX_CHANNEL = 14

    def __init__(self):
        self.init               = False
        self.mode               = 3 # Auto
        self.ssid               = ""
        self.pwd                = ""
        self.bssid              = ""
        self.pci_en             = ""
        self.reconn_interval    = 0
        self.listen_interval    = 3
        self.scan_mode          = 1
        self.jap_timeout        = 15
        self.pmf                = 1
        self.state              = self.STATES.NOT_CONN
        self.mode               = self.MODES.SoftAP
        self.country_code       = "GB"
        self.start_channel      = 1
        self.total_channel_count = 13


class at_wifi_sntp_info_t(object):
    def __init__(self):
        self.enabled = True
        self.timezone = 0
        self.servers = []


class at_wifi_mqtt_t(object):

    SCHEMES = enum.Enum(
        "SCHEMES", [
            "TCP",
            "TLS_NO_CERT",
            "TLS_VERIFY_SERVER",
            "TLS_PROVIDE_CLIENT",
            "TLS_BOTH",
            "WS",
            "WSS_NO_CERT",
            "WSS_VERIFY_SERVER",
            "WSS_PROVIDE_CLIENT",
            "WSS_BOTH",
        ])

    STATES = enum.Enum(
        "STATES", [
            "UNINIT",
            "SET_USER_CFG",
            "SET_CONN_CFG",
            "DISCONNECTED",
            "CONN_EST",
            "CONN_NO_SUB",
            "CONN_WITH_SUB",
        ])

    def __init__(self, device, selector):
        self._device = weakref.ref(device)
        self.scheme = self.SCHEMES.TCP
        self.addr = None
        self.client_id = None
        self.user = None
        self.pwd = None
        self.ca = None
        self.port = 1883
        self.subscriptions = []
        self._pending_subscription = None
        self._connected = False
        self.state = self.STATES.UNINIT
        self._selector = selector
        self.reinit_client()

    @property
    def device(self):
        return self._device()

    def reinit_client(self):
        was_connected = self._connected
        self._connected = False
        if was_connected:
            self.client.disconnect()
            self.state = self.STATES.UNINIT

        if int(paho.__version__.split(".")[0]) > 1:
            self.client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
        else:
            self.client = mqtt.Client()
        logger.info(f"MQTT Scheme: {self.scheme}")
        self.client.transport = "websockets" if self.scheme in [
            self.SCHEMES.WS,
            self.SCHEMES.WSS_NO_CERT,
            self.SCHEMES.WSS_VERIFY_SERVER,
            self.SCHEMES.WSS_PROVIDE_CLIENT,
            self.SCHEMES.WSS_BOTH ]  else "tcp"
        logger.info(f"MQTT transport: {self.client.transport}")
        self.client.on_connect = self._on_connect
        self.client.on_subscribe = self._on_subscribe
        self.client.on_message = self._on_message
        self.client.on_socket_open = self._on_socket_open
        self.client.on_socket_close = self._on_socket_close

        if self.scheme in [self.SCHEMES.TLS_VERIFY_SERVER,
                           self.SCHEMES.WSS_VERIFY_SERVER]:
            self.client.tls_set(ca_certs=self.ca)
            logger.info("Enable SSL for connection with CA.")
        elif self.scheme in [self.SCHEMES.TLS_NO_CERT,
                             self.SCHEMES.WSS_NO_CERT]:
            logger.info("Enable SSL for connection with no CA.")
            self.client.tls_set(cert_reqs=ssl.CERT_NONE)
            self.client.tls_insecure_set(True)
        else:
            logger.info("Disable SSL for connection.")

        if was_connected:
            self.connect()

    def __del__(self):
        self.close()

    def close(self):
        pass

    def _is_valid(self):
        return (self.addr   is not None and
            self.client_id  is not None and
            self.user       is not None and
            self.pwd        is not None and
            self.ca         is not None)

    def connect(self, timeout: float = 2) -> bool:
        if not self._is_valid():
            logger.info("Not valid")
            logger.info(f"{self.addr      = }")
            logger.info(f"{self.client_id = }")
            logger.info(f"{self.user      = }")
            logger.info(f"{self.pwd       = }")
            logger.info(f"{self.ca        = }")
            return False

        if self.scheme not in [self.SCHEMES.TCP,
                               self.SCHEMES.TLS_NO_CERT,
                               self.SCHEMES.TLS_VERIFY_SERVER,
                               self.SCHEMES.WSS_VERIFY_SERVER,
                               self.SCHEMES.WSS_NO_CERT]:
            logger.info("Scheme current unsupported.")
            return False

        self.client.username_pw_set(self.user, password=self.pwd)
        try:
            self.client.connect(self.addr, self.port, 60)
        except (ConnectionResetError, ConnectionRefusedError, OSError)  as e:
            logger.error(f"Connection Failed: {e}")
            return False
        except ConnectionRefusedError as e:
            logger.error(f"Connection Failed: {e}")
            return False

        self._connected = None
        end = time.monotonic() + timeout
        while not isinstance(self._connected, bool) and time.monotonic() < end:
            self.client.loop()
        logger.info(f"CONNECTED TO {self.user}:{self.pwd}@{self.addr}:{self.port} : {bool(self._connected)}")
        return bool(self._connected)

    def _on_connect(self, client, userdata, flags, rc, *args):
        # Not really sure what type of object properties should be
        properties = []
        if len(args):
            properties = args[0]
        logger.info(f"ON CONNECT; {rc = }")
        if rc == 0:
            self._connected = True
            self.state = self.STATES.CONN_NO_SUB
        else:
            self._connected = False

    def publish(self, topic, msg) -> bool:
        if not self._connected:
            return False
        logger.info(f"PUBLISH {topic}#{msg}")
        self.client.publish(topic, msg)
        return True

    def subscribe(self, topic, timeout=0.5):
        if not self._connected:
            return False
        if self._pending_subscription:
            return False
        topic = topic.decode()
        self.client.subscribe(topic)
        subscribed = False
        end = time.monotonic() + timeout
        self._pending_subscription = topic
        while topic not in self.subscriptions and time.monotonic() < end:
            self.client.loop()
        return topic in self.subscriptions

    def _on_subscribe(self, client, userdata, mid, rc, *args):
        # Not really sure what type of object properties should be
        properties = []
        if len(args):
            properties = args[0]
        logger.info(f"ON SUBSCRIBE")

        for sub_result in rc:
            # Any reason code >= 128 is a failure.
            if sub_result >= 128:
                logger.info(f"FAILED SUBSCRIBE {sub_result = }")
            else:
                if self._pending_subscription:
                    self.subscriptions.append(self._pending_subscription)
                    logger.info(f"SUBSCRIBED {self._pending_subscription}")
                    self.state = self.STATES.CONN_WITH_SUB
                    self._pending_subscription = None

    def _on_message(self, client, userdata, message):
        topic = message.topic
        payload = message.payload.decode()
        logger.info(f"ON MESSAGE: '{topic}':'{payload}'")
        msg = f"+MQTTSUBRECV:0,\"{topic}\",{len(payload)},{payload}"
        self.device.send(msg)

    def _on_socket_open(self, mqttc, userdata, sock):
        self._selector.register(sock, selectors.EVENT_READ, self.event)

    def _on_socket_close(self, mqttc, userdata, sock):
        self._selector.unregister(sock)

    def event(self):
        self.client.loop()

    def loop(self):
        self.client.loop_misc()


class at_wifi_dev_t(object):

    EOL             = b"\r\n"
    AT_VERSION      = "AT version (FAKE):2.2.0.0"
    SDK_VERSION     = "v4.0.1 (FAKE)"
    COMPILE_TIME    = "compile time:Oct 16 2023 12:18:22"
    BIN_VERSION     = "2.1.0 (FAKE)"
    BOOTTIME        = 2.
    RESET_PIN_POLL  = 0.0005

    def __init__(self, port, reset_pin_path):
        self.port = port

        self._selector = selectors.PollSelector()

        self._reset = False

        self._reset_pin_path = reset_pin_path

        self._reset_pin_poll_fd = timerfd.timerfd_t(self.RESET_PIN_POLL)
        self._reset_pin_poll_fd.start()
        self._selector.register(self._reset_pin_poll_fd, selectors.EVENT_READ, self._reset_pin_event)

        if isinstance(port, int):
            self._serial = os.fdopen(port, "rb", 0)
        elif os.path.exists(port):
            self._serial = serial.Serial(port)
        else:
            raise IOError(f"Don't know what to do with {port}")

        tty.setraw(self._serial)

        self.done = False
        self._serial_in = b""
        self.is_echo = True
        self.sntp = at_wifi_sntp_info_t()
        self.wifi = at_wifi_info_t()
        self.mqtt = at_wifi_mqtt_t(self, self._selector)

        command_types = [
            at_wifi_basic_at_commands_t,
            at_wifi_at_commands_t,
            at_wifi_cips_at_commands_t,
            at_wifi_mqtt_at_commands_t
            ]
        self._command_obj = at_commands_t(self, command_types)
        logger.info("AT WIFI INITED")

        self._selector.register(self._serial, selectors.EVENT_READ, self._serial_in_event)

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        self.close()

    def __del__(self):
        self.close()

    def close(self):
        if self._reset_pin_poll_fd:
            self._reset_pin_poll_fd.stop()
            self._reset_pin_poll_fd = None
        if self._serial:
            self._serial.close()
            self._serial = None

    def send_raw(self, msg:bytes):
        logger.debug(f">> {msg}")
        return os.write(self._serial.fileno(), msg)

    def send(self, msg:bytes):
        if isinstance(msg, str):
            msg = msg.encode()
        if not msg.endswith(self.EOL):
            msg += self.EOL
        return self.send_raw(msg)

    def _do_command(self, line):
        self._command_obj.command(line)

    def _read_serial(self, fd):
        char = os.read(fd, 1)
        if self.is_echo:
            os.write(fd, char)
        self._serial_in += char
        eol = b'\r' # Not sure why this isn't self.EOL \r\n
        if self._serial_in.endswith(eol):
            line = self._serial_in[:-len(eol)]
            if line.startswith(b'\n'):
                line = line[1:]
            logger.debug(f"<< {line}")
            self._do_command(line)
            self._serial_in = b""

    def read_blocking(self, length, timeout=0.1):
        msg = b""
        end = time.monotonic() + timeout
        fd = self._serial.fileno()
        while len(msg) < length and time.monotonic() < end:
            rlist,wlist,xlist = select.select([fd], [], [], timeout)
            for r in rlist:
                msg += os.read(fd, 1)
        logger.debug(f"<< {msg}")
        return msg

    def _reset_pin_event(self):
        self._reset_pin_poll_fd.reset()
        if os.path.exists(self._reset_pin_path):
            with open(self._reset_pin_path) as rs:
                v = rs.read()
            try:
                w = bool(int(v))
            except ValueError:
                w = False
            if w and self._reset is False:
                self._reset = time.monotonic()
            elif not w and self.reset_pin:
                self._reset = False

    @property
    def reset_pin(self):
        return self._reset is None or bool(self._reset)

    def loop_rest_pin(self):
        if self._reset and time.monotonic() > self._reset + self.BOOTTIME:
            self._reset = None
            self.restore()
            self.send(b"ready")

    def _serial_in_event(self):
        self._read_serial(self._serial.fileno())

    def run_forever(self):
        while not self.done:
            self.loop_rest_pin()
            self.mqtt.loop()
            events = self._selector.select(timeout=0.01)
            for key, mask in events:
                key.data()

    def restart(self):
        """ TODO: Make some kind of restart """
        pass

    @property
    def version(self):
        version = self.AT_VERSION.encode()  + self.EOL \
            + self.SDK_VERSION.encode()     + self.EOL \
            + self.COMPILE_TIME.encode()    + self.EOL \
            + self.BIN_VERSION.encode()     + self.EOL
        return version

    def restore(self):
        """ TODO: Make some kind of restore """
        pass

    def wifi_connect(self, connect:bool):
        self.wifi.state = wifi.CONNECTED if connect else wifi.DISCONNECTED


def run_standalone(tty_path, resetpin):
    directory = os.path.dirname(tty_path, resetpin)
    if not os.path.exists(directory):
        os.mkdir(directory)
    if os.path.islink(tty_path):
        a = os.access(tty_path, os.F_OK)
        if a:
            logger.error("Symlink already exists")
            return -1
        os.unlink(tty_path)
    master, slave = os.openpty()
    os.symlink(os.ttyname(slave), tty_path)
    logger.info(f"Connect to: {tty_path}")
    port = os.ttyname(master)
    dev = at_wifi_dev_t(master)
    try:
        dev.run_forever()
    except KeyboardInterrupt:
        print("at_wifi_dev_t : Caught keyboard interrupt, exiting")
    os.unlink(tty_path)
    return 0

def run_dependent(tty_path, resetpin):
    dev = at_wifi_dev_t(tty_path, resetpin)
    try:
        dev.run_forever()
    except KeyboardInterrupt:
        print("at_wifi_dev_t : Caught keyboard interrupt, exiting")
    return 0

def main():
    import argparse

    osm_loc = os.environ.get("OSM_LOC", "/tmp/osm")
    if not os.path.exists(osm_loc):
        os.mkdir(osm_loc)
    DEFAULT_VIRTUAL_COMMS_PATH = os.path.join(osm_loc, "UART_COMMS_slave")

    RESET_PIN = 10

    DEFAULT_RESET_PIN_PATH = os.path.join(osm_loc, "gpios", f"gpio_{RESET_PIN}")

    is_debug = os.environ.get("DEBUG")

    if is_debug:
        logger.setLevel(level=logging.DEBUG)
        logger.debug("Debug enabled")
        if is_debug == "file":
            log_file = os.path.join(osm_loc, "at_wifi_py.log")
            logger.debug(f"Logging to: {log_file}")
            logger.addHandler(logging.FileHandler(log_file))

    def get_args():
        parser = argparse.ArgumentParser(description='Fake AT Wifi Module.' )
        parser.add_argument('-s', '--standalone', help="If this should spin up its own pseudoterminal or use an existing one", action='store_true')
        parser.add_argument('-r', '--resetpin', metavar='RST', type=str, nargs=1, help='The reset pin path for this device', default=DEFAULT_RESET_PIN_PATH)
        parser.add_argument('pseudoterminal', metavar='PTY', type=str, nargs='?', help='The pseudoterminal for the fake AT wifi module', default=DEFAULT_VIRTUAL_COMMS_PATH)
        return parser.parse_args()

    args = get_args()

    if args.standalone:
        return run_standalone(args.pseudoterminal, args.resetpin)
    else:
        return run_dependent(args.pseudoterminal, args.resetpin)

if __name__ == '__main__':
    sys.exit(main())
