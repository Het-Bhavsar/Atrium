"""Loaders for the public PhysioNet records used by benchmark.py. Needs `wfdb` (requirements.txt).

- pulse-transit-time-ppg 1.1.0 (ODbL 1.0): 22 adults, sit / stationary walk / run, about 8 min each.
  Finger-clip PPG (green, red, IR) at 500 Hz, 3-lead ECG with checked R-peak annotations, IMU, sensor
  temperature. https://physionet.org/content/pulse-transit-time-ppg/1.1.0/
- wrist 1.0.0, "Wrist PPG During Exercise" (ODC-By 1.0): 8 adults, walk, run, bike. Wrist green PPG at 256 Hz,
  chest ECG, wrist accelerometer. https://physionet.org/content/wrist/1.0.0/
- sleep-accel 1.0.0 (ODC-By 1.0): 31 adults, one lab night each. Wrist accelerometer in g, wrist pulse in bpm, and
  sleep stages scored from polysomnography every 30 s. https://physionet.org/content/sleep-accel/1.0.0/
None is head-worn; see the README for what that means for the numbers.
"""
from dataclasses import dataclass
from pathlib import Path

import numpy as np


@dataclass
class Record:
    name: str
    activity: str
    fs: float
    ppg: np.ndarray            # green, blood-volume polarity (rises with each beat)
    red: np.ndarray | None
    ir: np.ndarray | None
    acc_g: np.ndarray | None   # (n, 3)
    temp_c: np.ndarray | None  # sensor temperature at fs, or None
    r_peaks_s: np.ndarray


def _fill(x):
    """Linear fill of missing samples (the wrist records have about 0.1 % NaN gaps)."""
    if x is None:
        return None
    x = np.array(x, float)
    for col in (x.T if x.ndim == 2 else [x]):
        bad = ~np.isfinite(col)
        if bad.any() and not bad.all():
            idx = np.arange(len(col))
            col[bad] = np.interp(idx[bad], idx[~bad], col[~bad])
    return x


def _ch(rec, name):
    return _fill(rec.p_signal[:, rec.sig_name.index(name)]) if name in rec.sig_name else None


def load_ptt(root):
    import wfdb
    root = Path(root)
    out = []
    for hea in sorted(root.glob("s*_*.hea")):
        name = hea.stem
        if not (root / f"{name}.atr").exists() or not (root / f"{name}.dat").exists():
            continue
        rec = wfdb.rdrecord(str(root / name))
        ann = wfdb.rdann(str(root / name), "atr")
        fs = float(rec.fs)
        # pleth_3 / pleth_6 are green (distal / proximal phalanx). Raw values are light intensity, which falls
        # as blood volume rises, so the sign is flipped.
        acc = np.column_stack([_ch(rec, k) for k in ("a_x", "a_y", "a_z")])
        if np.nanmedian(np.linalg.norm(acc, axis=1)) > 5:   # the header says g, the values are m/s^2
            acc = acc / 9.81
        out.append(Record(name, name.split("_", 1)[1], fs, -_ch(rec, "pleth_3"), -_ch(rec, "pleth_1"),
                          -_ch(rec, "pleth_2"), acc, _ch(rec, "temp_1"), ann.sample / fs))
    return out


def load_wrist(root):
    import wfdb
    root = Path(root)
    out = []
    for hea in sorted(root.glob("s*_*.hea")):
        name = hea.stem
        if not (root / f"{name}.atr").exists():
            continue
        rec = wfdb.rdrecord(str(root / name))
        ann = wfdb.rdann(str(root / name), "atr")
        fs = float(rec.fs)
        names = [s.lower() for s in rec.sig_name]
        ppg_i = next(i for i, s in enumerate(names) if "pleth" in s or "ppg" in s)
        acc_i = [i for i, s in enumerate(names) if s.startswith("wrist_low_noise_accelerometer")
                 or s.startswith("wrist_wide_range_accelerometer")][:3]
        acc = _fill(rec.p_signal[:, acc_i]) if len(acc_i) == 3 else None
        if acc is not None and np.nanmedian(np.linalg.norm(acc, axis=1)) > 5:   # m/s^2 to g
            acc = acc / 9.81
        temp_i = [i for i, s in enumerate(names) if "temp" in s]
        temp = _fill(rec.p_signal[:, temp_i[0]]) if temp_i else None
        out.append(Record(name, name.split("_", 1)[1], fs, _fill(rec.p_signal[:, ppg_i]), None, None, acc, temp,
                          ann.sample / fs))
    return out


@dataclass
class Night:
    subject: str
    act_mg: np.ndarray    # per 30 s epoch, the pod's activity index: mean of | |a| - 1 g |
    hr_bpm: np.ndarray    # per epoch, nan when no reading
    stage: np.ndarray     # 0 wake, 1 non-REM, 2 REM


def load_sleep_accel(root):
    root = Path(root)
    out = []
    for lab in sorted((root / "labels").glob("*_labeled_sleep.txt")):
        sid = lab.name.split("_")[0]
        mot, hrf = root / "motion" / f"{sid}_acceleration.txt", root / "heart_rate" / f"{sid}_heartrate.txt"
        if not (mot.exists() and hrf.exists()) or mot.stat().st_size == 0:
            continue
        lt = np.loadtxt(lab)
        keep = np.isin(lt[:, 1], [0, 1, 2, 3, 4, 5])
        t0, st = lt[keep, 0], lt[keep, 1].astype(int)
        stage = np.where(st == 0, 0, np.where(st == 5, 2, 1))
        m = np.loadtxt(mot)
        h = np.loadtxt(hrf, delimiter=",")
        dev = np.abs(np.linalg.norm(m[:, 1:4], axis=1) - 1.0) * 1000.0
        idx = np.searchsorted(m[:, 0], np.r_[t0, t0[-1] + 30.0])
        act = np.array([dev[a:b].mean() if b > a else np.nan for a, b in zip(idx[:-1], idx[1:])])
        hidx = np.searchsorted(h[:, 0], np.r_[t0, t0[-1] + 30.0])
        hr = np.array([h[a:b, 1].mean() if b > a else np.nan for a, b in zip(hidx[:-1], hidx[1:])])
        out.append(Night(sid, act, hr, stage))
    return out
