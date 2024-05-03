#!/usr/bin/env python3

import sys
import json
import socket
import select
import logging
import os

sys.path.append(os.path.abspath(os.path.join(os.path.dirname(__file__), os.path.pardir)))
try:
    from socket_server_base import socket_server_t
except ImportError:
    from .socket_server_base import socket_server_t

"""
Control intro message:
{
    "TYPE": "COMMAND"
}

Fake dev intro message:
{
    "TYPE": "FAKEDEV",
    "ID": "I2C",
    "DEVICES":
    {
        "VEML7700":
        [
            LUX
        ]
    }
}

From Command client:
{
    "GET":
    {
        "VEML7700":
        [
            "LUX"
        ],
        "HTU21D":
        [
            "TEMP",
            "HUMI"
        ]
    },
    "SET":
    {
        "VEML7700":
        {
            "LUX": 10
        },
        {
            "HTU21D":
            {
                "TEMP": 18,
                "HUMI": 23
            }
        }
    }
}
To command client:
{
    "GET":
    {
        "VEML7700":
        {
            "LUX": 10
        },
        "HTU21D":
        {
            "TEMP": 18,
            "HUMI": 23
        }
    },
    "SET":
    {
        "VEML7700":
        [
            "LUX"
        ],
        "HTU21D":
        [
            "TEMP",
            "HUMI"
        ]
    }
}
To fake device client:
{
    "FROM": 2,
    "GET":
    {
        "VEML7700":
        [
            "LUX":
        ],
    },
    "SET":
    {
        "VEML7700":
        {
            "LUX": 10
        }
    }
}
From fake device client:
{
    "FROM": 2,
    "GET":
    {
        "VEML7700":
        {
            "LUX": 10,
        }
    },
    "SET":
    {
        "VEML7700":
        [
            "LUX",
        ]
    }
}
"""
loc = os.getenv("OSM_LOC")
if not loc:
    loc = "/tmp/osm/"
DEFAULT_COMMAND_PORT    = f"{loc}command_socket"
CLIENT_TYPE_COMMAND     = "COMMAND"
CLIENT_TYPE_FAKE_DEVICE = "FAKEDEV"


class command_server_t(socket_server_t):


    def __init__(self, port=DEFAULT_COMMAND_PORT, logger=None):
        super().__init__(port, logger=logger)
        self._fake_devices = {}
        self._command_clients = {}
        self._logger.debug("INITIALISED COMMAND SERVER")

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        self.close()

    def close(self):
        clients = list(self._clients.values())
        for client in clients:
            self._close_client(client)

        self._server.close()

    def _read_data(self, client, data):
        try:
            data = data.decode()
        except UnicodeDecodeError:
            self._logger.error(f"Received {data} from client (fd:{client.fileno()}) can not be decoded.")
            return {}
        try:
            data_dict = json.loads(data)
        except json.decoder.JSONDecodeError:
            self._logger.error("Could not decode ")
            return {}
        return data_dict

    def _get_fake_dev_id(self, client):
        for fd,(fake_dev, id_, devs) in self._fake_devices.items():
            if client is fake_dev:
                return id_
        return None

    def _get_fake_dev_client(self, req_dev_):
        for fd,(fake_dev, id_, devs) in self._fake_devices.items():
            for dev_name in devs.keys():
                if req_dev_ == dev_name:
                    return fake_dev
        return None

    def _process_fake_devices(self, client, data):
        command_fd = data.get("FROM", None)
        dev_id = self._get_fake_dev_id(client)
        if command_fd is None:
            self._logger.critical(f"Fake device {self._get_fake_dev_id(client)} sent message without destination.")
            return
        command_client = self._command_clients.get(command_fd, None)
        if command_client is None:
            self._logger.error(f"Fake device {dev_id} sent message to perhaps outdated command client (fd:{command_fd}) (maybe closed?)")
            return
        resp_dict = {}
        get_info = data.get("GET", [])
        set_info = data.get("SET", None)
        msg_dict = {}
        if get_info:
            msg_dict["GET"] = get_info
        if set_info:
            msg_dict["SET"] = set_info
        msg_json = json.dumps(msg_dict)
        self._send_to_client(command_client, msg_json)

    def _process_command_clients(self, client, data):
        fd = client.fileno()
        get_vals = data.get("GET", {})
        set_vals = data.get("SET", {})

        for dev_id,params in get_vals.items():
            fake_dev = self._get_fake_dev_client(dev_id)
            if fake_dev is None:
                self._logger.error(f"Client (fd:{fd}) requested '{dev_id}' but that does not exist.")
                continue
            msg_dict = {"FROM" : fd}
            msg_dict["GET"] = {dev_id: params}
            msg = json.dumps(msg_dict)
            self._send_to_client(fake_dev, msg)

        for dev_id,params in set_vals.items():
            fake_dev = self._get_fake_dev_client(dev_id)
            if fake_dev is None:
                self._logger.error(f"Client (fd:{fd}) requested '{dev_id}' but that does not exist.")
                continue
            msg_dict = {"FROM" : fd}
            msg_dict["SET"] = {dev_id: params}
            msg = json.dumps(msg_dict)
            self._send_to_client(fake_dev, msg)

    def _process_single(self, client, data):
        self._logger.debug(f"DATA IN: {data}")
        data_dict = self._read_data(client, data)

        fd = client.fileno()

        if fd in self._fake_devices.keys():
            self._process_fake_devices(client, data_dict)
            return
        if fd in self._command_clients.keys():
            self._process_command_clients(client, data_dict)
            return

        type_ = data_dict.get("TYPE", None)
        if type_ is None:
            self._logger.warning(f"Client (fd:{fd}) has no type yet and attempted to send message without type.")
            self._close_client(client)
            return
        if type_ == CLIENT_TYPE_FAKE_DEVICE:
            # New fake device client
            id_ = data_dict.get("ID", None)
            if id_ is None:
                self._logger.warning(f"Client (fd:{fd}) has no ID yet and attempted to send message without ID.")
                self._close_client(client)
                return
            devices = data_dict.get("DEVICES", None)
            if devices is None:
                self._logger.warning(f"Client (fd:{fd}) did not publish devices.")
                self._close_client(client)
                return
            self._fake_devices[fd] = (client, id_, devices)
            self._logger.debug(f"Registered new client (fd:{fd}) for type {CLIENT_TYPE_FAKE_DEVICE} as '{id_}'")
        elif type_ == CLIENT_TYPE_COMMAND:
            # New command client
            self._command_clients[fd] = client
            self._logger.debug(f"Registered new client (fd:{fd}) for type {CLIENT_TYPE_COMMAND}")
        else:
            self._logger.warning(f"Client (fd:{fd}) attempted type '{type_}'.")
            self._close_client(client)
            return

    def _process(self, client, data):
        messages = data.split(b'\x04')
        for message in messages:
            if message:
                self._process_single(client, message)


class command_client_t(object):
    def __init__(self, port=None, logger=None):
        if port is None:
            port = DEFAULT_COMMAND_PORT
        self._conn = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        self._conn.connect(port)
        self.fileno = self._conn.fileno
        if logger is None:
            logger = logging.getLogger()
        self._logger = logger

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        self.close()

    def process(self):
        raise NotImplementedError

    def close(self):
        self._conn.close()

    def _send(self, msg):
        self._logger.debug(f"SENDING >> {msg}")
        # Add EOT
        EOT = '\x04'
        if isinstance(msg, bytes):
            EOT = b'\x04'
        if msg[-1] != EOT:
            msg += EOT
        self._conn.send(msg)

    def _recv(self, len_=1024):
        msg = self._conn.recv(len_)
        self._logger.debug(f"RECVING << {msg}")
        return msg


class fake_devs_command_client_t(command_client_t):
    def __init__(self, id_, mapping, port=None, logger=None):
        super().__init__(port=port, logger=logger)
        self.mapping = mapping
        devices = {}
        for dev,param_list in mapping.items():
            params = list(param_list)
            devices[dev] = params
        intro_msg = {"TYPE"     : CLIENT_TYPE_FAKE_DEVICE,
                     "ID"       : id_,
                     "DEVICES"  : devices}
        intro_msg_json = json.dumps(intro_msg)
        intro_msg_json = intro_msg_json.encode()
        self._logger.debug("Sending introduction message.")
        self._send(intro_msg_json)

    def process(self, fd, __mask):
        data = self._recv()
        if not data:
            return
        try:
            data = data.decode()
        except UnicodeDecodeError:
            self._logger.error(f"Could not decode '{data}'")
            return
        try:
            data_dict = json.loads(data)
        except json.decoder.JSONDecodeError:
            self._logger.error("Could not decode.")
            return
        from_fd = data_dict.get("FROM", None)
        if from_fd is None:
            self._logger.error("Message has no return destination.")
            return
        resp_dict = {"FROM" : from_fd}

        get_vals = data_dict.get("GET", {})
        set_vals = data_dict.get("SET", {})
        get_dict = {}
        for dev,param_list in get_vals.items():
            for param in param_list:
                try:
                    get_func = self.mapping[dev][param]["GET"]
                except KeyError:
                    self._logger.error(f"No mapping for GET {dev}:{param}")
                    continue
                get_dict[dev] = {param: get_func()}
        if get_dict:
            resp_dict["GET"] = get_dict
        set_dict = {}
        for dev,param_dict in set_vals.items():
            set_dict[dev] = []
            for param,val in param_dict.items():
                try:
                    set_func = self.mapping[dev][param]["SET"]
                except KeyError:
                    self._logger.error(f"No mapping for SET {dev}:{param}")
                    continue
                try:
                    set_func(val)
                except Exception as e:
                    self._logger.warning("Failed set value with")
                    self._logger.warning(str(e))
                set_dict[dev].append(param)
        if set_dict:
            resp_dict["SET"] = set_dict

        resp_json = json.dumps(resp_dict)
        self._send(resp_json.encode())

class command_command_client_t(command_client_t):
    def __init__(self, port=None, logger=None):
        super().__init__(port=port, logger=logger)
        intro_msg = {"TYPE": CLIENT_TYPE_COMMAND}
        intro_msg_json = json.dumps(intro_msg)
        intro_msg_json = intro_msg_json.encode()
        self._logger.debug("Sending introduction message.")
        self._send(intro_msg_json)

    def get_blocking(self, dev_name:str, val_names:list, timeout:float=0.1):
        msg_dict = {"GET": {dev_name: val_names}}
        msg_json = json.dumps(msg_dict)
        msg = msg_json.encode()
        self._send(msg)
        r = select.select([self], [], [], timeout)
        assert r[0], "No response before timeout."
        resp_json = self._recv()
        if not resp_json:
            return []
        resp_json = resp_json.decode()
        resp = json.loads(resp_json)
        get_items = resp.get("GET", {})
        if get_items:
            return list(get_items.values())[0]

    def set_blocking(self, dev_name:str, val_dict:dict, timeout:float=0.1):
        msg_dict = {"SET": {dev_name: val_dict}}
        msg_json = json.dumps(msg_dict)
        msg = msg_json.encode()
        self._send(msg)
        r = select.select([self], [], [], timeout)
        assert r[0], "No response before timeout."
        resp_json = self._recv()
        if not resp_json:
            return False
        resp_json = resp_json.decode()
        resp = json.loads(resp_json)
        set_items = resp.get("SET", [])
        set_params = []
        for dev_name,params in set_items.items():
            set_params += params
        diff = set(val_dict.keys()) - set(set_params)
        return not bool(diff)


class fake_param_t(object):
    def __init__(self, name:str, getf=None, setf=None):
        self.name = name
        if getf:
            self.get_function = getf
        if setf:
            self.set_function = setf

    def get_function(self):
        raise NotImplementedError

    def set_function(self)->bool:
        raise NotImplementedError

    def __str__(self):
        return f"FAKE_PARAM:{self.name}"


class fake_dev_t(object):
    def __init__(self, name:str, params:list):
        self.name   = name
        self.params = params

    def __str__(self):
        msg = f"FAKE_DEV:{self.name}\n"
        for param in self.params:
            msg += f"\t{param}"

    def format(self):
        params = {}
        for param in self.params:
            param_dict = {}
            param_dict["GET"] = param.get_function
            param_dict["SET"] = param.set_function
            params.update({param.name: param_dict})
        return {self.name: params}


def main(args):
    command_port = DEFAULT_COMMAND_PORT
    if len(args) >= 2:
        command_port = args[1]
    with command_server_t(command_port) as cmd_svr:
        cmd_svr.run_forever()
    return 0

if __name__ == "__main__":
    sys.exit(main(sys.argv))
