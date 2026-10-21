#!/usr/bin/env python3
"""Timed velocity measurement: G1 X50 F600 should take ~5s + accel ramps."""
import socket
import sys
import time
import re

port = int(sys.argv[1]) if len(sys.argv) > 1 else 34042
s = socket.create_connection(("127.0.0.1", port), timeout=5)
s.settimeout(3)


def cmd(c, wait=0.6):
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


print("G1", repr(cmd("G1 X50 F600\n")))
t0 = time.time()
last = 0.0
for i in range(16):
    time.sleep(1.0)
    st = cmd("?")
    m = re.search(r"MPos:([0-9.]+)", st)
    x = float(m.group(1)) if m else -1
    dt = time.time() - t0
    print(f"t={dt:.1f}s X={x:.3f} v≈{(x-last):.2f}mm/s {st.strip()[:50]}")
    last = x
    if "Idle" in st and i > 4:
        break
s.close()
