"""Overnight blood oxygen from the red and IR LEDs (FEATURES.md #8, Later). Wellness estimate only.

The pod reports the ratio of ratios R per burst with a confidence. The app turns the night's bursts into one
nightly blood-oxygen percentage, but only through a `Calibration` fitted on Atrium pods against a reference
oximeter (VALIDATION.md V5) and marked validated. Until then `nightly_percent` returns None and the app shows only
the relative trend. A higher R means relatively less oxygenated blood.
Rules for the number: one nightly average, whole percent, shown only inside the calibrated range (capped at 100);
no per-minute values, no alerts, no wording that implies a condition. Below the calibrated range the app shows
"lower than your usual" instead of a number.

Pod, per burst (red and IR at 25 Hz, the green HR slot keeps running):
 1. Raw intensity counts. Drop the first `settle_s` seconds (AGC and filter settling).
 2. Per window of `win_s` seconds, stepped by `step_s`: DC = mean of the raw counts; AC = RMS of the 0.5-4 Hz
    band-passed signal (causal biquads, primed at the burst start). R = (AC_red / DC_red) / (AC_ir / DC_ir).
 3. A window is good when: red and IR pulses agree (band-passed correlation >= `min_corr`), the IR perfusion index
    AC/DC >= `min_pi`, the head is still (accelerometer motion <= `max_motion_mg`), the green pulse is present,
    and neither channel is near the ADC limits.
 4. Burst R = median of the good windows. Confidence = (good share) x (1 - spread / `max_spread`), where spread is
    the interquartile range of good R over their median; 0 when fewer than `min_good` windows pass.
App: nightly R = confidence-weighted median of the bursts in the sleep window. With a validated calibration that
gives the nightly percentage (`nightly_percent`); without one, only the trend: tonight against the median of the
last 14 nights, as a relative change in R.

Pod cost: 2 biquads per channel and 6 running sums per window (no sample buffers), 16 window results per burst:
under 0.3 KB RAM.
"""
from dataclasses import dataclass

import numpy as np

from . import dsp


@dataclass
class Spo2Config:
    fs: float = 25.0
    settle_s: float = 4.0
    win_s: float = 4.0
    step_s: float = 2.0
    min_corr: float = 0.8          # [ASSUMED]
    min_pi: float = 2e-4           # 0.02 % IR perfusion [ASSUMED]; the finger records sit at 0.1-0.7 %
    max_motion_mg: float = 15.0    # [ASSUMED], the activity "still" level
    adc_full: float = 524287.0     # 19-bit
    adc_margin: float = 0.05
    min_good: int = 4
    max_spread: float = 0.2


@dataclass
class Window:
    t_s: float
    r: float
    corr: float
    pi_ir: float
    good: bool


def windows(red, ir, motion_mg=None, pulse_ok=None, cfg=Spo2Config()):
    """red, ir: raw intensity counts at cfg.fs for one burst. motion_mg, pulse_ok: per second (optional)."""
    red = np.asarray(red, float)
    ir = np.asarray(ir, float)
    br = dsp.bandpass(red, 0.5, 4.0, cfg.fs)
    bi = dsp.bandpass(ir, 0.5, 4.0, cfg.fs)
    w, st = int(cfg.win_s * cfg.fs), int(cfg.step_s * cfg.fs)
    out = []
    for s in range(int(cfg.settle_s * cfg.fs), len(red) - w + 1, st):
        r_ac, i_ac = br[s:s + w], bi[s:s + w]
        r_dc, i_dc = red[s:s + w].mean(), ir[s:s + w].mean()
        ar, ai = np.sqrt(np.mean(r_ac ** 2)), np.sqrt(np.mean(i_ac ** 2))
        corr = float(np.corrcoef(r_ac, i_ac)[0, 1]) if ar > 0 and ai > 0 else 0.0
        pi_ir = ai / i_dc if i_dc > 0 else 0.0
        r = (ar / r_dc) / pi_ir if r_dc > 0 and pi_ir > 0 else np.nan
        sec = slice(int(s / cfg.fs), int(np.ceil((s + w) / cfg.fs)))
        still = motion_mg is None or np.max(np.asarray(motion_mg)[sec], initial=0) <= cfg.max_motion_mg
        pulse = pulse_ok is None or bool(np.all(np.asarray(pulse_ok)[sec]))
        lo, hi = cfg.adc_margin * cfg.adc_full, (1 - cfg.adc_margin) * cfg.adc_full
        in_range = all(lo < v < hi for v in (red[s:s + w].min(), red[s:s + w].max(), ir[s:s + w].min(),
                                             ir[s:s + w].max()))
        good = bool(np.isfinite(r) and corr >= cfg.min_corr and pi_ir >= cfg.min_pi and still and pulse and in_range)
        out.append(Window((s + w) / cfg.fs, float(r), corr, float(pi_ir), good))
    return out


def burst(red, ir, motion_mg=None, pulse_ok=None, cfg=Spo2Config()):
    """(R or nan, confidence 0..1, good windows, all windows) for one burst."""
    ws = windows(red, ir, motion_mg, pulse_ok, cfg)
    good = np.array([x.r for x in ws if x.good])
    if len(good) < cfg.min_good:
        return np.nan, 0.0, len(good), len(ws)
    med = float(np.median(good))
    q1, q3 = np.percentile(good, [25, 75])
    spread = (q3 - q1) / med if med > 0 else np.inf
    conf = (len(good) / len(ws)) * float(np.clip(1.0 - spread / cfg.max_spread, 0.0, 1.0))
    return med, conf, len(good), len(ws)


def night_ratio(r_bursts, conf_bursts, min_conf=0.3):
    """Confidence-weighted median R of one night's bursts (nan if none pass min_conf)."""
    r = np.asarray(r_bursts, float)
    c = np.asarray(conf_bursts, float)
    m = np.isfinite(r) & (c >= min_conf)
    if not m.any():
        return np.nan
    order = np.argsort(r[m])
    rs, cs = r[m][order], c[m][order]
    return float(rs[np.searchsorted(np.cumsum(cs), cs.sum() / 2.0)])


@dataclass
class Calibration:
    """SpO2 % = a + b * R + c * R^2 over [r_min, r_max]."""
    a: float = 110.0
    b: float = -25.0     # placeholder: the textbook linear ratio-of-ratios line [ASSUMED], never shown to users
    c: float = 0.0
    r_min: float = 0.35
    r_max: float = 0.85
    pct_min: float = 90.0   # lowest value the calibration data covered; below it, no number
    validated: bool = False
    source: str = "placeholder"


def fit_calibration(r, ref_pct, quadratic=False, source="fit"):
    """Least-squares fit of reference oximeter % (burst means) against burst R. Returns (Calibration, arms),
    where arms is the root-mean-square error in % on the fitted data. Mark validated only after a held-out check."""
    r = np.asarray(r, float)
    y = np.asarray(ref_pct, float)
    m = np.isfinite(r) & np.isfinite(y)
    r, y = r[m], y[m]
    deg = 2 if quadratic else 1
    coef = np.polyfit(r, y, deg)[::-1]
    cal = Calibration(a=float(coef[0]), b=float(coef[1]), c=float(coef[2]) if deg == 2 else 0.0,
                      r_min=float(r.min()), r_max=float(r.max()), pct_min=float(np.floor(y.min())),
                      validated=False, source=source)
    return cal, arms(cal, r, y)


def percent(r, cal):
    """Calibrated % for R values (nan outside the calibrated R range); capped at 100."""
    r = np.asarray(r, float)
    p = cal.a + cal.b * r + cal.c * r ** 2
    p = np.where((r >= cal.r_min) & (r <= cal.r_max), p, np.nan)
    return np.minimum(p, 100.0)


def arms(cal, r, ref_pct):
    """Accuracy root mean square, %: sqrt(mean((estimate - reference)^2)) inside the calibrated range."""
    e = percent(r, cal) - np.asarray(ref_pct, float)
    e = e[np.isfinite(e)]
    return float(np.sqrt(np.mean(e ** 2))) if len(e) else np.nan


def nightly_percent(r_bursts, conf_bursts, cal, min_bursts=12, min_conf=0.3):
    """One nightly blood-oxygen number for the app: (whole %, or "lower than usual", or None).
    None when the calibration is not validated or fewer than `min_bursts` bursts pass (12 = one hour of sleep)."""
    if not cal.validated:
        return None
    r = np.asarray(r_bursts, float)
    c = np.asarray(conf_bursts, float)
    if int((np.isfinite(r) & (c >= min_conf)).sum()) < min_bursts:
        return None
    p = float(percent(night_ratio(r, c, min_conf), cal))
    if not np.isfinite(p) or p < cal.pct_min:
        return "lower than usual"
    return int(round(p))


def trend(night, history, min_nights=7):
    """Relative change of tonight's R against the median of up to the last 14 nights. Positive = higher R than
    usual (relatively less oxygenated). None until `min_nights` nights exist."""
    h = np.asarray([x for x in history[-14:] if np.isfinite(x)], float)
    if len(h) < min_nights or not np.isfinite(night):
        return None
    base = float(np.median(h))
    return (night - base) / base
