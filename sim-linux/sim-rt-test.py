#!/usr/bin/env python3
"""After starting motion, try realtime reset (0x18) and status (?)."""
import socket
import sys
import time

port = int(sys.argv[1]) if len(sys.argv) > 1 else 34034
s = socket.create_connection(("127.0.0.1", port), timeout=5)
s.settimeout(3)


def raw(data, wait=1.0):
    s.sendall(data)
    time.sleep(wait)
    out = b""
    try:
        while True:
            d = s.recv(8192)
            if not d:
                break
            out += d
            if len(d) < 8192:
                break
    except socket.timeout:
        pass
    return out


print("G1", repr(raw(b"G1 X10 F600\n")))
time.sleep(1)
print("RT-RESET", repr(raw(b"\x18")))
time.sleep(1)
print("Q", repr(raw(b"?")))
s.close()
