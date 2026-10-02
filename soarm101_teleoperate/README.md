# soarm101_teleoperate

Пакет для организации телеуправления роботизированной рукой **SOARM101**: ведомая рука (`follower`) повторяет движения ведущей (`leader`). Запускает обе руки, ретранслирует положения суставов и схвата, публикует статические трансформации и маркеры для RViz. Позволяет визуализировать одновременно движение ведущей и ведомой руки в RViz.

---

## Назначение

- Запуск двух рук SOARM101 одновременно: `leader` — только чтение, `follower` — управление.
- Ретрансляция положений суставов с leader на follower через `JointTrajectory`.
- Ретрансляция положения схвата leader на схват follower через action `GripperCommand`.
- Публикация статических трансформаций из `world_frame` в `leader/zero_point_link` и `follower/zero_point_link`.
- Публикация маркеров для визуальной идентификации рук в RViz.
- Пауза и возобновление ретрансляции по сервису, чтобы другие узлы (например, проигрыватель движений) могли управлять follower без конфликта с телеуправлением.

---

## Архитектура и ключевые особенности

Пакет содержит четыре исполняемых узла и один launch-файл.

### Узлы

- **`arm_relay`** — подписывается на `/leader/soarm101_telemetry_controller/motor_states`, фильтрует игнорируемые суставы и публикует `JointTrajectory` в `/follower/joint_trajectory_controller/joint_trajectory`. Пересылает каждое сообщение телеметрии; траектория начинает выполняться в момент получения (нулевой `header.stamp`). Поддерживает паузу через сервис `~/set_enabled`.
- **`gripper_relay`** — подписывается на ту же телеметрию leader, находит сустав `gripper_jaw_joint` и отправляет цель через action `/follower/gripper_controller/gripper_cmd`. Поддерживает паузу через сервис `~/set_enabled`.
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
| `command_timeout` | double | `0.03` | Время достижения целевой точки, сек (≈ 1–3 периода телеметрии) |
| `publish_rate` | double | `0.0` | Максимальная частота публикации, Гц (`0` — пересылать каждое сообщение) |
| `ignored_joints` | list of string | `['gripper_jaw_joint']` | Суставы, которые не ретранслируются |
| `enabled` | bool | `true` | Включена ли ретрансляция при старте |
| `resume_time` | double | `2.0` | Время плавного подъезда follower к leader после снятия паузы, сек |

### Параметры узла `gripper_relay`

| Параметр | Тип | По умолчанию | Описание |
|----------|-----|--------------|----------|
| `max_effort` | double | `10.0` | Максимальное усилие схвата |
| `joint_name` | string | `gripper_jaw_joint` | Имя сустава схвата |
| `enabled` | bool | `true` | Включена ли ретрансляция при старте |

### Сервисы

| Сервис | Тип | Описание |
|--------|-----|----------|
| `/arm_relay/set_enabled` | `std_srvs/SetBool` | `true` — возобновить ретрансляцию суставов, `false` — поставить на паузу |
| `/gripper_relay/set_enabled` | `std_srvs/SetBool` | То же для схвата |

После снятия паузы `arm_relay` не прыгает к текущей позе leader, а сначала плавно подводит follower к ней за `resume_time`, и только затем продолжает обычную ретрансляцию.

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

# Пауза и возобновление телеуправления
ros2 service call /arm_relay/set_enabled std_srvs/srv/SetBool "{data: false}"
ros2 service call /gripper_relay/set_enabled std_srvs/srv/SetBool "{data: false}"
ros2 service call /arm_relay/set_enabled std_srvs/srv/SetBool "{data: true}"
ros2 service call /gripper_relay/set_enabled std_srvs/srv/SetBool "{data: true}"
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
  - `std_srvs`
  - `rviz2`
- **Python:**
  - `PyYAML`

---

## Лицензия

Пакет распространяется под лицензией **MIT** (см. файл `LICENSE` в корне пакета).

---

## Версия

**1.1.0** - улучшена плавность движений в конфигурации по умолчанию.

**1.2.0** — пауза ретрансляции:
- сервисы `~/set_enabled` (`std_srvs/SetBool`) у `arm_relay` и `gripper_relay`;
- параметр `enabled` у обоих узлов;
- плавный подъезд follower к leader после снятия паузы (параметр `resume_time` у `arm_relay`);
- новая зависимость `std_srvs`.

**1.1.0** — плавное телеуправление:
- `arm_relay` по умолчанию пересылает каждое сообщение телеметрии (`publish_rate: 0.0`) вместо прореживания до 50 Гц;
- время достижения точки увеличено до `command_timeout: 0.03` с, чтобы `joint_trajectory_controller` интерполировал движение между сообщениями;
- траектория отправляется с нулевым `header.stamp` и начинает выполняться в момент получения.

Для плавной работы 1.1.0+ рассчитаны на `open_loop_control: true` у `joint_trajectory_controller` в `soarm101_ros2_control`.

**1.0.0** — пакет выпущен.

---

## Поддержка

Вопросы и предложения оформляйте через [Issues](https://github.com/cyberbanana777/so-arm101-ros2-pkgs/issues).