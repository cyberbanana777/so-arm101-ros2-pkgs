# soarm101_moveit_config

Пакет содержит конфигурацию MoveIt2 для робота SOARM101. Предоставляет файлы настроек, launch-скрипты и визуализацию для планирования траекторий движения.

> **Статус:** в активной разработке. Некоторые параметры (кинематический решатель, плагины управления) могут быть заменены в будущих версиях.

---

## Состав пакета

- **`config/`** – основные конфигурационные файлы:
  - `joint_limits.yaml` – ограничения скоростей и ускорений.
  - `kinematics.yaml` – настройка кинематического решателя (сейчас используется `prbt_manipulator/IKFastKinematicsPlugin`, но планируется переход на стандартный KDL или собственный решатель).
  - `moveit_controllers.yaml` – связь MoveIt с контроллерами (`joint_trajectory_controller` и `gripper_controller`).
  - `ros2_controllers.yaml` – конфигурация для `ros2_control` (используется в симуляции).
  - `so101.urdf.srdf` – семантическое описание робота (группы, состояния, отключенные коллизии).
  - `so101.urdf.ros2_control.xacro` и `so101.urdf.urdf.xacro` – xacro-обёртки для подключения `ros2_control` к URDF (в том числе mock-плагин для тестирования).
  - `initial_positions.yaml` – начальные позиции суставов.
  - `moveit.rviz` – конфигурация RViz для MoveIt.
  - `pilz_cartesian_limits.yaml` – ограничения для планера Pilz (если используется).

- **`launch/`** – launch-файлы, сгенерированные через `moveit_configs_utils`:
  - `demo.launch.py` – запуск MoveIt в демонстрационном режиме.
  - `move_group.launch.py` – запуск узла `move_group`.
  - `moveit_rviz.launch.py` – запуск RViz с предзагруженной конфигурацией MoveIt.
  - `rsp.launch.py` – запуск `robot_state_publisher`.
  - `spawn_controllers.launch.py` – загрузка контроллеров через `controller_manager`.
  - `setup_assistant.launch.py` – запуск Setup Assistant для редактирования конфигурации.
  - и другие вспомогательные launch-файлы.

---

## Использование

1. **Запуск MoveIt с RViz** (для визуализации и тестирования планирования):
   ```bash
   ros2 launch soarm101_moveit_config demo.launch.py
   ```

2. **Запуск только `move_group`** (без RViz):
   ```bash
   ros2 launch soarm101_moveit_config move_group.launch.py
   ```

3. **Запуск RViz с конфигурацией MoveIt**:
   ```bash
   ros2 launch soarm101_moveit_config moveit_rviz.launch.py
   ```

4. **Загрузка контроллеров** (при использовании с реальным роботом или симуляцией):
   ```bash
   ros2 launch soarm101_moveit_config spawn_controllers.launch.py
   ```

---

## Важные замечания

- **Кинематический решатель:** в файле `kinematics.yaml` указан `prbt_manipulator/IKFastKinematicsPlugin`. Этот плагин взят из конфигурации другого робота и, скорее всего, не будет работать корректно. Планируется замена на стандартный `kdl_kinematics_plugin/KDLKinematicsPlugin` или другой подходящий решатель.

- **Контроллеры:** в `ros2_controllers.yaml` и `moveit_controllers.yaml` заданы имена контроллеров `arm_controller` и `gripper_controller`, которые должны соответствовать контроллерам, загруженным в `controller_manager` (например, из пакета `soarm101_ros2_control`).

- **Симуляция:** пакет использует mock-плагин `mock_components/GenericSystem` для тестирования без реального оборудования. Для работы с реальным роботом необходимо заменить его на `soarm101_hardware/SOARM101SystemHardware` (как описано в `soarm101_ros2_control`).

- **Активная разработка:** файлы конфигурации и launch-скрипты могут меняться. Следите за обновлениями в репозитории.

---

## Зависимости

- `moveit_ros_move_group`, `moveit_kinematics`, `moveit_planners`, `moveit_simple_controller_manager` – основные компоненты MoveIt2.
- `moveit_configs_utils` – утилиты для генерации launch-файлов.
- `soarm101_description` – URDF-описание робота.
- `controller_manager`, `joint_trajectory_controller`, `position_controllers` – для управления через ros2_control.
- `rviz2`, `rviz_common`, `rviz_default_plugins` – для визуализации.
- `xacro`, `tf2_ros` – для обработки URDF и трансформаций.

---

## Лицензия

Пакет распространяется под лицензией **BSD** (как и стандартные конфигурации MoveIt).

---

## Поддержка

Вопросы и предложения оформляйте через [Issues](https://github.com/cyberbanana777/so-arm101-ros2-pkgs/issues).