#!/usr/bin/env python3
"""Line-number tracking test: does the interpolator pick up N-numbered blocks?"""
import socket
import sys
import time

port = int(sys.argv[1]) if len(sys.argv) > 1 else 34040
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
print("N1", repr(cmd("N1 G1 X10 F100\n")))
for i in range(4):
    print(i, repr(cmd("?")[:100]))
s.close()
