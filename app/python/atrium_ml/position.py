"""Sleep position from head orientation (FEATURES.md #5): back, left side, right side, front, upright.

Atrium method "skin-normal head frame":
 - The pod sits flat on the temple, so its skin normal (+z of the IMU, pointing out of the skin) is close to the
   head's lateral axis: it points right when worn on the right temple. Temple curvature and placement tilt it
   by an unknown 10-30 degrees [ASSUMED].
 - Upright calibration: the mean gravity direction while walking (the head is upright on average) gives the head's
   vertical axis in the IMU frame. The app keeps a running mean from walking minutes, so it needs no user step.
 - Head frame: up = calibrated vertical; right = skin normal (sign set by the wear side), made orthogonal to up;
   forward = up x right.
 - Each epoch: mean accelerometer over the epoch -> unit vector g. The head is lying when g . up < cos(`lie_deg`).
   The roll angle atan2(-(g . right), g . forward) then picks the sector: back (|roll| < 45 deg), left or right
   side, front. A new position must hold for `min_epochs` epochs (hysteresis against turning over).
On the pod this needs only the accelerometer at a low ODR (12.5 Hz is plenty) and the mean over each log record.
The pod logs that mean as pitch and roll (docs/ble_gatt.md section 7); `gravity_from_pitch_roll` turns them back
into the vector the app classifies.
"""
from dataclasses import dataclass

import numpy as np

LABELS = ("upright", "back", "left", "right", "front")


@dataclass
class PositionConfig:
    lie_deg: float = 50.0        # head axis more than 50 deg from vertical = lying
    min_epochs: int = 2


def head_frame(up_imu, normal_imu=(0.0, 0.0, 1.0), side="right"):
    """Rows: right, forward, up, in IMU coordinates. side: which temple the pod is on."""
    up = np.asarray(up_imu, float)
    up = up / np.linalg.norm(up)
    n = np.asarray(normal_imu, float)
    right = n if side == "right" else -n
    right = right - np.dot(right, up) * up
    right = right / np.linalg.norm(right)
    fwd = np.cross(up, right)
    return np.vstack([right, fwd, up])


def classify_vector(g_imu, frame, cfg=PositionConfig()):
    """g_imu: mean accelerometer (any units). Gravity reads +1 g along 'up' when the head is upright."""
    g = np.asarray(g_imu, float)
    g = g / (np.linalg.norm(g) + 1e-12)
    r, f, u = frame @ g
    if u > np.cos(np.radians(cfg.lie_deg)):
        return "upright"
    # The accelerometer reads +1 g towards world up. Face up, world up is the head's forward axis (f = +1);
    # right side down, it is the head's left (r = -1).
    roll = np.degrees(np.arctan2(-r, f))   # 0 back, +90 right side down, -90 left side down, 180 front
    if abs(roll) <= 45:
        return "back"
    if abs(roll) >= 135:
        return "front"
    return "right" if roll > 0 else "left"


def classify_epochs(g_epochs, frame, cfg=PositionConfig()):
    """Per-epoch labels with hysteresis."""
    raw = [classify_vector(g, frame, cfg) for g in g_epochs]
    out, cur, cand, run = [], None, None, 0
    for lab in raw:
        if cur is None:
            cur = lab
        elif lab != cur:
            run = run + 1 if lab == cand else 1
            cand = lab
            if run >= cfg.min_epochs:
                cur, run = lab, 0
        else:
            run = 0
        out.append(cur)
    return out


def upright_from_walking(acc_g, motion_mg, walk_mg=(40.0, 400.0)):
    """Mean gravity direction over samples whose motion is in the walking band."""
    acc = np.asarray(acc_g, float)
    m = (np.asarray(motion_mg) >= walk_mg[0]) & (np.asarray(motion_mg) <= walk_mg[1])
    v = acc[m].mean(0) if m.any() else acc.mean(0)
    return v / np.linalg.norm(v)


def gravity_from_pitch_roll(pitch_deg, roll_deg):
    """Unit gravity vector in the IMU frame from the logged angles: pitch = atan2(-x, sqrt(y^2 + z^2)),
    roll = atan2(y, z), as in docs/ble_gatt.md section 7."""
    p, r = np.radians(pitch_deg), np.radians(roll_deg)
    return np.array([-np.sin(p), np.cos(p) * np.sin(r), np.cos(p) * np.cos(r)])
