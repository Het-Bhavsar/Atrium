import numpy as np

from atrium_ml import hr, synth

FS = 25.0


def _walk_case(seconds=90, hr_bpm=72.0, cadence=1.8, artifact=0.5):
    beats = synth.beat_times(seconds, hr_bpm, 20.0, seed=1)
    ppg = synth.ppg_from_beats(beats, FS, seconds, noise=0.05, seed=2)
    acc = synth.walking_acc(FS, seconds, cadence_hz=cadence, seed=3)
    ppg = ppg + artifact * (acc[:, 2] - 1.0) / 0.15  # step artifact stronger than the pulse fundamental
    return ppg, acc


def test_rest_matches_truth():
    beats = synth.beat_times(60, 64.0, 30.0)
    ppg = synth.ppg_from_beats(beats, FS, 60, noise=0.05)
    r = hr.spectral_hr(ppg, None)
    assert np.nanmedian(np.abs(r.bpm[10:] - 64.0)) < 2.0
    assert r.valid[10:].mean() > 0.9


def test_motion_weighting_rejects_cadence():
    ppg, acc = _walk_case()
    with_imu = hr.spectral_hr(ppg, acc)
    no_imu = hr.spectral_hr(ppg, acc, motion=False)
    err_imu = np.nanmedian(np.abs(with_imu.bpm[20:] - 72.0))
    err_raw = np.nanmedian(np.abs(no_imu.bpm[20:] - 72.0))
    assert err_imu < 4.0
    assert err_raw > 20.0  # without the IMU it locks onto the 108 /min step rate


def test_autocorr_port_at_rest():
    beats = synth.beat_times(60, 75.0, 20.0)
    ppg = synth.ppg_from_beats(beats, FS, 60, noise=0.02)
    r = hr.autocorr_hr(ppg, FS)
    assert np.nanmedian(np.abs(r.bpm[5:] - 75.0)) < 3.0


def test_reference_hr():
    r = np.arange(0, 30, 0.8)  # 75 bpm
    ref = hr.reference_hr(r, np.array([8.0, 16.0, 24.0]))
    assert np.allclose(ref, 75.0)
