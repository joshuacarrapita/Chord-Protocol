#!/usr/bin/env python3

import re
import subprocess
import time
import socket


EXIT_SUCCESS = 0
EXIT_FAILURE = 1
buffer_size = 1024


def main():
    import argparse

    parser = argparse.ArgumentParser()
    parser.add_argument('executable')
    parser.add_argument('--port', default=4711, type=int)
    args = parser.parse_args()

    try:
        server = subprocess.Popen([args.executable, str(args.port)])
        conn = socket.create_connection(('localhost', args.port), timeout=2)
        conn.settimeout(.5)
        conn.send('Request\r\n\r\n'.encode())
        time.sleep(.5)  # Attempt to gracefuly handle all kinds of multi-packet replies...
        reply = conn.recv(1024)
        return EXIT_SUCCESS if re.match(br'HTTP/1.[01] 400.*\r\n\r\n', reply) else EXIT_FAILURE
    finally:
        server.kill()


if __name__ == '__main__':
    import sys
    sys.exit(main())
