import numpy as np

from atrium_ml import spo2

FS = 25.0


def _burst(r_true=0.5, seconds=30, pi_ir=0.004, hr_hz=1.1, dc=200000.0, noise=0.0, seed=0):
    rng = np.random.default_rng(seed)
    t = np.arange(int(seconds * FS)) / FS
    pulse = np.sin(2 * np.pi * hr_hz * t) + 0.3 * np.sin(4 * np.pi * hr_hz * t + 0.5)
    pulse /= np.sqrt(np.mean(pulse ** 2))
    ir = dc * (1 - pi_ir * pulse) + noise * rng.standard_normal(len(t))
    red = dc * (1 - r_true * pi_ir * pulse) + noise * rng.standard_normal(len(t))
    return red, ir


def test_ratio_recovered():
    for r in (0.45, 0.6, 0.9):
        red, ir = _burst(r)
        est, conf, n_good, n = spo2.burst(red, ir)
        assert abs(est - r) < 0.02 * r
        assert conf > 0.9 and n_good == n


def test_motion_and_pulse_gate():
    red, ir = _burst()
    assert spo2.burst(red, ir, motion_mg=np.full(30, 40.0))[2] == 0
    assert spo2.burst(red, ir, pulse_ok=np.zeros(30, bool))[2] == 0


def test_low_perfusion_and_noise_gate():
    red, ir = _burst(pi_ir=5e-5)
    assert np.isnan(spo2.burst(red, ir)[0])
    red, ir = _burst(noise=2000.0, seed=3)
    _, conf, _, _ = spo2.burst(red, ir)
    assert conf < 0.3


def test_saturation_gate():
    red, ir = _burst(dc=520000.0)
    assert spo2.burst(red, ir)[2] == 0


def test_night_ratio_and_trend():
    assert spo2.night_ratio([0.5, 0.6, 2.0], [0.9, 0.9, 0.1]) in (0.5, 0.6)
    assert np.isnan(spo2.night_ratio([0.5], [0.1]))
    assert spo2.trend(0.55, [0.5] * 6) is None
    assert abs(spo2.trend(0.55, [0.5] * 10) - 0.1) < 1e-9


def test_no_number_without_a_validated_calibration():
    assert spo2.nightly_percent([0.5] * 20, [0.9] * 20, spo2.Calibration()) is None


def test_fit_and_nightly_percent():
    rng = np.random.default_rng(5)
    r = rng.uniform(0.4, 0.75, 200)
    ref = 104.0 - 18.0 * r + rng.normal(0, 1.0, 200)
    cal, err = spo2.fit_calibration(r, ref)
    assert abs(cal.a - 104.0) < 1.0 and abs(cal.b + 18.0) < 1.5 and err < 1.2
    cal.validated = True
    assert spo2.nightly_percent([0.5] * 20, [0.9] * 20, cal) == 95
    assert spo2.nightly_percent([0.5] * 5, [0.9] * 5, cal) is None          # under an hour of good bursts
    assert spo2.nightly_percent([0.95] * 20, [0.9] * 20, cal) == "lower than usual"   # outside the fitted range


def test_percent_capped_at_100():
    cal = spo2.Calibration(a=110.0, b=-10.0, r_min=0.3, r_max=0.9)
    assert float(spo2.percent(0.4, cal)) == 100.0
    assert np.isnan(spo2.percent(1.2, cal))
