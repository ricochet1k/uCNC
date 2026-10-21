#!/usr/bin/env python3
"""Persistent-connection test: RST, unlock, move, poll status on one socket."""
import socket
import sys
import time

port = int(sys.argv[1]) if len(sys.argv) > 1 else 34034
s = socket.create_connection(("127.0.0.1", port), timeout=5)
s.settimeout(3)


def cmd(c, wait=0.8):
    s.sendall(c.encode())
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
    return out.decode(errors="replace")


print("RST", repr(cmd("$RST=*\n", 3.0))[-8:])
print("X", repr(cmd("$X\n"))[-8:])
print("G1", repr(cmd("G1 X10 F600\n")))
for i in range(5):
    st = cmd("?")
    print(i, repr(st[:75]))
s.close()
