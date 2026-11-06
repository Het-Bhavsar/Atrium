"""Motion level on the LSM6DSOX machine-learning core (MLC), and the activity class built on it (FEATURES.md #6).

On the IMU (no MCU work, no extra accelerometer current):
 - Accelerometer at 12.5 Hz, +-2 g, the pod's worn-mode setting. The MLC also runs at 12.5 Hz.
 - One feature: variance of the accelerometer norm |a| over 39 samples (3.12 s, back to back).
 - One decision tree of 7 nodes, a ladder: level k is the first k with variance <= ATRIUM_THRESHOLDS[k], else 7.
   The level is in MLC0_SRC (0x70), with a pulse on INT2 each time it is updated.
The register program is ST's BSD-3 `motion_intensity` example for this part, which has the same structure
(STMicroelectronics st-mems-machine-learning-core, examples/motion_intensity/lsm6dsox; notice below). Atrium
changes only the 7 thresholds (half-precision floats inside the tree nodes) and routes the interrupt to INT2.
ST's ladder starts at 9e-3 g^2, which puts a still head, nodding and slow walking all in level 0. Atrium's ladder
is set from ST's head-mounted LSM6DSOX recordings (examples/head_gestures/lsm6dsox/datalogs, BSD-3), where the
median 3 s variance was 3.6e-6 g^2 still, 2.4e-3 nodding, 4.3e-3 swinging, 5.4e-3 shaking and 1.3e-2 walking.

Activity class (on the MCU, once a log record, from the levels and the HR code's accelerometer spectrum):
 still: every level in the record is 0. walk: median level 4-5 and a periodic step peak (hr.HrResult.acc_peak_bpm,
 peak share >= `periodic_share`) at 70-150 steps/min. run: median level >= 6 and a step peak at 140-200 steps/min.
 other: anything else that moves (head turns, nodding, talking with gestures, chewing, desk work). In ST's head
 data, shaking and swinging sit at level 3 with a weaker rhythm, so level 3 alone never counts as walking.
Every threshold is [ASSUMED] until bench test V2 on the temple.

ST notice (for ST_MOTION_INTENSITY): Copyright (c) 2025, STMicroelectronics. All rights reserved.
Redistribution and use in source and binary forms, with or without modification, are permitted provided that the
following conditions are met: 1. Redistributions of source code must retain the above copyright notice, this list
of conditions and the following disclaimer. 2. Redistributions in binary form must reproduce the above copyright
notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided
with the distribution. 3. Neither the name of the copyright holder nor the names of its contributors may be used to
endorse or promote products derived from this software without specific prior written permission. THIS SOFTWARE IS
PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
"""
from dataclasses import dataclass

import numpy as np

FS = 12.5
WINDOW = 39
LEVELS = 8

# variance of |a| in g^2; level k when variance <= threshold k [ASSUMED, bench V2]
ATRIUM_THRESHOLDS = (1e-4, 1e-3, 4e-3, 1e-2, 3e-2, 8e-2, 0.25)

# (register, value) writes in order, from ST's lsm6dsox_motion_intensity.json (reg_config 2.0)
ST_MOTION_INTENSITY = (
    (0x10, 0x00), (0x11, 0x00), (0x01, 0x80), (0x04, 0x00), (0x05, 0x00), (0x17, 0x40), (0x02, 0x11), (0x08, 0xEA),
    (0x09, 0x46), (0x09, 0x03), (0x09, 0x50), (0x09, 0x03), (0x09, 0x00), (0x09, 0x00), (0x09, 0x0A), (0x02, 0x11),
    (0x08, 0xF2), (0x09, 0x27), (0x02, 0x11), (0x08, 0xFA), (0x09, 0x3C), (0x09, 0x03), (0x09, 0x52), (0x09, 0x03),
    (0x09, 0x5E), (0x09, 0x03), (0x02, 0x31), (0x08, 0x3C), (0x09, 0x3F), (0x09, 0x00), (0x09, 0x01), (0x09, 0x10),
    (0x09, 0x00), (0x09, 0x00), (0x09, 0x00), (0x09, 0x00), (0x09, 0x1F), (0x09, 0x00), (0x02, 0x31), (0x08, 0x52),
    (0x09, 0x00), (0x09, 0x00), (0x09, 0x00), (0x09, 0x00), (0x09, 0x00), (0x09, 0x00), (0x09, 0x00), (0x09, 0x00),
    (0x09, 0x00), (0x09, 0x00), (0x09, 0x00), (0x01, 0x00), (0x01, 0x80), (0x17, 0x40), (0x02, 0x31), (0x08, 0x5E),
    (0x09, 0x9C), (0x09, 0x20), (0x09, 0x10), (0x09, 0xC0), (0x09, 0x04), (0x09, 0x23), (0x09, 0x21), (0x09, 0xC0),
    (0x09, 0xFE), (0x09, 0x25), (0x09, 0x32), (0x09, 0xC0), (0x09, 0x40), (0x09, 0x28), (0x09, 0x43), (0x09, 0xC0),
    (0x09, 0x00), (0x09, 0x2D), (0x09, 0x54), (0x09, 0xC0), (0x09, 0x40), (0x09, 0x31), (0x09, 0x65), (0x09, 0xC0),
    (0x09, 0x00), (0x09, 0x35), (0x09, 0x76), (0x09, 0xE0), (0x01, 0x80), (0x17, 0x00), (0x04, 0x00), (0x05, 0x10),
    (0x02, 0x01), (0x01, 0x00), (0x5E, 0x02), (0x01, 0x80), (0x0D, 0x01), (0x60, 0x05), (0x01, 0x00), (0x10, 0x10),
    (0x11, 0x00),
)

_FUNC_CFG_ACCESS = 0x01
_PAGE_SEL, _PAGE_ADDRESS, _PAGE_VALUE = 0x02, 0x08, 0x09
_MD1_CFG, _MD2_CFG = 0x5E, 0x5F          # main page: route embedded functions to INT1 / INT2 [VERIFY datasheet]
_MLC_INT1, _MLC_INT2 = 0x0D, 0x11        # embedded page: route the MLC tree to INT1 / INT2 [VERIFY datasheet]


def _tree_slice(seq):
    """Index range of the 28 page writes that hold the 7 tree nodes (4 bytes each, threshold first)."""
    for i in range(len(seq) - 1):
        if seq[i] == (_PAGE_SEL, 0x31) and seq[i + 1] == (_PAGE_ADDRESS, 0x5E):
            start = i + 2
            end = start
            while end < len(seq) and seq[end][0] == _PAGE_VALUE:
                end += 1
            if end - start != 28:
                raise ValueError("unexpected tree size")
            return start, end
    raise ValueError("tree block not found")


def decode_thresholds(seq=ST_MOTION_INTENSITY):
    s, _ = _tree_slice(seq)
    out = []
    for k in range(7):
        lo, hi = seq[s + 4 * k][1], seq[s + 4 * k + 1][1]
        out.append(float(np.frombuffer(bytes([lo, hi]), dtype="<f2")[0]))
    return tuple(out)


def atrium_config(thresholds=ATRIUM_THRESHOLDS, int_pin=2):
    """The register program firmware writes after the driver's init: ST's structure, Atrium's thresholds."""
    t = np.asarray(thresholds, float)
    if len(t) != 7 or np.any(np.diff(t) <= 0):
        raise ValueError("7 increasing thresholds")
    seq = list(ST_MOTION_INTENSITY)
    s, _ = _tree_slice(seq)
    for k, v in enumerate(t):
        b = np.float16(v).astype("<f2").tobytes()
        seq[s + 4 * k] = (_PAGE_VALUE, b[0])
        seq[s + 4 * k + 1] = (_PAGE_VALUE, b[1])
    if int_pin == 2:
        page = 0
        for i, (a, d) in enumerate(seq):
            if a == _FUNC_CFG_ACCESS:
                page = 1 if d & 0x80 else 0
            elif page == 0 and a == _MD1_CFG:
                seq[i] = (_MD2_CFG, d)
            elif page == 1 and a == _MLC_INT1:
                seq[i] = (_MLC_INT2, d)
    return tuple(seq)


def levels(acc_g, thresholds=ATRIUM_THRESHOLDS, fs=FS):
    """MLC model: one level per 39-sample window of the 12.5 Hz accelerometer (n, 3) in g.
    Thresholds are rounded to half precision as the IMU stores them."""
    acc = np.asarray(acc_g, float)
    if fs != FS:
        raise ValueError("the MLC runs at 12.5 Hz")
    t = np.asarray(thresholds, np.float16).astype(float)
    norm = np.linalg.norm(acc, axis=1)
    n = len(norm) // WINDOW
    var = norm[:n * WINDOW].reshape(n, WINDOW).var(axis=1)
    return np.searchsorted(t, var, side="left"), var


@dataclass
class ClassConfig:
    periodic_share: float = 0.65    # share of 1-3.5 Hz |a| power within 0.15 Hz of the peak [ASSUMED]; ST head
                                    # data: walking median 0.85, nod / swing / shake 0.44-0.59
    walk_levels: tuple = (4, 5)
    walk_spm: tuple = (70.0, 150.0)
    run_spm: tuple = (140.0, 200.0)


CLASSES = ("still", "walk", "run", "other")


def activity_class(record_levels, acc_peak_bpm, acc_peak_share, cfg=ClassConfig()):
    """One class per log record. record_levels: MLC levels seen in the record. acc_peak_*: step rhythm from the
    HR code's accelerometer spectrum (nan when still)."""
    lv = np.asarray(record_levels, int)
    if len(lv) == 0 or lv.max() == 0:
        return "still"
    periodic = np.isfinite(acc_peak_bpm) and acc_peak_share >= cfg.periodic_share
    top = int(np.median(lv))
    if periodic and top >= 6 and cfg.run_spm[0] <= acc_peak_bpm <= cfg.run_spm[1]:
        return "run"
    if periodic and cfg.walk_levels[0] <= top <= cfg.walk_levels[1] and cfg.walk_spm[0] <= acc_peak_bpm <= cfg.walk_spm[1]:
        return "walk"
    return "other"


def step_rhythm(acc_g, fs, lo_hz=1.0, hi_hz=3.5, nfft=128):
    """Dominant step frequency (steps/min, parabolic-refined) and its share of 1-3.5 Hz power, from |a| over the
    last 8 s (100 samples at 12.5 Hz), Hann window, 128-point real FFT: 0.5 KB on the pod, once a record."""
    n = np.linalg.norm(np.asarray(acc_g, float), axis=1)
    n = (n - n.mean()) * np.hanning(len(n))
    p = np.abs(np.fft.rfft(n, nfft)) ** 2
    f = np.fft.rfftfreq(nfft, 1 / fs)
    band = (f >= lo_hz) & (f <= hi_hz)
    if p[band].sum() <= 0:
        return np.nan, 0.0
    k = np.flatnonzero(band)[np.argmax(p[band])]
    h = int(round(0.15 / (f[1] - f[0])))      # +-0.15 Hz around the peak, the Hann main lobe of an 8 s window
    near = band & (np.abs(np.arange(len(p)) - k) <= h)
    share = p[near].sum() / p[band].sum()
    a, b, c = (p[k - 1], p[k], p[k + 1]) if 0 < k < len(p) - 1 else (0.0, 1.0, 0.0)
    den = a - 2 * b + c
    frac = float(np.clip(0.5 * (a - c) / den, -0.5, 0.5)) if den != 0 else 0.0
    return float((k + frac) * (f[1] - f[0]) * 60.0), float(share)
