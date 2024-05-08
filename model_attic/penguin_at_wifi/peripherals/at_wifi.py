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

logging.basicConfig(level=logging.ERROR)

logger = logging.getLogger("at_wifi.py")


class base_at_commands_t(object):
    OK          = "OK"
    PARAM_ERROR = "AT+ERROR: PARAM"
    STATE_ERROR = "AT+ERROR: STATE"

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
            b"AT+SYSTIMESTAMP"      : { base_at_commands_t.QUERY:       self.systime },     # Get current time
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

    def systime(self, args):
        self.reply(f"+SYSTIMESTAMP:{int(time.time())}".encode())
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
        arg_list = args.split(b",")
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
        mqtt_obj.user = username.decode()[1:-1]
        mqtt_obj.pwd = password.decode()[1:-1]
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
        if not mqtt_obj.connect():
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
        self.device.mqtt.publish(topic.decode(), payload)
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

    def __init__(self, selector):
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

    def reinit_client(self):
        was_connected = self._connected
        self._connected = False
        if was_connected:
            self.client.disconnect()
            self.state = self.STATES.UNINIT

        if int(paho.__version__.split(".")[0]) > 1:
            self.client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION1)
        else:
            self.client = mqtt.Client()
        self.client.transport = "websockets" if self.scheme in [
            self.SCHEMES.WS,
            self.SCHEMES.WSS_NO_CERT,
            self.SCHEMES.WSS_VERIFY_SERVER,
            self.SCHEMES.WSS_PROVIDE_CLIENT,
            self.SCHEMES.WSS_BOTH ]  else "tcp"
        self.client.on_connect = self._on_connect
        self.client.on_subscribe = self._on_subscribe
        self.client.on_socket_open = self._on_socket_open
        self.client.on_socket_close = self._on_socket_close

        if self.scheme in [self.SCHEMES.TLS_VERIFY_SERVER,
                           self.SCHEMES.WSS_VERIFY_SERVER]:
            self.client.tls_set(ca_certs=self.ca)
        elif self.scheme in [self.SCHEMES.TLS_NO_CERT,
                             self.SCHEMES.WSS_NO_CERT]:
            self.client.tls_set(cert_reqs=ssl.CERT_NONE)
            self.client.tls_insecure_set(True)

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
        self.client.connect(self.addr, self.port, 60)
        self._connected = None
        end = time.monotonic() + timeout
        while not isinstance(self._connected, bool) and time.monotonic() < end:
            self.client.loop()
        logger.info(f"CONNECTED TO {self.user}:{self.pwd}@{self.addr}:{self.port}")
        return bool(self._connected)

    def _on_connect(self, client, userdata, flags, rc):
        logger.info(f"ON CONNECT; {rc = }");
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

    def _on_subscribe(self, client, userdata, mid, granted_qos):
        logger.info(f"ON SUBSCRIBE")
        if self._pending_subscription:
            self.subscriptions.append(self._pending_subscription)
            logger.info(f"SUBSCRIBED {self._pending_subscription}")
            self.state = self.STATES.CONN_WITH_SUB
            self._pending_subscription = None

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

    def __init__(self, port):
        self.port = port

        self._selector = selectors.PollSelector()

        if isinstance(port, int):
            self._serial = os.fdopen(port, "rb", 0)
        elif os.path.exists(port):
            self._serial = serial.Serial(port)
        else:
            raise IOError(f"Dont know what to do with {port}")

        tty.setraw(self._serial)

        self.done = False
        self._serial_in = b""
        self.is_echo = True
        self.sntp = at_wifi_sntp_info_t()
        self.wifi = at_wifi_info_t()
        self.mqtt = at_wifi_mqtt_t(self._selector)

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

    def _serial_in_event(self):
        self._read_serial(self._serial.fileno())

    def run_forever(self):
        while not self.done:
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


def run_standalone(tty_path):
    directory = os.path.dirname(tty_path)
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
        logger.error("\r")
    os.unlink(tty_path)
    return 0

def run_dependent(tty_path):
    dev = at_wifi_dev_t(tty_path)
    try:
        dev.run_forever()
    except KeyboardInterrupt:
        logger.error("\r")
    return 0

def main():
    import argparse

    osm_loc = os.environ.get("OSM_LOC", "/tmp/osm")
    if not os.path.exists(osm_loc):
        os.mkdir(osm_loc)
    DEFAULT_VIRTUAL_COMMS_PATH = os.path.join(osm_loc, "UART_COMMS_slave")

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
        parser.add_argument('pseudoterminal', metavar='PTY', type=str, nargs='?', help='The pseudoterminal for the fake AT wifi module', default=DEFAULT_VIRTUAL_COMMS_PATH)
        return parser.parse_args()

    args = get_args()

    if args.standalone:
        return run_standalone(args.pseudoterminal)
    else:
        return run_dependent(args.pseudoterminal)

if __name__ == '__main__':
    sys.exit(main())
