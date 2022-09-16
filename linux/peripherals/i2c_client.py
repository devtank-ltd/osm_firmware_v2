#!/usr/bin/env python


import socket


def main():
    HOST = "127.0.0.1"
    port = 65432
    client = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    client.connect((HOST, port))
    client.send(b"10:1:00:[--]")
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
