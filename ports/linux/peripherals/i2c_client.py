#!/usr/bin/env python3


import socket


def main():
    sock_loc = "/tmp/osm/i2c_socket"
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
