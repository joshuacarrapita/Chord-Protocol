#!/usr/bin/env python3

import re
import subprocess
from http.client import HTTPConnection


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
        conn = HTTPConnection('localhost', args.port, timeout=2)
        conn.connect()

        for path, content in {
            '/static/foo': b'Foo',
            '/static/bar': b'Bar',
            '/static/baz': b'Baz',
        }.items():
            conn.request('GET', path)
            reply = conn.getresponse()
            if reply.status != 200 or reply.read() != content:
                return EXIT_FAILURE

        for path in ['/static/other', '/static/anything', '/static/else']:
            conn.request('GET', path)
            if conn.getresponse().status != 404:
                return EXIT_FAILURE

        return EXIT_SUCCESS

    finally:
        server.kill()


if __name__ == '__main__':
    import sys
    sys.exit(main())
