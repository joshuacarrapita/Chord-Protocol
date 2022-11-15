#!/usr/bin/env python3

import re
import subprocess
from http.client import HTTPConnection


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
        conn = HTTPConnection('localhost', args.port, timeout=2)
        conn.connect()

        conn.request('HEAD', '/index.html')
        reply = conn.getresponse()
        reply.read()
        if reply.status != 501:
            return EXIT_FAILURE

        conn.request('GET', '/index.html')
        reply = conn.getresponse()
        reply.read()
        if reply.status != 404:
            return EXIT_FAILURE

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
