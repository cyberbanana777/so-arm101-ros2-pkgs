import random

import numpy as np
import pytest

from ik_solver.calibration_correction import state_to_true, true_to_command
from ik_solver.so101_ik import (
    approach_angle, forward_kinematics, JOINT_LIMITS, solve_ik, UnreachableError)

SEED_KEYS = ['shoulder_pan', 'shoulder_lift', 'elbow_flex', 'wrist_flex', 'wrist_roll']


def _random_config(rng):
    return [rng.uniform(0.95 * lo, 0.95 * hi) for lo, hi in JOINT_LIMITS.values()]


def test_zero_pose_position():
    pos = forward_kinematics(0, 0, 0, 0, 0)[:3, 3]
    assert np.allclose(pos, [0.020615, -0.277473, 0.266852], atol=1e-5)


def test_roundtrip_ik_fk():
    rng = random.Random(42)
    for _ in range(300):
        q = _random_config(rng)
        target = forward_kinematics(*q)[:3, 3]
        theta3 = approach_angle(q[1], q[2], q[3])
        sol = solve_ik(*target, theta3=theta3, roll=q[4], seed=dict(zip(SEED_KEYS, q)))
        back = forward_kinematics(*sol.as_list())[:3, 3]
        assert np.linalg.norm(back - target) < 1e-8


def test_unreachable_raises():
    with pytest.raises(UnreachableError):
        solve_ik(1.0, -1.0, 1.0)


def test_correction_roundtrip():
    scale = [1.089, 1.048, 1.025, 1.095, 1.0]
    offset = [0.03, -0.02, 0.01, 0.0, 0.0]
    q = [0.3, -0.2, 0.5, 1.0, 0.0]
    back = state_to_true(true_to_command(q, scale, offset), scale, offset)
    assert np.allclose(back, q)
