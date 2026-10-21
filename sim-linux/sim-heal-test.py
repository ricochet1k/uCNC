# Heal-sequence test: fresh EEPROM -> RST -> X -> move (crawl expected
# from NaN poisoning: 0x18 soft-reset, X, move at full speed.
import socket
import sys
import time
import re

port = int(sys.argv[1]) if len(sys.argv) > 1 else 34065
s = socket.create_connection(('127.0.0.1', port), timeout=5)
s.settimeout(2)


def cmd(c, wait=0.5):
    s.sendall(c.encode())
    time.sleep(wait)
    out = b''
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
    return out.decode(errors='replace')


def vel(label, move):
    cmd(move)
    t0 = time.time()
    xs = []
    for _ in range(6):
        time.sleep(1.0)
        st = cmd('?')
        m = re.search(r'MPos:([0-9.\-]+)', st)
        xs.append(float(m.group(1)) if m else -999)
    dt = time.time() - t0
    v = (xs[-1] - xs[0]) / dt if xs[0] != -999 else -999
    print(f'{label}: X {xs[0]:.2f} -> {xs[-1]:.2f} in {dt:.1f}s = {v:.2f} mm/s')
    return v


print(cmd('$RST=*\n', wait=2.0)[-8:])
print(cmd('$X\n')[-8:])
v1 = vel('before-reset', 'G1 X10 F600\n')
print(cmd('\x18', wait=1.0)[-40:])
print(cmd('$X\n')[-8:])
v2 = vel('after-reset ', 'G1 X20 F600\n')
print('HEALED' if v2 > 5 * max(v1, 0.01) else 'NOT-HEALED')
s.close()
