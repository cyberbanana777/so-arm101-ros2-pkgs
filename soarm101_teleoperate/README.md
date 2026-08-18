# soarm101_teleoperate

Пакет для организации телеуправления роботизированной рукой **SOARM101**: ведомая рука (`follower`) повторяет движения ведущей (`leader`). Запускает обе руки, ретранслирует положения суставов и схвата, публикует статические трансформации и маркеры для RViz. Позволяет визуализировать одновременно движение ведущей и ведомой руки в RViz.

---

## Назначение

- Запуск двух рук SOARM101 одновременно: `leader` — только чтение, `follower` — управление.
- Ретрансляция положений суставов с leader на follower через `JointTrajectory`.
- Ретрансляция положения схвата leader на схват follower через action `GripperCommand`.
- Публикация статических трансформаций из `world_frame` в `leader/zero_point_link` и `follower/zero_point_link`.
- Публикация маркеров для визуальной идентификации рук в RViz.

---

## Архитектура и ключевые особенности

Пакет содержит четыре исполняемых узла и один launch-файл.

### Узлы

- **`arm_relay`** — подписывается на `/leader/soarm101_telemetry_controller/motor_states`, фильтрует игнорируемые суставы и публикует `JointTrajectory` в `/follower/joint_trajectory_controller/joint_trajectory`.
- **`gripper_relay`** — подписывается на ту же телеметрию leader, находит сустав `gripper_jaw_joint` и отправляет цель через action `/follower/gripper_controller/gripper_cmd`.
- **`marker_publisher`** — читает `config/dual_arm_setup.yaml` и публикует `MarkerArray` в `/markers` с текстовыми метками и «ромбами» для каждой руки.
- **`static_transform_publisher_world_to_arms`** — публикует статические трансформации из `world_frame` в `leader/zero_point_link` и `follower/zero_point_link` на основе YAML-конфигурации.

### Launch-файл

`teleoperate.launch.py`:
- дважды включает `soarm101_bringup` — для leader и follower;
- запускает узлы статических трансформаций, маркеров, `arm_relay`, `gripper_relay`;
- запускает RViz с конфигурацией `dual_arm.rviz`.

### Конфигурация

Файл `config/dual_arm_setup.yaml` содержит:
- позиции `zero_point_link` для каждой руки в мире;
- параметры маркеров (текст, цвет, высота, форма).

---

## Параметры конфигурации

### Launch-аргументы

| Параметр | Тип | По умолчанию | Описание |
|----------|-----|--------------|----------|
| `leader_port` | string | `/dev/soarm101_leader` | Последовательный порт leader |
| `follower_port` | string | `/dev/soarm101_follower` | Последовательный порт follower |


### Параметры узла `arm_relay`

| Параметр | Тип | По умолчанию | Описание |
|----------|-----|--------------|----------|
| `command_timeout` | double | `0.0015` | Время достижения целевой точки, сек |
| `publish_rate` | double | `50.0` | Максимальная частота публикации, Гц |
| `ignored_joints` | list of string | `['gripper_jaw_joint']` | Суставы, которые не ретранслируются |

### Параметры узла `gripper_relay`

| Параметр | Тип | По умолчанию | Описание |
|----------|-----|--------------|----------|
| `max_effort` | double | `10.0` | Максимальное усилие схвата |
| `joint_name` | string | `gripper_jaw_joint` | Имя сустава схвата |

### YAML-конфигурация `dual_arm_setup.yaml`

| Секция | Ключ | Тип | Описание |
|--------|------|-----|----------|
| `world.<arm>` | `translation.x/y/z` | float | Позиция руки в мире, м |
| `world.<arm>` | `rotation.roll/pitch/yaw` | float | Ориентация руки, рад |
| `markers.<arm>` | `text` | string | Текст маркера |
| `markers.<arm>` | `color` | [r,g,b,a] | Цвет маркера |
| `markers.<arm>` | `offset_z` | float | Высота маркера над `zero_point_link`, м |
| `markers.<arm>` | `shape` | string | Форма маркера (`diamond` или иное) |
| `markers.<arm>` | `diamond_size` | float | Базовый размер ромба |

---

## Использование

### Запуск телеуправления

```bash
source install/setup.bash
ros2 launch soarm101_teleoperate teleoperate.launch.py
```

С указанием портов:

```bash
ros2 launch soarm101_teleoperate teleoperate.launch.py \
  leader_port:=/dev/soarm101_leader \
  follower_port:=/dev/soarm101_follower
```

После запуска:
- RViz откроется с двумя моделями рук;
- leader-рука публикует телеметрию;
- `arm_relay` передаёт положения суставов на follower;
- `gripper_relay` передаёт положение схвата.

### Полезные проверки

```bash
# Телеметрия leader
ros2 topic echo /leader/soarm101_telemetry_controller/motor_states

# Топик команд follower
ros2 topic echo /follower/joint_trajectory_controller/joint_trajectory

# Маркеры
ros2 topic echo /markers
```

---

## Зависимости

- **ROS 2 Humble:**
  - `rclpy`
  - `soarm101_bringup`
  - `soarm101_interfaces`
  - `trajectory_msgs`
  - `control_msgs`
  - `visualization_msgs`
  - `geometry_msgs`
  - `tf2_ros`
  - `tf_transformations`
  - `std_msgs`
  - `rviz2`
- **Python:**
  - `PyYAML`

---

## Лицензия

Пакет распространяется под лицензией **MIT** (см. файл `LICENSE` в корне пакета).

---

## Версия

**1.0.0** — пакет выпущен.

---

## Поддержка

Вопросы и предложения оформляйте через [Issues](https://github.com/cyberbanana777/so-arm101-ros2-pkgs/issues).