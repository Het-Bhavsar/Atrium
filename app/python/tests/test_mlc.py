import numpy as np
import pytest

from atrium_ml import mlc


def _acc(seconds, amp_g=0.0, hz=1.7, seed=0):
    rng = np.random.default_rng(seed)
    t = np.arange(int(seconds * mlc.FS)) / mlc.FS
    a = np.tile([0.0, 0.0, 1.0], (len(t), 1))
    a[:, 2] += amp_g * np.sin(2 * np.pi * hz * t)
    return a + 0.001 * rng.standard_normal(a.shape)


def test_st_tree_decodes_to_a_ladder():
    t = mlc.decode_thresholds()
    assert len(t) == 7 and np.all(np.diff(t) > 0)
    assert t[0] == pytest.approx(0.009, rel=0.01) and t[-1] == pytest.approx(0.3125)


def test_atrium_config_changes_only_thresholds_and_routing():
    cfg = mlc.atrium_config()
    assert len(cfg) == len(mlc.ST_MOTION_INTENSITY)
    assert mlc.decode_thresholds(cfg) == pytest.approx(mlc.ATRIUM_THRESHOLDS, rel=1e-3)
    changed = [(a, b) for a, b in zip(mlc.ST_MOTION_INTENSITY, cfg) if a != b]
    thr = [c for c in changed if c[0][0] == 0x09]
    route = [c for c in changed if c[0][0] != 0x09]
    assert len(thr) <= 14
    assert sorted(route) == [((0x0D, 0x01), (0x11, 0x01)), ((0x5E, 0x02), (0x5F, 0x02))]
    assert mlc.atrium_config(int_pin=1)[-12:] == mlc.ST_MOTION_INTENSITY[-12:]


def test_config_keeps_12_5_hz_2g():
    assert mlc.atrium_config()[-2:] == ((0x10, 0x10), (0x11, 0x00))


def test_bad_thresholds_rejected():
    with pytest.raises(ValueError):
        mlc.atrium_config((1, 2, 3))
    with pytest.raises(ValueError):
        mlc.atrium_config((0.1, 0.05, 0.2, 0.3, 0.4, 0.5, 0.6))


@pytest.mark.parametrize("amp,level", [(0.0, 0), (0.02, 1), (0.16, 4), (0.5, 6), (1.0, 7)])
def test_levels_follow_variance(amp, level):
    lv, var = mlc.levels(_acc(31.2, amp))
    # a sine of amplitude A has variance A^2 / 2
    assert np.all(lv == level), (lv, var)


def test_levels_need_12_5_hz():
    with pytest.raises(ValueError):
        mlc.levels(_acc(10), fs=26.0)


def test_classes():
    assert mlc.activity_class([0, 0, 0], np.nan, 0.0) == "still"
    assert mlc.activity_class([4, 4, 5], 105.0, 0.8) == "walk"
    assert mlc.activity_class([6, 7, 6], 165.0, 0.8) == "run"
    assert mlc.activity_class([3, 3, 3], 105.0, 0.8) == "other"   # level 3 alone is head motion
    assert mlc.activity_class([4, 4, 4], 105.0, 0.5) == "other"   # no clear step rhythm


def test_step_rhythm_finds_cadence():
    spm, share = mlc.step_rhythm(_acc(8, 0.2, hz=1.75), mlc.FS)
    assert abs(spm - 105.0) < 4 and share > 0.9
