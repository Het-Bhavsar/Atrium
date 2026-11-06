"""Heart rate from the green PPG with IMU motion rejection (FEATURES.md #1). Wellness estimate only.

Atrium method "spectral track", one output a second:
 1. Green PPG and the three accelerometer axes at 25 Hz, band-pass 0.6-3.5 Hz (36-210 bpm), causal biquads.
 2. Every second, take the last 8 s (200 samples), Hann window, 256-point real FFT.
 3. Motion: sum the three accelerometer power spectra. When the band-passed accelerometer RMS is above
    `motion_mg`, scale the PPG and accelerometer spectra to 1 at their in-band maxima, widen the accelerometer
    spectrum by one bin, compress it (fourth root) and subtract it, keeping at least `mask_floor` of each PPG
    bin. Bins where the head moves lose their power.
 4. Track: score each local maximum by its weighted power times a Gaussian prior around the last estimate,
    sigma `track_sigma_bpm`, widening while the estimate is lost. With no track, take the strongest peak,
    but prefer a peak at half its frequency if that peak has at least `half_ratio` of its power. While
    moving, peaks below `moving_f_lo` are ignored. A peak far from the track that is `switch_ratio` times
    stronger for `switch_after` steps in a row takes over the track (recovers from a wrong first lock).
 5. Confidence: share of the (motion-weighted) in-band PPG power within one bin of the chosen peak and its
    second harmonic. It rates the track, not whether a pulse exists: after the motion subtraction noise can
    score high. Use wear.pulse_present on the unweighted spectrum to decide that.
 6. The output moves by at most `slew_bpm` a second and is held (flagged low confidence) when confidence
    is below `conf_min`.

`autocorr_hr` is a port of the current firmware estimate (firmware/src/pulse.c) for comparison.
Pod cost at 25 Hz: 4 x 200-sample float ring buffers (3.2 KB) + 256-point FFT scratch and two spectra
(about 3 KB), so about 6.5 KB RAM; 4 real FFTs a second, 1 when still (the accelerometer FFTs are skipped).
"""
from dataclasses import dataclass

import numpy as np

from . import dsp


@dataclass
class HrConfig:
    fs: float = 25.0
    win_s: float = 8.0
    step_s: float = 1.0
    nfft: int = 256
    f_lo: float = 0.6
    f_hi: float = 3.5
    motion_mg: float = 20.0      # [ASSUMED] band-passed accelerometer RMS that turns on the motion weighting
    mask_floor: float = 0.001
    track_sigma_bpm: float = 12.0
    half_ratio: float = 0.5
    slew_bpm: float = 6.0
    conf_min: float = 0.12       # [ASSUMED] tuned on the benchmark records, check on the pod
    lost_after: int = 10         # steps below conf_min before the track is dropped
    mask_power: float = 0.25     # accelerometer spectrum is compressed (^0.25) so its weaker harmonics mask too
    moving_f_lo: float = 0.9     # [ASSUMED] while moving, a pulse below 54 bpm is not plausible
    switch_ratio: float = 2.0    # an untracked peak this much stronger ...
    switch_after: int = 6        # ... for this many steps in a row, within switch_bpm of itself, takes over
    switch_bpm: float = 8.0


@dataclass
class HrResult:
    t: np.ndarray       # window end time, s
    bpm: np.ndarray     # output, held through low-confidence steps; nan before the first estimate
    conf: np.ndarray    # 0..1
    motion_mg: np.ndarray
    valid: np.ndarray   # conf >= conf_min
    peak_bpm: np.ndarray = None  # this window's chosen peak, before tracking limits; nan if none
    acc_peak_bpm: np.ndarray = None  # strongest accelerometer peak while moving; nan when still


def _peaks(p, lo, hi):
    i = np.arange(max(lo, 1), min(hi, len(p) - 1))
    m = (p[i] >= p[i - 1]) & (p[i] > p[i + 1])
    return i[m]


def spectral_hr(ppg, acc_g, cfg=HrConfig(), motion=True, track=True):
    """ppg: samples at cfg.fs. acc_g: (n, 3) accelerometer in g at cfg.fs, or None."""
    fs = cfg.fs
    x = dsp.bandpass(np.asarray(ppg, float), cfg.f_lo, cfg.f_hi, fs)
    a = None if acc_g is None else dsp.bandpass(np.asarray(acc_g, float) * 1000.0, cfg.f_lo, cfg.f_hi, fs)
    starts, w = dsp.windows(len(x), fs, cfg.win_s, cfg.step_s)
    hann = np.hanning(w)
    df = fs / cfg.nfft
    lo, hi = int(np.floor(cfg.f_lo / df)), int(np.ceil(cfg.f_hi / df)) + 1

    out_t, out_bpm, out_conf, out_mot, out_peak, out_acc = [], [], [], [], [], []
    lo_moving = int(np.floor(cfg.moving_f_lo / df))
    est, lost, alt, alt_n = None, 0, None, 0
    for s in starts:
        seg = x[s:s + w]
        seg = (seg - seg.mean()) * hann
        p = np.abs(np.fft.rfft(seg, cfg.nfft)) ** 2
        mot = 0.0
        pw = p.copy()
        lo_s = lo
        acc_peak = np.nan
        if a is not None:
            aseg = a[s:s + w]
            mot = float(np.sqrt(np.mean(np.sum((aseg - aseg.mean(0)) ** 2, axis=1))))
            if mot > cfg.motion_mg:
                m = sum(np.abs(np.fft.rfft((aseg[:, k] - aseg[:, k].mean()) * hann, cfg.nfft)) ** 2 for k in range(3))
                acc_peak = (lo + int(np.argmax(m[lo:hi]))) * df * 60.0
            if motion and mot > cfg.motion_mg:
                m = np.maximum(m, np.maximum(np.roll(m, 1), np.roll(m, -1)))
                mn = (m / (m[lo:hi].max() + 1e-12)) ** cfg.mask_power
                pn_ = p / (p[lo:hi].max() + 1e-12)
                pw = np.maximum(pn_ - mn, cfg.mask_floor * pn_)
                lo_s = lo_moving
        cand = _peaks(pw, lo_s, hi)
        if len(cand) == 0:
            out_t.append((s + w) / fs); out_bpm.append(np.nan if est is None else est)
            out_conf.append(0.0); out_mot.append(mot); out_peak.append(np.nan); out_acc.append(acc_peak); lost += 1
            continue
        pn = pw[cand] / pw[cand].max()
        if track and est is not None and lost < cfg.lost_after:
            f_c = (cand + np.array([dsp.parabolic(pw, i) for i in cand])) * df * 60.0
            sigma = cfg.track_sigma_bpm * (1.0 + 0.3 * lost)
            score = pn * np.exp(-0.5 * ((f_c - est) / sigma) ** 2)
            k = cand[int(np.argmax(score))]
            # re-acquire: a different peak that stays much stronger than the tracked one takes over
            top = cand[int(np.argmax(pn))]
            f_top = top * df * 60.0
            if abs(f_top - est) > 2 * cfg.switch_bpm and pw[top] >= cfg.switch_ratio * pw[k]:
                alt_n = alt_n + 1 if alt is not None and abs(f_top - alt) <= cfg.switch_bpm else 1
                alt = f_top
                if alt_n >= cfg.switch_after:
                    k, est, alt, alt_n = top, None, None, 0
            else:
                alt, alt_n = None, 0
        else:
            k = cand[int(np.argmax(pn))]
            half = cand[np.abs(cand - k / 2.0) <= 1.0]
            if len(half) and pw[half].max() >= cfg.half_ratio * pw[k] and half[np.argmax(pw[half])] * df * 60 >= 40:
                k = half[np.argmax(pw[half])]
        f = (k + dsp.parabolic(pw, k)) * df
        bpm = f * 60.0
        near = pw[max(k - 1, 0):k + 2].sum()
        k2 = int(round(2 * f / df))
        if k2 + 1 < hi:
            near += pw[k2 - 1:k2 + 2].sum()
        conf = float(min(near / (pw[lo:hi].sum() + 1e-12), 1.0))
        if conf >= cfg.conf_min:
            if est is None or lost >= cfg.lost_after:
                est = bpm
            else:
                est = est + float(np.clip(bpm - est, -cfg.slew_bpm * cfg.step_s, cfg.slew_bpm * cfg.step_s))
            lost = 0
        else:
            lost += 1
        out_t.append((s + w) / fs)
        out_bpm.append(np.nan if est is None else est)
        out_conf.append(conf)
        out_mot.append(mot)
        out_peak.append(bpm)
        out_acc.append(acc_peak)
    conf = np.array(out_conf)
    return HrResult(np.array(out_t), np.array(out_bpm), conf, np.array(out_mot), conf >= cfg.conf_min,
                    np.array(out_peak), np.array(out_acc))


def autocorr_hr(ppg, fs, win_s=8.0, step_s=1.0):
    """Port of firmware/src/pulse.c: 1 s one-pole baseline, light smoothing, normalised autocorrelation 40-200 bpm."""
    x = np.asarray(ppg, float)
    dc, lp = x[0], 0.0
    y = np.empty_like(x)
    for i, v in enumerate(x):
        dc += (v - dc) / fs
        lp += ((v - dc) - lp) * 0.5
        y[i] = lp
    starts, w = dsp.windows(len(y), fs, win_s, step_s)
    lag_min, lag_max = int(60 * fs / 200), int(60 * fs / 40)
    t, bpm, conf = [], [], []
    for s in starts:
        seg = y[s:s + w] - y[s:s + w].mean()
        r = np.zeros(lag_max + 2)
        for lag in range(lag_min - 1, lag_max + 2):
            u, v = seg[:w - lag], seg[lag:]
            d = np.sqrt((u * u).sum() * (v * v).sum())
            r[lag] = (u * v).sum() / d if d > 0 else 0.0
        best = 0
        for lag in range(lag_min, lag_max + 1):
            if r[lag] >= r[lag - 1] and r[lag] >= r[lag + 1] and (best == 0 or r[lag] > r[best]):
                best = lag
        b = np.nan
        c = 0.0
        if best and r[best] > 0:
            for lag in range(lag_min, min(best // 2 + 2, lag_max + 1)):
                if r[lag] >= r[lag - 1] and r[lag] >= r[lag + 1] and r[lag] >= 0.85 * r[best] and 2 * lag + 2 >= best:
                    best = lag
                    break
            b = 60.0 * fs / (best + dsp.parabolic(r, best))
            c = float(min(r[best], 1.0))
        t.append((s + w) / fs); bpm.append(b); conf.append(c)
    conf = np.array(conf)
    return HrResult(np.array(t), np.array(bpm), conf, np.zeros(len(t)), conf >= 0.5)


def reference_hr(r_peaks_s, t_end, win_s=8.0):
    """ECG reference: 60 / mean RR of the R peaks inside each window [t - win_s, t]. nan if fewer than 3 peaks."""
    r = np.asarray(r_peaks_s, float)
    out = np.full(len(t_end), np.nan)
    for i, t in enumerate(t_end):
        b = r[(r >= t - win_s) & (r <= t)]
        if len(b) >= 3:
            out[i] = 60.0 / np.mean(np.diff(b))
    return out
