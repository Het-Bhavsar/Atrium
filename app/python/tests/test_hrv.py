import numpy as np
import pytest

from atrium_ml import hrv, synth


@pytest.mark.parametrize("fs", [25.0, 50.0, 64.0])
def test_rmssd_close_to_truth(fs):
    beats = synth.beat_times(300, 60.0, 45.0, seed=4)
    true = np.sqrt(np.mean(np.diff(np.diff(beats) * 1000) ** 2))
    ppg = synth.ppg_from_beats(beats, fs, 300, noise=0.01, seed=5)
    det = hrv.detect_beats(ppg, fs)
    _, rr, ok = hrv.clean_intervals(det)
    est, n = hrv.rmssd(rr, ok, 50)
    assert n > 200
    assert abs(est - true) < 0.15 * true


def test_interpolation_needed_at_25_hz():
    beats = synth.beat_times(300, 60.0, 30.0, seed=6)
    true = np.sqrt(np.mean(np.diff(np.diff(beats) * 1000) ** 2))
    ppg = synth.ppg_from_beats(beats, 25.0, 300, noise=0.005, seed=7)
    err = {}
    for m in ("none", "parabolic", "cubic"):
        det = hrv.detect_beats(ppg, 25.0, hrv.HrvConfig(interp=m))
        _, rr, ok = hrv.clean_intervals(det)
        err[m] = abs(hrv.rmssd(rr, ok, 50)[0] - true)
    assert err["none"] > 0.2 * true          # 40 ms timing steps inflate RMSSD
    assert err["parabolic"] < 0.1 * true
    assert err["cubic"] < 0.1 * true


def test_missed_beat_is_rejected():
    beats = np.cumsum(np.full(40, 1.0))
    beats = np.delete(beats, 20)  # one missed beat: a 2 s interval
    _, rr, ok = hrv.clean_intervals(beats)
    assert not ok[19]
    assert ok.sum() == len(ok) - 1
    r, n = hrv.rmssd(rr, ok, 5)
    assert r == pytest.approx(0.0, abs=1e-9)


def test_pooled_matches_gatt_rule():
    assert hrv.pooled([(30.0, 10), (40.0, 10)]) == pytest.approx(np.sqrt((900 + 1600) / 2))
    assert hrv.pooled([(None, 3)]) is None
