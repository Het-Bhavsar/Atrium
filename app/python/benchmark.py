"""Run the Atrium prototypes on public PhysioNet records and write the numbers in app/python/README.md.

Usage (from the repo root; download the records first, see app/python/README.md):
    python3 app/python/benchmark.py --ptt DATA/pulse-transit-time-ppg --wrist DATA/wrist \
        --sleep DATA/sleep-accel --st-head DATA/st_head_gestures --out app/python/results/benchmark.json
Every PPG is resampled to the pod rate before it is processed. Results are wellness-algorithm checks, not
clinical validation.
"""
import argparse
import json
import sys
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parent))
from atrium_ml import datasets, dsp, hr, hrv, mlc, position, sleep, spo2, synth, wear  # noqa: E402

FS_HR = 25.0


def _stats(err):
    e = np.abs(np.asarray(err, float))
    e = e[np.isfinite(e)]
    if len(e) == 0:
        return {"n": 0}
    return {"n": int(len(e)), "mae": round(float(e.mean()), 2), "within_5": round(float((e <= 5).mean()), 3),
            "within_10": round(float((e <= 10).mean()), 3)}


def _prep(rec, fs):
    ppg = dsp.resample(rec.ppg, rec.fs, fs)
    acc = None if rec.acc_g is None else dsp.resample(rec.acc_g, rec.fs, fs)
    if acc is not None:
        n = min(len(ppg), len(acc))
        ppg, acc = ppg[:n], acc[:n]
    return ppg, acc


def bench_hr(records):
    rows = {}
    for rec in records:
        ppg, acc = _prep(rec, FS_HR)
        methods = {
            "spectral_imu": hr.spectral_hr(ppg, acc),
            "spectral_no_imu": hr.spectral_hr(ppg, acc, motion=False),
            "firmware_autocorr": hr.autocorr_hr(ppg, FS_HR),
        }
        for m, r in methods.items():
            ref = hr.reference_hr(rec.r_peaks_s, r.t)
            keep = r.t >= 16.0   # 8 s window plus 8 s to acquire
            d = rows.setdefault((m, rec.activity), {"all": [], "valid": [], "cov": [], "motion": []})
            d["all"] += list((r.bpm - ref)[keep])
            d["valid"] += list(np.where(r.valid, r.bpm - ref, np.nan)[keep])
            d["cov"] += list(r.valid[keep])
            d["motion"] += list(r.motion_mg[keep])
    out = {}
    for (m, act), d in sorted(rows.items()):
        out.setdefault(m, {})[act] = {"output": _stats(d["all"]), "confident_only": _stats(d["valid"]),
                                      "coverage": round(float(np.mean(d["cov"])), 3),
                                      "motion_mg_median": round(float(np.median(d["motion"])), 1)}
    return out


REF_CFG = hrv.HrvConfig(max_dev=0.20)   # ECG reference: the usual 20 % ectopic-beat filter


def _ref_rmssd(r_peaks, t0, t1, win):
    return hrv.windowed_rmssd(r_peaks, t0, t1, win, REF_CFG)


def bench_hrv(records, rates=(25.0, 50.0, 64.0, 100.0), interps=("parabolic", "cubic", "none"), win_s=60.0):
    out = {}
    still = [r for r in records if r.activity == "sit"]
    for fs in rates:
        for interp in interps:
            cfg = hrv.HrvConfig(interp=interp)
            pairs, per_rec = [], []
            for rec in still:
                ppg = dsp.resample(rec.ppg, rec.fs, fs)
                beats = hrv.detect_beats(ppg, fs, cfg)
                t1 = min(len(ppg) / fs, rec.r_peaks_s[-1])
                ref = _ref_rmssd(rec.r_peaks_s, 10.0, t1, win_s)
                est = hrv.windowed_rmssd(beats, 10.0, t1, win_s, cfg)
                for (t, rr_ref, n_ref), (_, rr_est, n_est) in zip(ref, est):
                    if rr_ref is not None and rr_est is not None:
                        pairs.append((rr_ref, rr_est))
                pr, pe = hrv.pooled([(a, b) for _, a, b in ref]), hrv.pooled([(a, b) for _, a, b in est])
                if pr and pe:
                    per_rec.append((pr, pe))
            key = f"{int(fs)}Hz_{interp}"
            out[key] = {"windows_60s": _agree(pairs, len(still) * 8), "records_pooled": _agree(per_rec, len(still))}
    return out


def _agree(pairs, n_possible):
    if not pairs:
        return {"n": 0}
    a = np.array(pairs)
    d = a[:, 1] - a[:, 0]
    return {"n": int(len(a)), "coverage": round(len(a) / max(n_possible, 1), 3),
            "ref_median_ms": round(float(np.median(a[:, 0])), 1), "bias_ms": round(float(d.mean()), 2),
            "mae_ms": round(float(np.abs(d).mean()), 2),
            "mape": round(float(np.mean(np.abs(d) / a[:, 0])), 3),
            "loa_ms": [round(float(d.mean() - 1.96 * d.std()), 1), round(float(d.mean() + 1.96 * d.std()), 1)],
            "r": round(float(np.corrcoef(a[:, 0], a[:, 1])[0, 1]), 3)}


def _per_second(rec):
    """Per-second wear inputs from a real on-skin record."""
    ppg, acc = _prep(rec, FS_HR)
    r = hr.spectral_hr(ppg, acc, motion=False)
    n = len(r.t)
    intensity = -dsp.resample(rec.ppg, rec.fs, FS_HR)        # undo the blood-volume sign flip
    dc = wear.dc_per_second(intensity, FS_HR)
    temp = dsp.resample(rec.temp_c, rec.fs, 1.0) if rec.temp_c is not None else np.full(len(dc), 33.0)
    k = 8                                                      # first spectral estimate ends at 8 s
    m = min(n, len(dc) - k, len(temp) - k)
    pulse = wear.pulse_present(r.conf, r.peak_bpm, r.acc_peak_bpm)
    return dc[k:k + m], pulse[:m], r.motion_mg[:m], temp[k:k + m], intensity, acc


def bench_wear(records, seed=0):
    rng = np.random.default_rng(seed)
    cfg = wear.WearConfig()
    on_total, on_false = 0, 0
    lat = {"lifted_open_air": [], "table_lamp": [], "pocket_walking": []}
    false_on = {k: 0 for k in lat}
    secs = {k: 0 for k in lat}
    for rec in records:
        dc, pulse, mot, temp, intensity, acc = _per_second(rec)
        w = wear.run(dc, pulse, mot, temp, cfg)
        on_total += len(w)
        on_false += int((~w).sum())
        # Off-body continuations. Pulse presence comes from running the real HR code on the off-body PPG.
        dur = 600
        n = int(dur * FS_HR)
        noise = np.std(np.diff(intensity)) / np.sqrt(2) / np.median(intensity)   # AFE noise, relative to on-skin DC
        t_end = temp[-1]
        # the record's own accelerometer when it is a walk or run, else a synthetic walk
        walk_acc = synth.walking_acc(FS_HR, dur, seed=seed) if rec.activity == "sit" else np.resize(acc, (n, 3))
        scen = {   # (DC level, motion-coupled reflection, accelerometer, room temperature)
            "lifted_open_air": (0.02, 0.0, synth.walking_acc(FS_HR, dur, amp_g=0.05, seed=seed), 24.0),
            "table_lamp": (0.6, 0.0, np.tile([0.0, 0.0, 1.0], (n, 1)) + 0.002 * rng.standard_normal((n, 3)), 24.0),
            "pocket_walking": (0.5, 0.15, walk_acc, 30.0),
        }
        for name, (level, mod, a, t_room) in scen.items():
            # PPG: a level, cloth or lamp; plus motion-coupled reflection change; plus AFE noise. No pulse.
            p = level + mod * (a[:, 2] - a[:, 2].mean()) + noise * rng.standard_normal(n)
            r = hr.spectral_hr(p, a, motion=False)
            pres = wear.pulse_present(r.conf, r.peak_bpm, r.acc_peak_bpm)
            dcs = wear.dc_per_second(p, FS_HR, dc_ref=1.0)[8:8 + len(r.t)]
            t = t_room + (t_end - t_room) * np.exp(-np.arange(len(r.t)) / 240.0)  # [ASSUMED] 4 min pod time constant
            seq_dc = np.r_[dc[-60:], dcs]
            seq = wear.run(seq_dc, np.r_[pulse[-60:], pres], np.r_[mot[-60:], r.motion_mg], np.r_[temp[-60:], t], cfg)
            off = seq[60:]
            first = int(np.argmax(~off)) if (~off).any() else len(off)
            lat[name].append(first)
            # after going off, does it ever wrongly come back on?
            false_on[name] += int(off[first:].sum())
            secs[name] += len(off) - first
    return {
        "on_skin_records": len(records), "on_skin_seconds": on_total,
        "false_off_body_fraction": round(on_false / max(on_total, 1), 4),
        "off_body_latency_s": {k: {"median": float(np.median(v)), "max": int(np.max(v))} for k, v in lat.items()},
        "false_on_fraction_after_off": {k: round(false_on[k] / max(secs[k], 1), 4) for k in lat},
    }


def bench_position(n_mounts=400, seed=0):
    """Monte Carlo over placement errors and pillow tilt; no public head-worn sleep IMU set was found."""
    rng = np.random.default_rng(seed)
    head_up = {"back": (0, 1, 0), "front": (0, -1, 0), "right": (-1, 0, 0), "left": (1, 0, 0), "upright": (0, 0, 1)}
    base = np.array([[0, 1, 0], [0, 0, 1], [1, 0, 0]], float)
    res = {}
    for max_tilt in (10, 20, 30, 40):
        ok, tot, conf = 0, 0, {}
        for _ in range(n_mounts):
            m = synth.rotation((0, 0, 1), rng.uniform(-180, 180)) @ synth.rotation(
                rng.normal(size=3), rng.uniform(0, max_tilt)) @ base
            up_imu = m @ np.array([0, 0, 1.0])
            acc = synth.walking_acc(FS_HR, 60, g_vec=up_imu, seed=int(rng.integers(1 << 30)))
            frame = position.head_frame(position.upright_from_walking(acc, np.full(len(acc), 100.0)), side="right")
            for lab, v in head_up.items():
                # pillow and neck: the head is rotated up to 25 deg about its other axes in each position
                h = synth.rotation(rng.normal(size=3), rng.uniform(0, 25)) @ np.array(v, float)
                g = m @ h + 0.01 * rng.standard_normal(3)
                got = position.classify_vector(g, frame)
                ok += got == lab
                tot += 1
                conf[(lab, got)] = conf.get((lab, got), 0) + 1
        res[f"tilt_up_to_{max_tilt}deg"] = {"accuracy": round(ok / tot, 3), "n": tot,
                                             "errors": {f"{a}->{b}": c for (a, b), c in conf.items() if a != b}}
    return res


def bench_sleep(nights):
    """Leave-one-night-out: train on 30 nights, score the 31st."""
    feats = [sleep.epoch_features(n.act_mg, n.hr_bpm) for n in nights]
    res = {}
    for name, cols, k in (("wake_sleep_motion_only", [0, 1, 2, 5], 2), ("wake_sleep", list(range(6)), 2),
                          ("wake_nrem_rem", list(range(6)), 3)):
        ys, ps, tst = [], [], []
        for i, n in enumerate(nights):
            X = np.vstack([f[:, cols] for j, f in enumerate(feats) if j != i])
            y = np.concatenate([(m.stage > 0).astype(int) if k == 2 else m.stage for j, m in enumerate(nights) if j != i])
            model = sleep.fit(X, y, balanced=k == 3)   # 3 classes: equal class weights, else REM is never called
            yt = (n.stage > 0).astype(int) if k == 2 else n.stage
            pr = sleep.predict(model, feats[i][:, cols])
            ys.append(yt); ps.append(pr)
            tst.append((pr > 0).sum() * 0.5 - (yt > 0).sum() * 0.5)   # minutes of sleep, estimate - PSG
        y, p = np.concatenate(ys), np.concatenate(ps)
        r = {"epochs": int(len(y)), "accuracy": round(float((y == p).mean()), 3), "kappa": round(sleep.kappa(y, p), 3)}
        if k == 2:
            r["wake_correct"] = round(float((p[y == 0] == 0).mean()), 3)
            r["sleep_correct"] = round(float((p[y == 1] == 1).mean()), 3)
            r["sleep_time_error_min"] = {"mean": round(float(np.mean(tst)), 1), "mae": round(float(np.mean(np.abs(tst))), 1)}
        else:
            r["per_class_correct"] = {c: round(float((p[y == j] == j).mean()), 3) for j, c in enumerate(("wake", "nrem", "rem"))}
        res[name] = r
    res["nights"] = len(nights)
    return res


def bench_spo2(records, burst_s=30.0):
    """Ratio of ratios on the PTT red and IR channels, cut into 30 s bursts at the pod's 25 Hz.
    The finger is not the temple, and SpO2 only varied 94-99 % between people, so this checks repeatability
    and gating, not accuracy."""
    out = {}
    for rec in records:
        red = -dsp.resample(rec.red, rec.fs, FS_HR)     # back to raw intensity
        ir = -dsp.resample(rec.ir, rec.fs, FS_HR)
        ppg, acc = _prep(rec, FS_HR)
        raw = hr.spectral_hr(ppg, acc, motion=False)
        pres = np.r_[np.zeros(8, bool), wear.pulse_present(raw.conf, raw.peak_bpm, raw.acc_peak_bpm)]
        mot = np.r_[np.full(8, np.nan), raw.motion_mg]
        n = int(burst_s * FS_HR)
        rs, cs = [], []
        for b0 in range(int(16 * FS_HR), len(red) - n + 1, n):
            sec = slice(b0 // int(FS_HR), (b0 + n) // int(FS_HR))
            r, c, _, _ = spo2.burst(red[b0:b0 + n], ir[b0:b0 + n], mot[sec], pres[sec])
            rs.append(r); cs.append(c)
        rs, cs = np.array(rs), np.array(cs)
        d = out.setdefault(rec.activity, {"bursts": 0, "passed": 0, "cv": [], "r": []})
        d["bursts"] += len(rs)
        ok = np.isfinite(rs) & (cs >= 0.3)
        d["passed"] += int(ok.sum())
        if ok.sum() >= 3:
            d["cv"].append(float(np.std(rs[ok]) / np.mean(rs[ok])))
            d["r"].append(float(np.median(rs[ok])))
    res = {}
    for act, d in out.items():
        res[act] = {"bursts": d["bursts"], "passed_share": round(d["passed"] / max(d["bursts"], 1), 3),
                    "records_with_3_passed": len(d["cv"]),
                    "within_record_cv_median": round(float(np.median(d["cv"])), 3) if d["cv"] else None,
                    "r_median": round(float(np.median(d["r"])), 3) if d["r"] else None,
                    "r_range": [round(min(d["r"]), 3), round(max(d["r"]), 3)] if d["r"] else None}
    return res


def bench_mlc(st_head_dir):
    """MLC level and activity class on ST's head-mounted LSM6DSOX logs (26 Hz, resampled to the MLC's 12.5 Hz).
    The thresholds were set from these same logs, so this is a consistency check, not a held-out test."""
    from scipy import signal
    truth = {"stationary": "still", "nod": "other", "shake": "other", "swing": "other", "walk": "walk"}
    res = {}
    for name, want in truth.items():
        a = np.loadtxt(Path(st_head_dir) / f"{name}.csv", delimiter=",", skiprows=1)[:, :3] / 1000.0
        a12 = signal.resample_poly(a, 25, 52, axis=0)
        lv, var = mlc.levels(a12)
        got = []
        for i in range(len(lv) - 2):            # 3 MLC windows = a 9.4 s record
            seg = a12[i * mlc.WINDOW:(i + 3) * mlc.WINDOW]
            spm, share = mlc.step_rhythm(seg[-100:], mlc.FS)
            got.append(mlc.activity_class(lv[i:i + 3], spm, share))
        res[name] = {"windows": int(len(lv)), "level_counts": np.bincount(lv, minlength=8).tolist(),
                     "variance_median_g2": float(f"{np.median(var):.3g}"), "truth": want,
                     "class_correct": round(float(np.mean(np.array(got) == want)), 3), "records": len(got)}
    return res


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--ptt", required=True)
    ap.add_argument("--wrist")
    ap.add_argument("--sleep", help="sleep-accel folder (labels/, motion/, heart_rate/)")
    ap.add_argument("--st-head", help="ST head_gestures/lsm6dsox/datalogs folder (BSD-3)")
    ap.add_argument("--out", required=True)
    args = ap.parse_args()
    ptt = datasets.load_ptt(args.ptt)
    res = {"pod_rate_hz": FS_HR, "ptt_records": len(ptt)}
    res["hr_ptt"] = bench_hr(ptt)
    if args.wrist:
        wr = datasets.load_wrist(args.wrist)
        res["wrist_records"] = len(wr)
        res["hr_wrist"] = bench_hr(wr)
    res["hrv_ptt_sit"] = bench_hrv(ptt)
    res["wear_ptt"] = bench_wear(ptt)
    res["position_mc"] = bench_position()
    res["spo2_ptt"] = bench_spo2(ptt)
    if args.st_head:
        res["mlc_st_head"] = bench_mlc(args.st_head)
    if args.sleep:
        res["sleep_accel"] = bench_sleep(datasets.load_sleep_accel(args.sleep))
    Path(args.out).parent.mkdir(parents=True, exist_ok=True)
    Path(args.out).write_text(json.dumps(res, indent=1))
    print(json.dumps(res, indent=1))


if __name__ == "__main__":
    main()
