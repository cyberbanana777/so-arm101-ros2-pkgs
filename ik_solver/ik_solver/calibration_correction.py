"""
Поправка калибровки (тики <-> радианы) на каждый сустав.

Реальный угол при команде c (рад):      реальный = delta + r * c
Чтобы получить нужный реальный угол q:  c = (q - delta) / r

r (joint_scale) и delta (joint_zero_offset_deg) получаются из тестов
(см. TESTS_calibration_instructions.txt). По умолчанию r = 1, delta = 0,
то есть поправки нет и узел работает "как есть".

Порядок суставов везде: pan, lift, elbow, wrist_flex, wrist_roll.
"""


def true_to_command(q_true, scale, offset_rad):
    """Нужные реальные углы -> то, что надо подать в контроллер."""
    return [(q - d) / r for q, r, d in zip(q_true, scale, offset_rad)]


def state_to_true(c_state, scale, offset_rad):
    """Показания /joint_states (проходят через ту же линейку) -> реальные углы."""
    return [r * c + d for c, r, d in zip(c_state, scale, offset_rad)]
