"""Synthetic signals with a known truth, for the tests and for cases with no public recording."""
import numpy as np


def beat_times(duration_s, hr_bpm=60.0, rmssd_ms=40.0, seed=0):
    """Beat times whose successive differences have the requested RMSSD (Gaussian, white)."""
    rng = np.random.default_rng(seed)
    rr0 = 60.0 / hr_bpm
    n = int(duration_s / rr0) + 5
    # white RR noise of sd s gives RMSSD = sqrt(2) s
    rr = rr0 + rng.normal(0, rmssd_ms / 1000.0 / np.sqrt(2), n)
    t = np.cumsum(rr)
    return t[t < duration_s]


def ppg_from_beats(beats, fs, duration_s, amp=1.0, dc=100.0, noise=0.0, seed=0):
    """Blood-volume-like pulse: systolic and diastolic Gaussians after each beat (rising = more blood)."""
    rng = np.random.default_rng(seed)
    t = np.arange(int(duration_s * fs)) / fs
    x = np.zeros_like(t)
    for b in beats:
        x += amp * np.exp(-0.5 * ((t - b - 0.18) / 0.07) ** 2)
        x += 0.4 * amp * np.exp(-0.5 * ((t - b - 0.45) / 0.10) ** 2)
    return dc + x + noise * rng.standard_normal(len(t))


def walking_acc(fs, duration_s, cadence_hz=1.8, amp_g=0.15, seed=0, g_vec=(0.0, 0.0, 1.0)):
    """Head acceleration while walking: vertical bounce at the step rate plus sway at half of it."""
    rng = np.random.default_rng(seed)
    t = np.arange(int(duration_s * fs)) / fs
    g = np.asarray(g_vec, float)
    acc = np.tile(g, (len(t), 1))
    acc[:, 2] += amp_g * np.sin(2 * np.pi * cadence_hz * t)
    acc[:, 0] += 0.4 * amp_g * np.sin(np.pi * cadence_hz * t + 0.3)
    acc += 0.01 * rng.standard_normal(acc.shape)
    return acc


def rotation(axis, deg):
    a = np.asarray(axis, float)
    a = a / np.linalg.norm(a)
    th = np.radians(deg)
    k = np.array([[0, -a[2], a[1]], [a[2], 0, -a[0]], [-a[1], a[0], 0]])
    return np.eye(3) + np.sin(th) * k + (1 - np.cos(th)) * k @ k
