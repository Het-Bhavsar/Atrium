import numpy as np
import pytest

from atrium_ml import position, synth

# world "up" expressed in head axes (right, forward, up) for each true label
HEAD_UP = {"upright": (0, 0, 1), "back": (0, 1, 0), "front": (0, -1, 0), "right": (-1, 0, 0), "left": (1, 0, 0)}


def _mount(tilt_deg, twist_deg):
    """Head-to-IMU rotation: the IMU z sits on the head's right axis, tilted and twisted by the placement."""
    base = np.array([[0, 1, 0], [0, 0, 1], [1, 0, 0]], float)  # rows: IMU x, y, z in head axes
    return synth.rotation((0, 0, 1), twist_deg) @ synth.rotation((1, 0, 0), tilt_deg) @ base


@pytest.mark.parametrize("tilt,twist", [(0, 0), (20, 15), (-25, 40), (30, -60)])
def test_all_positions_with_placement_error(tilt, twist):
    m = _mount(tilt, twist)
    up_imu = m @ np.array([0, 0, 1.0])
    frame = position.head_frame(up_imu, side="right")
    for lab, v in HEAD_UP.items():
        assert position.classify_vector(m @ np.array(v, float), frame) == lab


def test_upright_from_walking():
    m = _mount(15, 30)
    up_imu = m @ np.array([0, 0, 1.0])
    acc = synth.walking_acc(25.0, 60, g_vec=up_imu)
    est = position.upright_from_walking(acc, np.full(len(acc), 100.0))
    assert np.degrees(np.arccos(np.dot(est, up_imu))) < 3.0


def test_left_temple_flips_sides():
    m = _mount(0, 0)
    m_left = np.diag([1, 1, -1]) @ m  # mirror: skin normal now points to the head's left
    frame = position.head_frame(m_left @ np.array([0, 0, 1.0]), side="left")
    assert position.classify_vector(m_left @ np.array([-1.0, 0, 0]), frame) == "right"


def test_hysteresis():
    frame = np.eye(3)
    g = [(0, 1, 0)] * 3 + [(-1, 0, 0)] + [(0, 1, 0)] * 2 + [(-1, 0, 0)] * 3
    out = position.classify_epochs(g, frame)
    assert out[:6] == ["back"] * 6
    assert out[-1] == "right"


def test_pitch_roll_round_trip():
    rng = np.random.default_rng(3)
    for _ in range(50):
        g = rng.normal(size=3)
        g /= np.linalg.norm(g)
        pitch = np.degrees(np.arctan2(-g[0], np.hypot(g[1], g[2])))
        roll = np.degrees(np.arctan2(g[1], g[2]))
        assert np.allclose(position.gravity_from_pitch_roll(pitch, roll), g, atol=1e-9)


def test_logged_angle_steps_keep_the_position():
    # the log stores pitch in 1 deg steps and roll in 2 deg steps
    m = _mount(20, 15)
    frame = position.head_frame(m @ np.array([0, 0, 1.0]), side="right")
    for lab, v in HEAD_UP.items():
        g = m @ np.array(v, float)
        pitch = np.round(np.degrees(np.arctan2(-g[0], np.hypot(g[1], g[2]))))
        roll = 2 * np.round(np.degrees(np.arctan2(g[1], g[2])) / 2)
        assert position.classify_vector(position.gravity_from_pitch_roll(pitch, roll), frame) == lab
