#!/usr/bin/env python3
"""Minimal interactive client for the uCNC Linux simulator (TCP Grbl console)."""
import socket
import sys
import time

port = int(sys.argv[1]) if len(sys.argv) > 1 else 34000
cmds = sys.argv[2:] or None


def main():
    s = socket.create_connection(("127.0.0.1", port), timeout=5)
    s.settimeout(3)

    def cmd(c, wait=1.0):
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

    if cmds:
        for c in cmds:
            if c.startswith("sleep:"):
                time.sleep(float(c.split(":")[1]))
            else:
                print(f">> {c!r}")
                print(f"<< {cmd(c + chr(10))!r}")
    else:
        print("type Grbl commands (blank line quits)")
        while True:
            try:
                line = input("> ")
            except EOFError:
                break
            if not line:
                break
            print(cmd(line + "\n"))
    s.close()


main()
