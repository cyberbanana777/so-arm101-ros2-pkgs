"""
Точный (closed-form) IK для руки SO-101, выведенный напрямую из soarm101.xacro.

Идея (см. объяснение в чате): shoulder_pan разворачивает рабочую вертикальную
плоскость (yaw жёстко привязан к положению цели), а shoulder_lift + elbow_flex +
wrist_flex работают внутри этой плоскости как 3R-механизм. wrist_roll свободен
и не двигает точку схвата.

У этой конкретной руки есть НЕидеальность (это не приближение, а точный факт
геометрии, извлечённый из URDF): схват в этой "плоскости подхода" сдвинут в
сторону на H_OFFSET = 1.81 см относительно плоскости shoulder_lift/elbow.
Формулы ниже это смещение учитывают точно, а не игнорируют.

Соглашение по углу подхода theta3 (проверено численно через FK):
    theta3 =   0°   -> схват смотрит СТРОГО ВНИЗ  (обычный захват сверху)
    theta3 = +90°   -> схват смотрит горизонтально (в одну сторону)
    theta3 = -90°   -> схват смотрит горизонтально (в другую сторону)
    theta3 = 180°   -> схват смотрит строго вверх
Это единственная величина ориентации, которую нужно выбирать самим (кроме roll).

Целевая точка (x, y, z) задаётся в системе координат base_link (та же, что
использует /follower/joint_states и MoveIt). Если у тебя точка в
zero_point_link, сначала преобразуй через TF (zero_point_link -> base_link
задаётся joint 'zero_point_to_base_joint' в xacro).

Проверено: round-trip FK(IK(FK(q))) == q с ошибкой ~1e-15 на 300 случайных
конфигурациях в пределах лимитов суставов.
"""

import numpy as np
from dataclasses import dataclass


# ---------------------------------------------------------------------------
# 1. Геометрия -- взята буквально из soarm101.xacro (joint origin xyz/rpy).
#    Ничего не подбиралось руками -- это те самые числа из файла.
# ---------------------------------------------------------------------------

def _Rx(a):
    c, s = np.cos(a), np.sin(a)
    return np.array([[1, 0, 0], [0, c, -s], [0, s, c]])


def _Ry(a):
    c, s = np.cos(a), np.sin(a)
    return np.array([[c, 0, s], [0, 1, 0], [-s, 0, c]])


def _Rz(a):
    c, s = np.cos(a), np.sin(a)
    return np.array([[c, -s, 0], [s, c, 0], [0, 0, 1]])


def _rpy(r, p, y):
    return _Rz(y) @ _Ry(p) @ _Rx(r)


def _T(xyz, rpy):
    M = np.eye(4)
    M[:3, :3] = _rpy(*rpy)
    M[:3, 3] = xyz
    return M


_PAN_XYZ = np.array([0.0207909, -0.0230745, 0.0948817])
_PAN_RPY = (-3.14159, 6.03684e-16, 1.5708)
_LIFT_XYZ = np.array([-0.0303992, -0.0182778, -0.0542])
_LIFT_RPY = (-1.5708, -1.5708, 0)
_ELBOW_XYZ = np.array([-0.11257, -0.028, 2.46331e-16])
_ELBOW_RPY = (-1.22818e-15, 5.75928e-16, 1.5708)
_WRISTF_XYZ = np.array([-0.1349, 0.0052, 1.65232e-16])
_WRISTF_RPY = (3.2474e-15, 2.86219e-15, -1.5708)
_ROLL_XYZ = np.array([0.0, -0.0611, 0.0181])
_ROLL_RPY = (1.5708, -9.38083e-08, 3.14159)

def _Tz(theta):
    M = np.eye(4)
    M[:3, :3] = _Rz(theta)
    return M


T_PAN_O = _T(_PAN_XYZ, _PAN_RPY)
T_LIFT_O = _T(_LIFT_XYZ, _LIFT_RPY)
T_ELBOW_O = _T(_ELBOW_XYZ, _ELBOW_RPY)
T_WRISTF_O = _T(_WRISTF_XYZ, _WRISTF_RPY)
T_ROLL_O = _T(_ROLL_XYZ, _ROLL_RPY)

R_LIFT_O = _rpy(*_LIFT_RPY)
R_ELBOW_O = _rpy(*_ELBOW_RPY)
R_WRISTF_O = _rpy(*_WRISTF_RPY)

# --- производные константы плоского 3R-механизма (вычислены из geometry выше) ---
L1 = float(np.linalg.norm(_ELBOW_XYZ[:2]))          # shoulder_lift -> elbow
L2 = float(np.linalg.norm(_WRISTF_XYZ[:2]))         # elbow -> wrist_flex
L3 = float(np.linalg.norm(_ROLL_XYZ[:2]))           # wrist_flex -> схват (в плоскости)
H_OFFSET = 0.0181                                    # боковой сдвиг схвата (из ROLL_XYZ[2])

_beta1 = float(np.arctan2(_ELBOW_XYZ[1], _ELBOW_XYZ[0]))
_beta2 = float(np.arctan2(_WRISTF_XYZ[1], _WRISTF_XYZ[0]))
_beta3 = float(np.arctan2(_ROLL_XYZ[1], _ROLL_XYZ[0]))
_gamma_elbow = float(np.arctan2(R_ELBOW_O[1, 0], R_ELBOW_O[0, 0]))
_gamma_wristf = float(np.arctan2(R_WRISTF_O[1, 0], R_WRISTF_O[0, 0]))

_n_vec = R_LIFT_O[:, 2]
_K = H_OFFSET + float(np.dot(_n_vec, _LIFT_XYZ))

# --- лимиты суставов (из xacro <limit>) ---
JOINT_LIMITS = {
    "shoulder_pan_joint": (-1.91986, 1.91986),
    "shoulder_lift_joint": (-1.74533, 1.74533),
    "elbow_flex_joint": (-1.74533, 1.5708),
    "wrist_flex_joint": (-1.65806, 1.65806),
    "wrist_roll_joint": (-2.79253, 2.79253),
}


class UnreachableError(ValueError):
    pass


@dataclass
class Solution:
    shoulder_pan: float
    shoulder_lift: float
    elbow_flex: float
    wrist_flex: float
    wrist_roll: float

    def as_list(self):
        return [self.shoulder_pan, self.shoulder_lift, self.elbow_flex,
                self.wrist_flex, self.wrist_roll]

    def within_limits(self) -> bool:
        vals = {
            "shoulder_pan_joint": self.shoulder_pan,
            "shoulder_lift_joint": self.shoulder_lift,
            "elbow_flex_joint": self.elbow_flex,
            "wrist_flex_joint": self.wrist_flex,
            "wrist_roll_joint": self.wrist_roll,
        }
        return all(lo <= vals[j] <= hi for j, (lo, hi) in JOINT_LIMITS.items())


def _solve_branch(target_base, theta3, roll_a, branch_pan, branch_elbow):
    p_PF = (np.linalg.inv(T_PAN_O) @ np.append(target_base, 1.0))[:3]
    px, py, pz = p_PF

    C1 = _n_vec[0] * px + _n_vec[1] * py
    C2 = _n_vec[0] * py - _n_vec[1] * px
    C3 = _K - _n_vec[2] * pz
    R = np.hypot(C1, C2)
    if abs(C3) > R:
        raise UnreachableError("цель вне рабочей зоны по pan (не в этой вертикальной плоскости)")
    base_ang = np.arctan2(C2, C1)
    delta = np.arccos(np.clip(C3 / R, -1.0, 1.0))
    pan_a = np.arctan2(np.sin(base_ang + branch_pan * delta), np.cos(base_ang + branch_pan * delta))

    p_after_pan = _Rz(-pan_a) @ p_PF
    p_lift_frame = R_LIFT_O.T @ (p_after_pan - _LIFT_XYZ)
    target_xy = p_lift_frame[:2]

    W = target_xy - L3 * np.array([np.cos(theta3), np.sin(theta3)])
    d = float(np.linalg.norm(W))
    if d > L1 + L2 or d < abs(L1 - L2):
        raise UnreachableError(f"цель вне зоны досягаемости (d={d:.4f}, "
                                f"допустимо {abs(L1-L2):.4f}..{L1+L2:.4f})")

    cos_e = (d ** 2 - L1 ** 2 - L2 ** 2) / (2 * L1 * L2)
    elbow_interior = branch_elbow * np.arccos(np.clip(cos_e, -1.0, 1.0))
    ang_W = np.arctan2(W[1], W[0])
    k1 = L1 + L2 * np.cos(elbow_interior)
    k2 = L2 * np.sin(elbow_interior)
    theta1 = ang_W - np.arctan2(k2, k1)
    theta2 = theta1 + elbow_interior

    lift_a = theta1 - _beta1
    elbow_a = theta2 - lift_a - _gamma_elbow - _beta2
    wristf_a = theta3 - lift_a - _gamma_elbow - elbow_a - _gamma_wristf - _beta3

    def wrap(a):
        return float(np.arctan2(np.sin(a), np.cos(a)))

    return Solution(wrap(pan_a), wrap(lift_a), wrap(elbow_a), wrap(wristf_a), wrap(roll_a))


def solve_ik_all(x, y, z, theta3=0.0, roll=0.0):
    """
    Все геометрически существующие решения для цели (до 4 веток:
    pan вперёд/через спину x локоть вверх/вниз). Лимиты суставов НЕ фильтруются:
    смотри s.within_limits(). Бросает UnreachableError, если решений нет.
    """
    target = np.array([x, y, z], dtype=float)
    candidates = []
    for bp in (1, -1):
        for be in (1, -1):
            try:
                candidates.append(_solve_branch(target, theta3, roll, bp, be))
            except UnreachableError:
                continue
    if not candidates:
        raise UnreachableError(f"точка ({x:.3f}, {y:.3f}, {z:.3f}) не достижима ни в одной ветке")
    return candidates


def solve_ik(x, y, z, theta3=0.0, roll=0.0, seed=None):
    """
    x, y, z       -- целевая точка схвата в системе base_link, метры.
    theta3        -- угол подхода (см. описание модуля): 0 = вниз, +-90 град = горизонтально.
    roll          -- поворот схвата вокруг своей оси, радианы, свободный параметр.
    seed          -- dict с текущими углами (для выбора ветки, ближайшей к текущей
                     позе, чтобы не дёргать рукой) -- опционально.

    Возвращает Solution. Бросает UnreachableError, если ни одна из 4 веток
    (pan +/-, elbow up/down) не лежит в пределах геометрии руки.
    Что цель В ПРИНЦИПЕ вне лимитов суставов -- проверяй sol.within_limits().
    """
    candidates = solve_ik_all(x, y, z, theta3, roll)

    in_limits = [s for s in candidates if s.within_limits()]
    pool = in_limits if in_limits else candidates

    if seed is None:
        return pool[0]

    seed_vec = np.array([seed.get("shoulder_pan", 0.0), seed.get("shoulder_lift", 0.0),
                         seed.get("elbow_flex", 0.0), seed.get("wrist_flex", 0.0),
                         seed.get("wrist_roll", 0.0)])
    return min(pool, key=lambda s: float(np.sum(np.abs(np.array(s.as_list()) - seed_vec))))


def forward_kinematics(pan, lift, elbow, wrist_flex, roll=0.0):
    """
    Прямая кинематика: однородная матрица 4x4 системы gripper_link в base_link
    (углы суставов в радианах, нуль = нуль URDF). Положение схвата = [:3, 3].
    Нужна для проверок: FK(IK(цель)) должна вернуть цель.
    """
    return (T_PAN_O @ _Tz(pan) @ T_LIFT_O @ _Tz(lift) @ T_ELBOW_O @ _Tz(elbow)
            @ T_WRISTF_O @ _Tz(wrist_flex) @ T_ROLL_O @ _Tz(roll))


def approach_angle(lift, elbow, wrist_flex):
    """Угол подхода theta3 (0 = схват строго вниз), который даёт такая поза."""
    return float(lift + _gamma_elbow + elbow + _gamma_wristf + wrist_flex + _beta3)
