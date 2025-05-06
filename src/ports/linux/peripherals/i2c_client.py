#!/usr/bin/env python3


import socket


class i2c_client_t(object):
    def __init__(self, port):
        self._port = port
        self._conn = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        self._conn.connect(port)

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        self.close()

    def transfer(self, addr:int, w:list, r:list):
        wn = len(w)
        rn = len(r)

        w_str = ""
        r_str = ""

        if wn:
            w_str = "[" + ",".join([f"{wp:02x}" for wp in w]) + "]"
        if rn:
            r_str = "[" + ",".join([f"{rp:02x}" for rp in r]) + "]"
        msg = f"{addr:02x}:{wn:d}{w_str}:{rn}{r_str}".encode()
        print(msg)
        self._conn.send(msg)

    def close(self):
        self._conn.close()


def main():

    osm_loc = os.getenv("OSM_LOC", "/tmp/osm/")
    if not os.path.exists(osm_loc):
        os.mkdir(osm_loc)
    sock_loc = os.path.join(osm_loc, "i2c_socket")
    client = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    client.connect(sock_loc)
    client.send(b"10:1[00]:2")
    try:
        while True:
            line = client.recv(1024)
            if not line or not len(line):
                break
            print(line)
    except KeyboardInterrupt:
        print("Caught keyboard interrupt.")
    finally:
        client.close()
    return 0

if __name__ == '__main__':
    import sys
    sys.exit(main())
