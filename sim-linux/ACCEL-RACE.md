# Junction overspeed when a second command arrives during acceleration

Status: reproduced in the Linux simulator on the `my_cnc-linux-sim` worktree
(2026-10-21), uCNC v1.11.0 core. Root cause found and a one-line fix verified in a
clone; still unfixed upstream and not yet confirmed on hardware.

## Symptom

Send a short move, then another move in the **same direction** while the first
is still accelerating (the classic "click the jog button twice" case, second
click ~10–100 ms later). The machine briefly runs **faster than the programmed
feed rate** and faster than the acceleration limit allows for the remaining
distance, then drops back. On hardware this is the audible speed jump that was
seen before; it can miss steps.

## Reproduction

```sh
cd uCNC-my_cnc/sim-linux
make
./build/ucnc-sim -p 34060 &
python3 sim-accel-race-test.py 34060
```

The script boots the sim, sets `$120` (X accel) and `$14` (S-curve profile),
sends two same-direction moves with a chosen delay, samples the status every
~10 ms (MPos and the firmware's own `FS` feed), and compares against a single
move covering the same distance.

Minimal case (`$120=500`, `$14=0`, F3000):

```gcode
G91 G1 X2 F3000
G1 X2 F3000          ; second line 20-50ms later
```

Feed sequence seen at ~10 ms resolution (gap 30 ms):

```
t=0.079  feed=1739 mm/min
t=0.090  feed=3042
t=0.102  feed=4384   <-- 1.46x the F3000 limit
t=0.113  feed=2516
...
```

A single 4 mm move peaks at ~2516 mm/min. A single 2 mm move peaks at ~1739.
Two 2 mm moves should peak at ~2683 (accelerate over the first block, decelerate
over the second); observed 3042–4384 depending on phase, i.e. 13–63% above the
feed limit and 14–63% above the physically possible peak.

A second, larger case (`$120=200`, F1200, two 10 mm moves, second line 30 ms
later) jumps `780 → 1860 → 1200 mm/min`: 55% over the commanded feed for one
sample, then a sharp drop back to 1200.

Delay matters exactly as remembered: with the second command arriving after the
first block has finished (about 130 ms for 2 mm at F3000/$120=500, about 200 ms
for 2 mm at lower accel), the peak is the normal single-block peak. With 0–50 ms
it overshoots. `$J=` jogs behave identically (same planner/interpolator path).

## What does *not* matter

- Both G1 and `$J=` are affected.
- S-curve on/off changes the shape and magnitude but not the existence (S0 is
  affected too; `$14=0` is the machine default).
- Both blocks need not be equal; any junction where the executing block's profile
  is recomputed mid-flight can jump.
- The first command's feed, only the ratio of block length to accel/time matters.

## Root cause (confirmed)

Not the planner entry-speed hypothesis originally guessed here. The bug is in
the **S-curve acceleration segment builder** in
`uCNC/src/core/interpolator.c`:

```c
float new_speed = acc_scale * s_curve_function(acum) + acc_init_speed;
new_speed = (t_acc_integrator >= 0) ? (new_speed + acc_init_speed) : (acc_init_speed - new_speed);
speed_change = new_speed - current_speed;
```

`new_speed` already contains `acc_init_speed`; the positive branch adds it a
**second time**. On a mid-motion replan `acc_init_speed` is the live speed (not
0), so the first post-replan segments are `2*init + delta` instead of
`init + delta`, producing a step up; the next segments correct it and the speed
drops. At a normal block start `acc_init_speed` is the block entry, so the bug
is dormant only when entry is 0 or the block stays at its top speed. It is also
reachable through feed override (`M220`; `planner_feed_ovr` → `itp_update` →
same replan).

The deceleration branch (`new_speed = junction_speed - deac_scale * s(acum)`)
uses the correct single-base form. `$14=0` still reproduces because the level
-1 build uses this branch for all profiles (the default profile is linear);
before the sim compiled `S_CURVE_ACCELERATION_LEVEL -1` it used a different
(non-S-curve) code path, which is why the first sim experiments showed nothing.

Instrumented trace for the 2x2mm F3000 / `$120=500` case: entry/live speed
1897 mm/min; the first post-replan segment predicts 2290 + 1897 = 4187, ΔV 2290,
midpoint speed 3042, next 4384 — matching the measured 3042 → 4384. Top speed
was 2683 and F was 3000.

### Fix

Restore the single base:

```c
float new_speed = acc_scale * s_curve_function(acum);
new_speed = (t_acc_integrator >= 0) ? (acc_init_speed + new_speed) : (acc_init_speed - new_speed);
speed_change = new_speed - current_speed;
```

(or delete the first `+ acc_init_speed` and drop the ternary, which is equivalent
if the negative-integrator path is unreachable). The sign-preserving form is
preferred. A patch file is in `sim-linux/patches/0001-itp-accel-double-add.patch`.

Verified in a /tmp clone + sim rebuild: the same case now profiles
1739 → 2094 → 2488 → 2516 (monotone, ≤ top 2683, ≤ F 3000); S1/S2/S3 and
A100/A10 cases peak at 93–99% of the expected physical maximum. Steady-state
profiles are unchanged (`acc_init_speed` is 0 or the profile is at top speed
there), deceleration is untouched, and the S-curve shape is preserved.

### Upstream status: NOT fixed

Checked `Paciente8159/uCNC` master at `8c96148f` (2026-10-14, ~698 commits after
the local v1.11.0 base): the same three lines exist in
`uCNC/src/core/interpolator.c` (upstream around lines 634-635) and
`planner_recalculate` is logic-identical. No open issue/PR found for this
mechanism. The stale unmerged `alternative-interpolator` branch (2023) uses the
correct single-base form, which corroborates the diagnosis. Nearest unrelated
PRs: #960 (interpolator recalc with active HOLD), #936 (feed override realtime
speed), #952 (junction deviation).

There is a secondary latent issue: `entry_feed_sqr` doubles as both the plan
field and the live-speed cursor (`itp_run` does
`current_speed = sqrt(entry_feed_sqr)`), and the planner's backward pass can
overwrite it for an executing block. The double-add is the one proven to cause
this report; the cursor aliasing should be reviewed separately.

### Hardware relevance

High. The real `my_cnc` build sets `S_CURVE_ACCELERATION_LEVEL -1`
(`uCNC/cnc_hal_overrides.h:9`), so it compiles exactly this branch. A 20–50%
step-rate spike in ~20 ms is a plausible cause of the audible jump and missed
steps. Confirm on hardware with the same two-command probe; the fix is one line.

## Relation to the SimpleCNC server

The server does not cause this: it sends whole lines, and the firmware decides
when to start executing. Any client that streams short moves (including jog
buttons, `$J`, or a file of small segments) can hit it. Mitigations on the
server side are not obvious and would hurt smoothness (e.g. delaying new lines
until the planner is empty). Fix belongs in uCNC.

## Caveats

- Simulator timing is not identical to the MCU; the *ratios* and the presence of
  a discontinuity are firmware math, but the exact overshoot may differ on
  hardware.
- The analysis uses the firmware's reported `FS` as the velocity (mm/min) and
  confirms it against position deltas; both agree.
- Status sampling is ~10–11 ms and the host timestamp is used, so derivatives
  have some noise. The jump cases are far outside the noise floor.
