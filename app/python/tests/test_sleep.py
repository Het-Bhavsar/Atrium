import numpy as np

from atrium_ml import sleep


def _night(seed, n=900):
    """Synthetic night: still with a low pulse while asleep, moving with a higher pulse while awake."""
    rng = np.random.default_rng(seed)
    stage = np.ones(n, int)
    stage[:40] = 0
    stage[400:420] = 0
    stage[-30:] = 0
    act = np.where(stage == 0, 60.0, 4.0) * rng.lognormal(0, 0.5, n)
    hr = np.where(stage == 0, 72.0, 58.0) + rng.normal(0, 3, n)
    return act, hr, stage


def test_features_shape_and_hr_normalisation():
    act, hr, _ = _night(0)
    f = sleep.epoch_features(act, hr)
    assert f.shape == (900, 6)
    assert abs(np.median(f[:, 3])) < 0.05


def test_fit_separates_wake_and_sleep():
    nights = [_night(s) for s in range(4)]
    X = np.vstack([sleep.epoch_features(a, h) for a, h, _ in nights[:3]])
    y = np.concatenate([(s > 0).astype(int) for _, _, s in nights[:3]])
    model = sleep.fit(X, y)
    a, h, s = nights[3]
    p = sleep.predict(model, sleep.epoch_features(a, h))
    assert ((p == 1) == (s > 0)).mean() > 0.9
    assert sleep.kappa((s > 0).astype(int), p) > 0.6


def test_missing_pulse_is_filled():
    act, hr, _ = _night(1)
    hr[100:200] = np.nan
    assert np.isfinite(sleep.epoch_features(act, hr)).all()


def test_kappa_perfect_and_chance():
    y = np.array([0, 1, 1, 0, 1])
    assert sleep.kappa(y, y) == 1.0
    assert sleep.kappa(y, np.ones(5, int)) == 0.0


def test_threshold_matches_training_sleep_time():
    nights = [_night(s) for s in range(3)]
    X = np.vstack([sleep.epoch_features(a, h) for a, h, _ in nights])
    y = np.concatenate([(s > 0).astype(int) for _, _, s in nights])
    model = sleep.fit(X, y)
    assert abs(sleep.predict(model, X).mean() - y.mean()) < 0.01
