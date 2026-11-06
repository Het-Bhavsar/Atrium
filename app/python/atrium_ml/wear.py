"""Wear detection (FEATURES.md #18): decide once a second whether the pod is on skin.

Atrium method, three cues per second:
 - proximity: green DC per mA of LED current, as a fraction of the user's own on-skin DC (`dc_ref`,
   learned on the first confident minute of each wear). Open air reflects almost nothing.
 - pulse: `pulse_present`: the unweighted HR spectrum (no motion subtraction, which inflates confidence on
   noise) has a peak share of at least `pulse_conf` with the peak within `pulse_tol_bpm` from one second to
   the next, for `pulse_run_s` seconds in a row, and not within `pulse_tol_bpm` of the strongest accelerometer
   peak, half or twice it (a pod swinging in a pocket reflects at the step rate). Sensor noise peaks wander, so they rarely pass.
 - warmth: the skin-side reference NTC (TH3, away from the heater). Off the head the pod cools towards room
   temperature.

State machine (worn -> off-body):
 - dark: DC below `dc_off` for `dark_s` seconds (lifted into open air, or face up on a table);
 - no pulse, still and cool: no confident pulse for `no_pulse_s`, accelerometer still, skin temperature below
   `cool_c` or falling faster than `cool_rate` (on a table under a lamp, in a drawer);
 - no pulse for `no_pulse_long_s` while not warm (in a pocket or bag, where cloth reflects and it moves).
off-body -> worn: DC above `dc_on` for `on_s` seconds, and a confident pulse within `confirm_s` seconds.
During exercise motion masks the pulse but the pod stays warm and lit, so it stays worn.

Pod cost: a few counters and a 60 s temperature slope; the HR confidence is already computed.
Thresholds are [ASSUMED] until the bench sets them (see the validation protocol).
"""
from dataclasses import dataclass

import numpy as np


@dataclass
class WearConfig:
    dc_off: float = 0.15
    dc_on: float = 0.40
    dark_s: int = 4
    on_s: int = 2
    confirm_s: int = 15
    pulse_conf: float = 0.5
    pulse_run_s: int = 5
    pulse_tol_bpm: float = 4.0
    still_mg: float = 15.0
    no_pulse_s: int = 60
    no_pulse_long_s: int = 300
    cool_c: float = 30.0
    cool_rate: float = -0.10      # degC per minute
    warm_c: float = 32.0


@dataclass
class WearState:
    worn: bool = True
    dark: int = 0
    lit: int = 0
    since_pulse: int = 0
    pending: int = -1             # seconds since lit while waiting for a pulse, -1 when not waiting


def pulse_present(conf, peak_bpm, acc_peak_bpm=None, cfg=WearConfig()):
    """Per-second pulse flags from hr.spectral_hr(..., motion=False): conf, peak_bpm, acc_peak_bpm."""
    out = np.zeros(len(conf), bool)
    acc = np.full(len(conf), np.nan) if acc_peak_bpm is None else np.asarray(acc_peak_bpm, float)
    k = 0
    for i in range(len(conf)):
        steady = i > 0 and np.isfinite(peak_bpm[i]) and np.isfinite(peak_bpm[i - 1]) \
            and abs(peak_bpm[i] - peak_bpm[i - 1]) <= cfg.pulse_tol_bpm
        motion = np.isfinite(acc[i]) and any(abs(peak_bpm[i] - h * acc[i]) <= cfg.pulse_tol_bpm for h in (0.5, 1, 2))
        k = k + 1 if conf[i] >= cfg.pulse_conf and steady and not motion else 0
        out[i] = k >= cfg.pulse_run_s
    return out


def update(st, cfg, dc_rel, pulse, motion_mg, temp_c, temp_rate):
    """One second. dc_rel: DC / on-skin reference. pulse: pulse_present flag. temp_rate: degC per minute."""
    st.since_pulse = 0 if pulse else st.since_pulse + 1
    if st.worn:
        st.dark = st.dark + 1 if dc_rel < cfg.dc_off else 0
        still = motion_mg < cfg.still_mg
        cool = temp_c < cfg.cool_c or temp_rate < cfg.cool_rate
        if (st.dark >= cfg.dark_s
                or (st.since_pulse >= cfg.no_pulse_s and still and cool)
                or (st.since_pulse >= cfg.no_pulse_long_s and temp_c < cfg.warm_c)):
            st.worn, st.lit, st.pending = False, 0, -1
    else:
        st.lit = st.lit + 1 if dc_rel > cfg.dc_on else 0
        if st.pending < 0 and st.lit >= cfg.on_s:
            st.pending = 0
        if st.pending >= 0:
            if pulse:
                st.worn, st.dark, st.since_pulse, st.pending = True, 0, 0, -1
            elif dc_rel < cfg.dc_on or st.pending >= cfg.confirm_s:
                st.pending = -1
            else:
                st.pending += 1
    return st.worn


def run(dc_rel, pulse, motion_mg, temp_c, cfg=WearConfig(), worn=True):
    """Per-second arrays in (pulse: pulse_present flags), worn flags out."""
    st = WearState(worn=worn)
    temp_c = np.asarray(temp_c, float)
    out = np.zeros(len(dc_rel), bool)
    for i in range(len(dc_rel)):
        j = max(0, i - 60)
        rate = (temp_c[i] - temp_c[j]) * 60.0 / max(i - j, 1)
        out[i] = update(st, cfg, dc_rel[i], bool(pulse[i]), motion_mg[i], temp_c[i], rate)
    return out


def dc_per_second(ppg, fs, dc_ref=None):
    """Mean of each second, relative to dc_ref (default: median of the record = the on-skin level)."""
    n = int(len(ppg) // fs)
    dc = np.asarray(ppg[:int(n * fs)], float).reshape(n, int(fs)).mean(1)
    ref = np.median(dc) if dc_ref is None else dc_ref
    return dc / ref
