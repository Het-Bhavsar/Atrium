"""Beat-to-beat intervals and RMSSD from PPG (FEATURES.md #2). Wellness estimate; still windows only.

Atrium method "max-slope PPI":
 1. Band-pass 0.5-5 Hz, causal biquads (keeps the systolic upstroke at 25 Hz and up).
 2. Fiducial point: the maximum of the first difference on each upstroke. It is sharper than the
    systolic peak, so it times better at low sample rates.
 3. A beat is a local maximum of the slope that is at least `thr_frac` of the median of the last 8 beat
    slopes, and at least `refractory_s` after the previous beat.
 4. Sub-sample timing: fit a parabola to the slope at the maximum and its two neighbours ("parabolic"),
    or evaluate a local cubic through 4 samples on a 1 ms grid ("cubic").
 5. Clean intervals: 300-2000 ms, and within `max_dev` (15 %) of the median of the last 5 in-range intervals.
    A successive difference counts only when both intervals are clean and adjacent.
 6. RMSSD = sqrt(mean(dPPI^2)) over the window; None below `min_diffs` differences.

Pod cost: 2 biquads + a 5-sample history + 13 floats of beat history; no buffers.
"""
from dataclasses import dataclass

import numpy as np

from . import dsp


@dataclass
class HrvConfig:
    f_lo: float = 0.5
    f_hi: float = 5.0
    refractory_s: float = 0.33
    thr_frac: float = 0.4
    interp: str = "parabolic"  # "parabolic", "cubic" or "none"; cubic was no better on the benchmark
    rr_min_ms: float = 300.0
    rr_max_ms: float = 2000.0
    max_dev: float = 0.15      # Atrium artifact rule, against the local median; 0.15 had the lowest error
    min_diffs: int = 20


def _cubic_max(d, i, fs):
    """Peak time (s) of a Catmull-Rom cubic through d[i-1..i+1] neighbourhood, on a 1 ms grid."""
    if i < 2 or i > len(d) - 3:
        return i / fs
    best_t, best_v = float(i), d[i]
    for seg in (i - 1, i):
        p0, p1, p2, p3 = d[seg - 1], d[seg], d[seg + 1], d[seg + 2]
        for u in np.linspace(0, 1, max(int(1000 / fs), 2)):
            v = 0.5 * ((2 * p1) + (-p0 + p2) * u + (2 * p0 - 5 * p1 + 4 * p2 - p3) * u ** 2
                       + (-p0 + 3 * p1 - 3 * p2 + p3) * u ** 3)
            if v > best_v:
                best_v, best_t = v, seg + u
    return best_t / fs


def detect_beats(ppg, fs, cfg=HrvConfig()):
    """Beat times in s (fiducial = maximum upstroke slope)."""
    x = dsp.bandpass(np.asarray(ppg, float), cfg.f_lo, min(cfg.f_hi, 0.45 * fs), fs)
    d = np.diff(x, prepend=x[0])
    settle = int(2 * fs)
    beats, slopes = [], []
    ref = int(round(cfg.refractory_s * fs))
    for i in range(max(settle, 1), len(d) - 1):
        if not (d[i] > d[i - 1] and d[i] >= d[i + 1] and d[i] > 0):
            continue
        if slopes and d[i] < cfg.thr_frac * np.median(slopes[-8:]):
            continue
        if beats and i - beats[-1][0] < ref:
            if d[i] > beats[-1][1]:       # a steeper slope inside the refractory window replaces the last one
                beats[-1] = (i, d[i]); slopes[-1] = d[i]
            continue
        beats.append((i, d[i])); slopes.append(d[i])
    t = []
    for i, _ in beats:
        if cfg.interp == "parabolic":
            t.append((i + dsp.parabolic(d, i)) / fs)
        elif cfg.interp == "cubic":
            t.append(_cubic_max(d, i, fs))
        else:
            t.append(i / fs)
    return np.array(t)


def clean_intervals(beat_s, cfg=HrvConfig()):
    """(interval end time s, interval ms, clean flag) for each pair of consecutive beats."""
    b = np.asarray(beat_s, float)
    rr = np.diff(b) * 1000.0
    ok = np.zeros(len(rr), bool)
    hist = []
    for k, v in enumerate(rr):
        in_range = cfg.rr_min_ms <= v <= cfg.rr_max_ms
        good = in_range
        if good and len(hist) >= 3:
            med = np.median(hist[-5:])
            good = abs(v - med) <= cfg.max_dev * med
        if in_range:
            hist.append(v)  # the median of 5 tolerates 2 bad intervals, and recovers after a rate step
        ok[k] = good
    return b[1:], rr, ok


def rmssd(rr_ms, ok, min_diffs):
    """RMSSD over adjacent clean pairs. Returns (rmssd_ms or None, n_diffs)."""
    rr = np.asarray(rr_ms, float)
    ok = np.asarray(ok, bool)
    pair = ok[1:] & ok[:-1]
    d = np.diff(rr)[pair]
    if len(d) < min_diffs:
        return None, len(d)
    return float(np.sqrt(np.mean(d ** 2))), len(d)


def windowed_rmssd(beat_s, t0, t1, win_s, cfg=HrvConfig()):
    """RMSSD per non-overlapping window: list of (window start, rmssd or None, n_diffs)."""
    te, rr, ok = clean_intervals(beat_s, cfg)
    out = []
    t = t0
    while t + win_s <= t1:
        m = (te > t) & (te <= t + win_s)
        r, n = rmssd(rr[m], ok[m], cfg.min_diffs)
        out.append((t, r, n))
        t += win_s
    return out


def pooled(rmssd_list):
    """Combine windows the way docs/ble_gatt.md section 7 does: sqrt(sum(r^2 n) / sum(n))."""
    num = sum(r * r * n for r, n in rmssd_list if r is not None)
    den = sum(n for r, n in rmssd_list if r is not None)
    return float(np.sqrt(num / den)) if den else None
