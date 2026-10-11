# ik_solver

Точный (closed-form) IK для руки SO-101: цель (x, y, z) в `base_link` -> `JointTrajectory`.
Геометрия взята из `soarm101.xacro`, без численных решателей и без MoveIt.

## Сборка
    cd <ws> && colcon build --packages-select ik_solver --symlink-install
    source install/setup.bash

## Сухой прогон (рука не двигается, ROS-узлы не нужны)
    ros2 run ik_solver ik_check --xyz 0.02 -0.22 0.20 --pitch-deg 0

## Запуск узла
    ros2 launch ik_solver ik_solver.launch.py
    ros2 param set /ik_solver dry_run true      # только считать, не публиковать
    ros2 topic pub --once /ik_solver/target_position geometry_msgs/msg/Point "{x: 0.02, y: -0.22, z: 0.20}"

## Параметры (config/ik_solver.yaml)
`pitch_deg` (0 = схват вниз), `roll_deg`, `duration_sec`, `max_speed_rad_s`, `track_state`,
`dry_run`, `output_topic`, `joint_states_topic`, `joint_scale`, `joint_zero_offset_deg`.

`joint_scale` и `joint_zero_offset_deg` - поправка калибровки (по умолчанию её нет):
реальный угол = delta + r * команда. Значения берутся из тестов калибровки.

## Тесты
    python3 -m pytest test/      # из каталога пакета, ROS не нужен

Координаты: рука достаёт в сторону отрицательного y системы base_link.
