#!/usr/bin/env python3
"""Does uCNC still jump when two same-direction moves arrive 10-100ms apart?

The reported bug: send a short move, then a second same-direction move while
the first is still accelerating. The planner was said to act as if both lines
arrived at once, so speed/position jumped to the "no delay" profile instead of
blending smoothly (audible on the machine, missed steps).

This script measures the real firmware core in the Linux simulator:

  * two `G1` moves (or two `$J=` jogs) in the same direction, with a delay
  * status polled every ~10ms (MPos + FS feed)
  * compare the measured velocity/acceleration profile with the single move
    covering the same total distance (the "no delay" baseline)

A healthy planner accelerates continuously; the buggy profile shows a
velocity step (|dv| between adjacent samples far above accel * dt) and/or a
position step.

Usage:
  cd uCNC-my_cnc/sim-linux
  ./build/ucnc-sim -p 34060 &
  python3 sim-accel-race-test.py 34060
"""
import json
import re
import socket
import sys
import time

PORT = int(sys.argv[1]) if len(sys.argv) > 1 else 34060
POLL = 0.002  # plus the status read, ~10ms per sample
SAMPLE_WINDOW = 3.0

STATUS_RE = re.compile(r"<([^|>]+)\|(.*)>")


class Sim:
    def __init__(self, port):
        self.s = socket.create_connection(("127.0.0.1", port), timeout=5)
        self.s.settimeout(0.002)

    def send(self, data):
        self.s.sendall(data.encode())

    def line(self, data, wait=0.25):
        self.send(data + "\n")
        return self.read_for(wait)

    def rt(self, data):
        self.send(data)

    def read_for(self, seconds):
        end = time.time() + seconds
        out = b""
        while time.time() < end:
            try:
                d = self.s.recv(8192)
                if not d:
                    break
                out += d
            except (socket.timeout, BlockingIOError):
                pass
        return out.decode(errors="replace")

    def close(self):
        self.s.close()


def read_status(sim):
    """Send ? and return the newest parsed status, or None."""
    sim.rt("?")
    buf = sim.read_for(0.009)
    best = None
    for line in buf.replace("\r", "\n").split("\n"):
        m = STATUS_RE.search(line)
        if not m:
            continue
        state, rest = m.group(1), m.group(2)
        pos = feed = None
        for part in rest.split("|"):
            if part.startswith("MPos:"):
                try:
                    pos = float(part[5:].split(",")[0])
                except ValueError:
                    pass
            elif part.startswith("FS:"):
                try:
                    feed = float(part[3:].split(",")[0])
                except ValueError:
                    pass
        if pos is not None:
            best = (state, pos, feed, time.time())
    return best


def boot(sim):
    sim.line("$RST=*", 2.0)
    sim.line("$X")
    sim.line("$10=7")
    sim.line("$120=10")  # X acceleration mm/s^2 (default-ish)
    sim.line("$110=3000")  # X max rate mm/min
    sim.line("G90")
    sim.line("G92 X0")
    for _ in range(100):
        st = read_status(sim)
        if st and st[0] == "Idle":
            return
        time.sleep(0.05)
    raise RuntimeError("sim did not reach Idle")


def wait_idle(sim, timeout=20.0):
    t0 = time.time()
    while time.time() - t0 < timeout:
        st = read_status(sim)
        if st and st[0] == "Idle":
            return st
        time.sleep(0.02)
    raise RuntimeError("timeout waiting for Idle")


def sample_move(sim, lines, delays, duration):
    """Send lines with delays, sampling status every ~POLL. Returns samples."""
    samples = []
    t0 = time.time()
    idx = 0
    next_send = t0
    while time.time() - t0 < duration:
        now = time.time()
        if idx < len(lines) and now >= next_send:
            sim.line(lines[idx], 0)
            idx += 1
            if idx - 1 < len(delays):
                next_send = now + delays[idx - 1]
        st = read_status(sim)
        if st:
            state, pos, feed, ts = st
            samples.append({"t": ts - t0, "x": pos, "feed": feed, "state": state})
        time.sleep(POLL)
    return samples


def profile(samples):
    """Clean samples, derive v (from x) and reported feed; report extremes."""
    # dedupe same timestamp
    pts = []
    for s in samples:
        if pts and s["t"] - pts[-1]["t"] < 1e-4:
            continue
        pts.append(s)
    # drop the first/last idle noise
    moving = [p for p in pts if p["x"] is not None]
    if len(moving) < 3:
        return None
    v = []
    for a, b in zip(moving, moving[1:]):
        dt = b["t"] - a["t"]
        if dt <= 0:
            continue
        v.append((b["t"], (b["x"] - a["x"]) / dt))
    if not v:
        return None
    dv = []
    for a, b in zip(v, v[1:]):
        dt = b[0] - a[0]
        if dt > 0:
            dv.append((b[0], (b[1] - a[1]) / dt))
    feeds = [p["feed"] for p in moving if p["feed"] is not None]
    # reported FS feed is mm/min: derive its implied acceleration too
    fv = [(p["t"], p["feed"] / 60.0) for p in moving if p["feed"] is not None]
    fd = []
    for a, b in zip(fv, fv[1:]):
        dt = b[0] - a[0]
        if dt > 0:
            fd.append((b[0], (b[1] - a[1]) / dt))
    return {
        "x0": moving[0]["x"],
        "x1": moving[-1]["x"],
        "dist": moving[-1]["x"] - moving[0]["x"],
        "vmax": max((abs(x[1]) for x in v), default=0),
        "amax": max((abs(x[1]) for x in dv), default=0),
        "vel": v,
        "acc": dv,
        "feed_acc": fd,
        "feed_max": max(feeds) if feeds else None,
        "n": len(moving),
    }


def report(label, p, baseline=None):
    if p is None:
        print(f"{label:28s} no samples")
        return
    jumps = [a for a in p["acc"] if abs(a[1]) > 200]  # 200 mm/s^2 is way over $120=10
    fjumps = [a for a in p["feed_acc"] if abs(a[1]) > 200]
    print(
        f"{label:28s} dist={p['dist']:8.3f} vmax={p['vmax']:8.2f} "
        f"amax={p['amax']:9.1f} xjumps>200={len(jumps):2d} "
        f"fjumps>200={len(fjumps):2d} feedmax={p['feed_max']}"
    )
    if baseline:
        print(
            f"{'  baseline':28s} dist={baseline['dist']:8.3f} vmax={baseline['vmax']:8.2f} "
            f"amax={baseline['amax']:9.1f}"
        )
    for t, a in (jumps + fjumps)[:5]:
        print(f"    accel jump at t={t:.3f}s: {a:.1f} mm/s^2")


def trial_g1(sim, dist, feed, delay, accel=10, s_curve=0, feed2=None):
    sim.line(f"$120={accel}")
    sim.line(f"$14={s_curve}")
    sim.line("G90")
    sim.line("G92 X0")
    wait_idle(sim)
    # two moves of `dist` each, same direction
    f2 = feed if feed2 is None else feed2
    lines = [f"G91 G1 X{dist:.3f} F{feed}", f"G1 X{dist:.3f} F{f2}"]
    duration = (2 * dist) / (min(feed, f2) / 60.0) + 2.0 + delay
    samples = sample_move(sim, lines, [delay], duration)
    wait_idle(sim)
    return profile(samples)


def trial_baseline(sim, dist, feed, accel=10, s_curve=0):
    sim.line(f"$120={accel}")
    sim.line(f"$14={s_curve}")
    sim.line("G90")
    sim.line("G92 X0")
    wait_idle(sim)
    lines = [f"G91 G1 X{2 * dist:.3f} F{feed}"]
    duration = (2 * dist) / (feed / 60.0) + 2.0
    samples = sample_move(sim, lines, [], duration)
    wait_idle(sim)
    return profile(samples)


def trial_jog(sim, dist, feed, delay):
    """Two $J jogs in the same direction (the original UI report)."""
    sim.line("G90")
    sim.line("G92 X0")
    wait_idle(sim)
    lines = [f"$J=G91 X{dist:.3f} F{feed}", f"$J=G91 X{dist:.3f} F{feed}"]
    duration = (2 * dist) / (feed / 60.0) + 3.0 + delay
    samples = sample_move(sim, lines, [delay], duration)
    wait_idle(sim)
    return profile(samples)


def main():
    sim = Sim(PORT)
    try:
        boot(sim)
        for accel in (10, 100):
            for dist, feed in ((2.0, 600), (10.0, 1200), (2.0, 3000)):
                base = trial_baseline(sim, dist, feed, accel)
                for delay_ms in (0, 10, 20, 30, 50, 100):
                    p = trial_g1(sim, dist, feed, delay_ms / 1000.0, accel)
                    report(f"G1 {dist}mm F{feed} A{accel} +{delay_ms}ms", p, base)
                print()
        # focused: very short moves, high accel, all S-curve profiles.
        # This is closest to the original "jog button clicked twice" report:
        # a tiny move whose profile is acceleration-limited, joined by a
        # second command 10-50ms later.
        for s_curve in (0, 1, 2, 3):
            for dist, feed, accel in ((0.5, 3000, 500), (2.0, 3000, 500), (10.0, 3000, 100)):
                base = trial_baseline(sim, dist, feed, accel, s_curve)
                for delay_ms in (0, 10, 20, 30, 50):
                    p = trial_g1(sim, dist, feed, delay_ms / 1000.0, accel, s_curve)
                    report(f"G1 {dist}mm F{feed} A{accel} S{s_curve} +{delay_ms}ms", p, base)
                # also a feed change across the junction
                for delay_ms in (20, 50):
                    p = trial_g1(sim, dist, feed, delay_ms / 1000.0, accel, s_curve, feed2=feed * 4)
                    report(f"G1 {dist}mm F{feed}->{feed*4} S{s_curve} +{delay_ms}ms", p, base)
            print()

        # jog variant at one representative setting
        for dist, feed in ((2.0, 600), (10.0, 1200)):
            base = trial_baseline(sim, dist, feed)
            for delay_ms in (0, 20, 50):
                p = trial_jog(sim, dist, feed, delay_ms / 1000.0)
                report(f"JOG {dist}mm F{feed} +{delay_ms}ms", p, base)
    finally:
        sim.close()


if __name__ == "__main__":
    main()
