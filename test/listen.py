#!/usr/bin/env python3

import subprocess
import socket


EXIT_SUCCESS = 0
EXIT_FAILURE = 1


def main():
    import argparse

    parser = argparse.ArgumentParser()
    parser.add_argument('executable')
    parser.add_argument('--port', default=4711, type=int)
    args = parser.parse_args()

    try:
        server = subprocess.Popen([args.executable, str(args.port)])
        socket.create_connection(('localhost', args.port), timeout=2)
        return EXIT_SUCCESS
    finally:
        server.kill()


if __name__ == '__main__':
    import sys
    code = main()
    if code == 0:
        print("SUCCEED")
    else:
        print("FAILED")
    sys.exit(code)
