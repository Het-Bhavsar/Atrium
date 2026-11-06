"""Shared filters and resampling. Everything here has a cheap fixed-point or float C equivalent on the pod."""
import numpy as np
from scipy import signal


def resample(x, fs_in, fs_out):
    """Anti-aliased resample to the pod rate, as the AFE decimator would deliver it."""
    x = np.asarray(x, dtype=float)
    if fs_in == fs_out:
        return x.copy()
    g = np.gcd(int(round(fs_in)), int(round(fs_out)))
    return signal.resample_poly(x, int(round(fs_out)) // g, int(round(fs_in)) // g, axis=0)


def butter_sos(lo, hi, fs, order=2):
    """Band-pass (or high-pass when hi is None) biquad cascade. Order 2 = 2 biquads for a band-pass."""
    if hi is None:
        return signal.butter(order, lo, btype="highpass", fs=fs, output="sos")
    return signal.butter(order, [lo, hi], btype="bandpass", fs=fs, output="sos")


def bandpass(x, lo, hi, fs, order=2, causal=True):
    """Causal by default, because the pod filters sample by sample."""
    sos = butter_sos(lo, hi, fs, order)
    if causal:
        # start in steady state at the first sample, as the pod primes its filters on the first reading
        x = np.asarray(x, dtype=float)
        zi = signal.sosfilt_zi(sos)
        zi = zi[:, :, None] * x[0] if x.ndim == 2 else zi * x[0]
        return signal.sosfilt(sos, x, axis=0, zi=zi)[0]
    return signal.sosfiltfilt(sos, x, axis=0)


def parabolic(y, i):
    """Sub-bin offset of the peak at index i from its two neighbours, in bins (-0.5..0.5)."""
    if i <= 0 or i >= len(y) - 1:
        return 0.0
    a, b, c = y[i - 1], y[i], y[i + 1]
    den = a - 2 * b + c
    return 0.0 if den == 0 else float(np.clip(0.5 * (a - c) / den, -0.5, 0.5))


def windows(n, fs, win_s, step_s):
    """Start indices of full windows."""
    w = int(round(win_s * fs))
    s = int(round(step_s * fs))
    return range(0, max(n - w + 1, 0), s), w
