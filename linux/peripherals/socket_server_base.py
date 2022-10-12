import os
import sys
import socket
import datetime
import selectors

COLOUR_WHITE  = "\033[29m"
COLOUR_BLACK  = "\033[30m"
COLOUR_RED    = "\033[31m"
COLOUR_GREEN  = "\033[32m"
COLOUR_YELLOW = "\033[33m"
COLOUR_BLUE   = "\033[34m"
COLOUR_CYAN   = "\033[36m"
COLOUR_GREY   = "\033[37m"

COLOUR_RESET  = COLOUR_WHITE


def log(file_, msg, colour=None):
    if file_ is not sys.stdout and file_.closed:
        file_.open()
    payload = f"[{datetime.datetime.isoformat(datetime.datetime.utcnow())}] {msg}\n"
    if file_ is sys.stdout and colour:
        payload = colour + payload + COLOUR_RESET
    file_.write(payload)
    file_.flush()
    return len(payload)


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

        if logger is None:
            log_file_obj = sys.stdout if log_file is None else open(log_file, "a")
            self._log = lambda m, c=None: log(log_file_obj, m, c)
            self.info    = lambda m : self._log(f"{{INFO}}: {m}", COLOUR_GREY)
            self.error   = lambda m : self._log(f"{{ERROR}}: {m}", COLOUR_RED)
            self.warning = lambda m : self._log(f"{{WARNING}}: {m}", COLOUR_YELLOW)
            if os.environ.get("DEBUG", None):
                self.debug   = lambda m : self._log(f"{{DEBUG}}: {m}", COLOUR_CYAN)
            else:
                self.debug   = lambda *x : x
        else:
            self.info    = logger.info
            self.error   = logger.error
            self.warning = logger.warning
            self.debug   = logger.debug if os.environ.get("DEBUG", None) else lambda *x : x

        self._recv_size = 1024


    def _new_client_callback(self, sock, __mask):
        conn, addr = sock.accept()
        self.debug("New client connection FD:%u" % conn.fileno())

        self._clients[conn.fileno()] = conn

        self._selector.register(conn,
                                selectors.EVENT_READ,
                                self._client_message)

    def _client_message(self, sock, __mask):
        fd = sock.fileno()
        client = self._clients.get(fd, None)
        if client is None:
            self.error(f"Message from client [fd:{fd}] not in connected clients list.")
            return
        while True:
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
        self._selector.unregister(client)
        client.close()

    def _send_to_client(self, client, msg):
        self.debug("Message to client [fd:%u] >> '%s'" % (client.fileno(), msg))
        try:
            resp = client.send(msg.encode())
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
            print("Caught keyboard interrupt, exiting")
        finally:
            self._selector.close()
