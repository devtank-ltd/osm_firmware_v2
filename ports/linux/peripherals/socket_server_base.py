import os
import sys
import socket
import datetime
import selectors

import basetypes


class socket_server_t(object):
    def __init__(self, socket_loc, log_file=None, logger=None):
        if os.path.exists(socket_loc):
            os.unlink(socket_loc)
        self._server = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        self._server.bind(socket_loc)
        self._server.setblocking(False)
        self._server.listen(5)

        self._selector = selectors.PollSelector()
        self._selector.register(self._server, selectors.EVENT_READ, self._new_client_callback)

        self._clients = {}

        self.fileno = self._server.fileno

        self._done = False

        if not logger:
            logger = basetypes.get_logger(log_file)

        self._logger = logger

        self.info    = logger.info
        self.error   = logger.error
        self.warning = logger.warning
        self.debug   = logger.debug

        self._recv_size = 1024


    def _new_client_callback(self, sock, __mask):
        conn, addr = sock.accept()
        fd = conn.fileno()
        self.debug("New client connection FD:%u" % fd)

        self._clients[fd] = conn

        self._selector.register(conn,
                                selectors.EVENT_READ,
                                self._client_message)

    def _client_message(self, sock, __mask):
        fd = sock.fileno()
        client = self._clients.get(fd, None)
        if client is None:
            self.error(f"Message from client [fd:{fd}] not in connected clients list.")
            return
        try:
            data = client.recv(self._recv_size)
        except OSError:
            self._close_client(client)
            return
        if data is None:
            self._close_client(client)
            return
        if data is False:
            self.warning(f"Tried to read from client [fd:{fd}] but failed.")
            return
        if not len(data):
            return
        self._process(client, data)

    def _process(self, client, data):
        raise NotImplemented

    def _close_client(self, client):
        fd = client.fileno()
        try:
            self._clients.pop(fd)
        except KeyError:
            return
        self.info(f"Disconnecting client [fd:{fd}]")
        try:
            self._selector.unregister(client)
        except KeyError:
            self.info(f"Could not properly disconnect client [fd:{fd}].")
        client.close()

    def _send_to_client(self, client, msg):
        self.debug("Message to client [fd:%u] >> '%s'" % (client.fileno(), msg))
        if not isinstance(msg, bytes):
            msg = msg.encode()
        try:
            resp = client.send(msg)
        except BrokenPipeError:
            self._close_client(client)
            return
        if not resp:
            self._close_client(client)

    def _send_to_all_clients(self, msg):
        for client in self._clients.values():
            self._send_to_client(client, msg)

    def run_forever(self):
        try:
            while not self._done:
                events = self._selector.select(timeout=0.5)
                for key, mask in events:
                    callback = key.data
                    callback(key.fileobj, mask)
        except KeyboardInterrupt:
            print(self.__class__.__name__ + " : Caught keyboard interrupt, exiting")
        finally:
            self._selector.close()
