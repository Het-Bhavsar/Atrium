"""Reference test vectors for porting hr, hrv and wear pulse presence to the pod firmware.

Usage (from the repo root):
    python3 app/python/make_test_vectors.py OUT_DIR
Synthetic signals only (atrium_ml.synth), so the files carry no dataset licence. For each case OUT_DIR gets:
 - <case>_input.csv: t_s, ppg_counts (raw intensity, as the AFE gives it: negate before processing),
   ax_mg, ay_mg, az_mg. 25 Hz, the accelerometer already on the PPG time base.
 - <case>_hr.csv: one row a second from hr.spectral_hr (motion subtraction on) and pulse presence from the
   unweighted spectrum: t_s, bpm, conf, peak_bpm, acc_peak_bpm, raw_conf, raw_peak_bpm, pulse_present.
 - <case>_hrv.csv: beat times and interval cleaning: beat_s, rr_ms, clean (first beat has no interval).
 - <case>_rmssd.csv: per 15 s log record: start_s, rmssd_ms (empty below 5 differences), n_diffs.
MLC and activity class (mlc.py):
 - mlc_config.csv: the LSM6DSOX register program (addr, data, in order) for INT2.
 - mlc_<case>_input.csv: t_s, ax_mg, ay_mg, az_mg at 12.5 Hz.
 - mlc_<case>_levels.csv: window, variance_g2, level (one per 39 samples); then per 3-window record the class
   and step rhythm in mlc_<case>_class.csv: record, spm, share, class.
SpO2 trend (spo2.py), one 30 s burst per case:
 - spo2_<case>_input.csv: t_s, red_counts, ir_counts, motion_mg, pulse_ok (motion and pulse per second).
 - spo2_<case>_windows.csv: t_s, r, corr, pi_ir, good; spo2_<case>_burst.csv: r, confidence, n_good, n_windows.
Port tolerances [ASSUMED]: bpm within 0.5, beat times within 2 ms, RMSSD within 1 ms, variance within 1 %, R within
0.5 %, flags, levels and classes equal except next to a threshold.
"""
import csv
import sys
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parent))
from atrium_ml import hr, hrv, mlc, spo2, synth, wear  # noqa: E402

FS = 25.0
DC_COUNTS = 200000.0      # [ASSUMED] mid-range on-skin DC for the 19-bit ADC
PULSE_COUNTS = 1500.0     # about 0.75 % perfusion


def _case(name, seconds, hr_bpm, rmssd_ms, cadence_hz=None, artifact=0.0, acc_amp_g=0.15, pulse=True, seed=0):
    beats = synth.beat_times(seconds, hr_bpm, rmssd_ms, seed=seed)
    bv = synth.ppg_from_beats(beats, FS, seconds, amp=1.0, dc=0.0, noise=0.02, seed=seed + 1)
    if not pulse:
        bv = np.random.default_rng(seed + 1).standard_normal(int(seconds * FS)) * 0.05
    if cadence_hz:
        acc = synth.walking_acc(FS, seconds, cadence_hz=cadence_hz, amp_g=acc_amp_g, seed=seed + 2)
        bv = bv + artifact * (acc[:, 2] - 1.0) / acc_amp_g
    else:
        acc = synth.walking_acc(FS, seconds, cadence_hz=1.0, amp_g=0.0, seed=seed + 2)
    counts = np.round(DC_COUNTS - PULSE_COUNTS * bv).astype(int)      # intensity falls as blood volume rises
    acc_mg = np.round(acc * 1000.0).astype(int)
    return name, counts, acc_mg


CASES = [
    ("rest_64bpm", 120, 64.0, 35.0, None, 0.0),
    ("walk_72bpm_steps108", 120, 72.0, 20.0, 1.8, 0.5),
    ("run_140bpm_steps168", 120, 140.0, 8.0, 2.8, 0.8),
    ("hrv_rest_58bpm_rmssd45", 300, 58.0, 45.0, None, 0.0),
]


def write(out):
    out = Path(out)
    out.mkdir(parents=True, exist_ok=True)
    specs = [_case(n, s, h, r, c, a, seed=i * 10) for i, (n, s, h, r, c, a) in enumerate(CASES)]
    specs.append(_case("offbody_noise", 120, 60.0, 0.0, pulse=False, seed=90))
    for name, counts, acc_mg in specs:
        t = np.arange(len(counts)) / FS
        with open(out / f"{name}_input.csv", "w", newline="") as f:
            w = csv.writer(f)
            w.writerow(["t_s", "ppg_counts", "ax_mg", "ay_mg", "az_mg"])
            for row in zip(np.round(t, 3), counts, *acc_mg.T):
                w.writerow(row)
        bv = -counts.astype(float)
        acc_g = acc_mg / 1000.0
        r = hr.spectral_hr(bv, acc_g)
        raw = hr.spectral_hr(bv, acc_g, motion=False)
        present = wear.pulse_present(raw.conf, raw.peak_bpm, raw.acc_peak_bpm)
        with open(out / f"{name}_hr.csv", "w", newline="") as f:
            w = csv.writer(f)
            w.writerow(["t_s", "bpm", "conf", "peak_bpm", "acc_peak_bpm", "raw_conf", "raw_peak_bpm", "pulse_present"])
            for i in range(len(r.t)):
                w.writerow([f"{r.t[i]:.0f}", f"{r.bpm[i]:.2f}", f"{r.conf[i]:.4f}", f"{r.peak_bpm[i]:.2f}",
                            f"{r.acc_peak_bpm[i]:.2f}", f"{raw.conf[i]:.4f}", f"{raw.peak_bpm[i]:.2f}",
                            int(present[i])])
        beats = hrv.detect_beats(bv, FS)
        _, rr, ok = hrv.clean_intervals(beats)
        with open(out / f"{name}_hrv.csv", "w", newline="") as f:
            w = csv.writer(f)
            w.writerow(["beat_s", "rr_ms", "clean"])
            w.writerow([f"{beats[0]:.4f}", "", ""] if len(beats) else [])
            for b, v, k in zip(beats[1:], rr, ok):
                w.writerow([f"{b:.4f}", f"{v:.2f}", int(k)])
        with open(out / f"{name}_rmssd.csv", "w", newline="") as f:
            w = csv.writer(f)
            w.writerow(["start_s", "rmssd_ms", "n_diffs"])
            for t0, val, n in hrv.windowed_rmssd(beats, 0.0, t[-1], 15.0, hrv.HrvConfig(min_diffs=5)):
                w.writerow([f"{t0:.0f}", "" if val is None else f"{val:.2f}", n])
    names = [s[0] for s in specs]
    names += _write_mlc(out)
    names += _write_spo2(out)
    return names


MLC_CASES = [("still", 0.0, 1.0), ("head_motion", 0.06, 1.0), ("walk", 0.16, 1.75), ("run", 0.5, 2.75)]


def _write_mlc(out):
    with open(out / "mlc_config.csv", "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["addr", "data"])
        for a, d in mlc.atrium_config():
            w.writerow([f"0x{a:02X}", f"0x{d:02X}"])
    names = []
    for i, (name, amp, hz) in enumerate(MLC_CASES):
        rng = np.random.default_rng(200 + i)
        t = np.arange(int(39 * 8)) / mlc.FS
        a = np.tile([0.0, 0.0, 1.0], (len(t), 1))
        a[:, 2] += amp * np.sin(2 * np.pi * hz * t)
        a[:, 0] += 0.3 * amp * np.sin(np.pi * hz * t + 0.4)
        a_mg = np.round((a + 0.001 * rng.standard_normal(a.shape)) * 1000.0).astype(int)
        acc = a_mg / 1000.0
        with open(out / f"mlc_{name}_input.csv", "w", newline="") as f:
            w = csv.writer(f)
            w.writerow(["t_s", "ax_mg", "ay_mg", "az_mg"])
            for row in zip(np.round(t, 3), *a_mg.T):
                w.writerow(row)
        lv, var = mlc.levels(acc)
        with open(out / f"mlc_{name}_levels.csv", "w", newline="") as f:
            w = csv.writer(f)
            w.writerow(["window", "variance_g2", "level"])
            for k, (v, l) in enumerate(zip(var, lv)):
                w.writerow([k, f"{v:.6g}", int(l)])
        with open(out / f"mlc_{name}_class.csv", "w", newline="") as f:
            w = csv.writer(f)
            w.writerow(["record", "spm", "share", "class"])
            for k in range(len(lv) - 2):
                seg = acc[k * mlc.WINDOW:(k + 3) * mlc.WINDOW][-100:]
                spm, share = mlc.step_rhythm(seg, mlc.FS)
                w.writerow([k, f"{spm:.2f}", f"{share:.4f}", mlc.activity_class(lv[k:k + 3], spm, share)])
        names.append(f"mlc_{name}")
    return names


SPO2_CASES = [("r045", 0.45, 0.004, None), ("r070", 0.70, 0.004, None), ("moved", 0.5, 0.004, (12, 20)),
              ("low_perfusion", 0.5, 5e-5, None)]


def _write_spo2(out):
    names = []
    for i, (name, r_true, pi_ir, moved) in enumerate(SPO2_CASES):
        rng = np.random.default_rng(300 + i)
        n = int(30 * FS)
        t = np.arange(n) / FS
        beats = synth.beat_times(30, 62.0, 30.0, seed=300 + i)
        bv = synth.ppg_from_beats(beats, FS, 30, amp=1.0, dc=0.0)
        bv = (bv - bv.mean()) / bv.std()
        ir = DC_COUNTS * (1 - pi_ir * bv)
        red = 0.8 * DC_COUNTS * (1 - r_true * pi_ir * bv)
        motion = np.full(30, 3.0)
        if moved:
            motion[moved[0]:moved[1]] = 60.0
            k = (t >= moved[0]) & (t < moved[1])
            art = 0.01 * np.sin(2 * np.pi * 1.8 * t[k])
            ir[k] *= 1 + art
            red[k] *= 1 + 1.6 * art
        red = np.round(red + 20 * rng.standard_normal(n)).astype(int)
        ir = np.round(ir + 20 * rng.standard_normal(n)).astype(int)
        pulse = np.ones(30, bool)
        with open(out / f"spo2_{name}_input.csv", "w", newline="") as f:
            w = csv.writer(f)
            w.writerow(["t_s", "red_counts", "ir_counts", "motion_mg", "pulse_ok"])
            for k in range(n):
                w.writerow([f"{t[k]:.2f}", red[k], ir[k], f"{motion[int(t[k])]:.1f}", int(pulse[int(t[k])])])
        ws = spo2.windows(red, ir, motion, pulse)
        with open(out / f"spo2_{name}_windows.csv", "w", newline="") as f:
            w = csv.writer(f)
            w.writerow(["t_s", "r", "corr", "pi_ir", "good"])
            for x in ws:
                w.writerow([f"{x.t_s:.1f}", f"{x.r:.5f}", f"{x.corr:.4f}", f"{x.pi_ir:.6f}", int(x.good)])
        r, conf, ng, nw = spo2.burst(red, ir, motion, pulse)
        with open(out / f"spo2_{name}_burst.csv", "w", newline="") as f:
            w = csv.writer(f)
            w.writerow(["r", "confidence", "n_good", "n_windows"])
            w.writerow(["" if np.isnan(r) else f"{r:.5f}", f"{conf:.4f}", ng, nw])
        names.append(f"spo2_{name}")
    return names


if __name__ == "__main__":
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    print("\n".join(write(sys.argv[1])))
