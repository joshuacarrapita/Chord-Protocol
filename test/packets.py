#!/usr/bin/env python3

import socket
import subprocess
import time


EXIT_SUCCESS = 0
EXIT_FAILURE = 1
buffer_size = 1024


def main():
    import argparse

    parser = argparse.ArgumentParser()
    parser.add_argument('executable')
    parser.add_argument('--port', default=4711, type=int)
    args = parser.parse_args()

    server = subprocess.Popen([args.executable, str(args.port)])
    conn = socket.create_connection(('localhost', args.port), timeout=2)
    conn.settimeout(.5)
    conn.send('Request1\r\n\r\n'.encode())
    conn.send('Request\r\n2\r\n'.encode())
    conn.send('\r\n'.encode())
    time.sleep(.5)  # Attempt to gracefuly handle all kinds of multi-packet replies...
    replies = conn.recv(1024).split(b'\r\n\r\n')
    server.kill()
    return EXIT_SUCCESS if (replies[0] and replies[1] and not replies[2]) else EXIT_FAILURE


if __name__ == '__main__':
    import sys
    sys.exit(main())
