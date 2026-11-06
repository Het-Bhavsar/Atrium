import numpy as np

from atrium_ml import wear


def _series(*parts):
    """parts: (seconds, dc, pulse present, motion, temp_start, temp_end)."""
    cols = [[], [], [], []]
    for n, dc, conf, mot, t0, t1 in parts:
        cols[0] += [dc] * n
        cols[1] += [conf] * n
        cols[2] += [mot] * n
        cols[3] += list(np.linspace(t0, t1, n))
    return [np.array(c) for c in cols]


def test_lift_off_and_back_on():
    dc, conf, mot, temp = _series((120, 1.0, 1, 5, 33, 33), (60, 0.02, 0, 30, 33, 31), (60, 1.0, 1, 5, 31, 32))
    w = wear.run(dc, conf, mot, temp)
    assert w[:120].all()
    off_at = 120 + np.argmax(~w[120:180])
    assert off_at - 120 <= 5
    on_at = 180 + np.argmax(w[180:])
    assert on_at - 180 <= 3


def test_table_under_lamp_goes_off():
    # lit by a lamp (DC high), still, no pulse, cooling
    dc, conf, mot, temp = _series((60, 1.0, 1, 5, 33, 33), (300, 0.6, 0, 2, 33, 26))
    w = wear.run(dc, conf, mot, temp)
    assert not w[-1]
    assert np.argmax(~w[60:]) <= 90


def test_pocket_goes_off():
    dc, conf, mot, temp = _series((60, 1.0, 1, 5, 33, 33), (600, 0.5, 0, 80, 33, 30))
    w = wear.run(dc, conf, mot, temp)
    assert not w[-1]


def test_exercise_stays_worn():
    # motion hides the pulse for 20 min, but the pod is lit and warm
    dc, conf, mot, temp = _series((60, 1.0, 1, 5, 33, 33), (1200, 1.0, 0, 250, 33, 34))
    assert wear.run(dc, conf, mot, temp).all()


def test_lit_without_pulse_does_not_wake():
    dc, conf, mot, temp = _series((10, 0.0, 0, 0, 25, 25), (120, 0.8, 0, 0, 25, 25))
    assert not wear.run(dc, conf, mot, temp, worn=False).any()


def test_pulse_present_needs_a_steady_peak():
    conf = np.full(20, 0.9)
    steady = np.full(20, 70.0)
    wander = np.where(np.arange(20) % 2, 60.0, 90.0)
    assert wear.pulse_present(conf, steady)[5:].all()
    assert not wear.pulse_present(conf, wander).any()
    assert not wear.pulse_present(np.full(20, 0.3), steady).any()


def test_step_rate_peak_is_not_a_pulse():
    conf = np.full(20, 0.9)
    peak = np.full(20, 110.0)
    assert not wear.pulse_present(conf, peak, np.full(20, 111.0)).any()
    assert wear.pulse_present(conf, peak, np.full(20, 60.0))[5:].all()
