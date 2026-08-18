
# soarm101_bringup

Пакет-агрегатор, который обеспечивает **единый запуск** всего программного стека робота SOARM101. Он объединяет запуск `robot_state_publisher`, `ros2_control_node` (или Gazebo) и всех необходимых контроллеров в зависимости от выбранного режима: **симуляция** или **реальный робот**, а также от типа руки (**leader** или **follower**).

---

## Назначение

- Предоставить одну точку входа для запуска всей системы.
- Автоматически подбирать конфигурацию `ros2_control` (реальное железо или Gazebo) на основе аргумента `use_sim`.
- Автоматически выбирать hardware-плагин (leader или follower) на основе аргумента `arm_type`.
- Загружать и активировать контроллеры выборочно:
  - `joint_state_broadcaster` – всегда.
  - `joint_trajectory_controller` и `gripper_controller` – только для follower.
  - `soarm101_telemetry_controller` – только для реального робота.
- Передавать параметры (порт, скорость, ускорение) через launch-аргументы в xacro.
- Обрабатывать xacro-файл, подставляя нужные include для `ros2_control` и разрешая пути к мешам (заменяя `package://` на абсолютные пути).

---


## Launch-файл: `bringup.launch.py`

### Аргументы

| Аргумент | Тип | По умолчанию | Описание |
|----------|-----|--------------|----------|
| `use_sim` | bool | `false` | `true` – запуск в Gazebo, `false` – реальный робот |
| `arm_type` | string | `follower` | Тип руки: `follower` (управляемая) или `leader` (только чтение) |
| `port` | string | `/dev/ttyACM0` | Последовательный порт для подключения к моторам |
| `max_speed` | int | `2400` | Максимальная скорость (0–3400) для каждого сустава |
| `max_accel` | int | `50` | Максимальное ускорение (0–254) для каждого сустава |

### Логика работы

1. **Разбор xacro** – читает `urdf/full.xacro`, передаёт в него все аргументы (`use_sim`, `arm_type`, `port`, `max_speed`, `max_accel`), получает полную URDF-строку.
2. **Замена `package://`** – все пути вида `package://soarm101_description/meshes/...` заменяются на абсолютные пути с префиксом `file://`, чтобы Gazebo и другие компоненты могли загрузить меши.
3. **Запуск `robot_state_publisher`** – публикует `<arm_prefix>/robot_description` и транслирует `tf`, где `arm_prefix` может быть или `leader`, или `follower`.
4. **Ветвление по `use_sim`**:

#### Режим симуляции (`use_sim:=true`)
- Запускает **Gazebo** через `ros_gz_sim` (пустой мир).
- Спавнит робота через `ros_gz_sim create`.
- Загружает контроллеры из `sim_controllers.yaml`:
  - `joint_state_broadcaster` – всегда.
  - `joint_trajectory_controller` – только для follower.
  - `gripper_controller` – только для follower.

#### Режим реального робота (`use_sim:=false`)
- Запускает **`ros2_control_node`** с параметрами:
  - `robot_description` (обработанный URDF)
  - `real_controllers.yaml`
- Загружает контроллеры через spawner:
  - `joint_state_broadcaster` – всегда.
  - `soarm101_telemetry_controller` – всегда.
  - `joint_trajectory_controller` – только для follower.
  - `gripper_controller` – только для follower.

---

## Полный URDF: `urdf/full.xacro`

Файл включает:
- Основной URDF робота (`soarm101_description/urdf/soarm101.xacro`).
- Макросы из `soarm101_ros2_control/urdf/ros2_control_real.xacro`.
- Логику выбора hardware-плагина:
  - **Симуляция** – подключается `ros2_control_sim.xacro` (плагин `gz_ros2_control`).
  - **Реальный робот + leader** – вызывается макрос `soarm101_hardware_leader`.
  - **Реальный робот + follower** – вызывается макрос `soarm101_hardware_follower`.

Это позволяет использовать одно и то же описание робота для разных режимов и типов руки, меняя только низкоуровневый hardware-интерфейс.

---

## Использование

### Запуск реального робота (follower)

```bash
ros2 launch soarm101_bringup bringup.launch.py use_sim:=false arm_type:=follower
```

После запуска:
- Подключается к шине моторов (порт и скорость задаются аргументами).
- Активируются все контроллеры (включая управляющие).
- Робот готов к приёму траекторий через `joint_trajectory_controller`.

### Запуск реального робота (leader – только чтение)

```bash
ros2 launch soarm101_bringup bringup.launch.py use_sim:=false arm_type:=leader
```

После запуска:
- Подключается к шине моторов (только чтение).
- Управляющие контроллеры **не загружаются**.
- Доступна только телеметрия (position, velocity, temperature, voltage, current).

### Запуск симуляции в Gazebo

```bash
ros2 launch soarm101_bringup bringup.launch.py use_sim:=true arm_type:=follower
```

После запуска:
- Открывается окно Gazebo с пустым миром.
- Робот появляется в мире с начальной позицией.
- Контроллеры работают в симуляционном режиме.

### Изменение параметров порта, скорости, ускорения

```bash
ros2 launch soarm101_bringup bringup.launch.py \
    use_sim:=false \
    arm_type:=follower \
    port:=/dev/ttyUSB0 \
    max_speed:=2000 \
    max_accel:=40
```

---

## Зависимости

- `robot_state_publisher` – публикация состояния робота.
- `controller_manager` – управление контроллерами.
- `soarm101_description` – URDF-описание.
- `soarm101_hardware` – аппаратные плагины для реального робота.
- `soarm101_ros2_control` – конфигурации `ros2_control`.
- `soarm101_telemetry_controller` – кастомный контроллер телеметрии.

Для симуляции дополнительно требуются:
- `ros_gz_sim` – интеграция Gazebo с ROS2.
- `gz_ros2_control` – плагин управления в Gazebo.

---

## Примечания

- Управляющие контроллеры (`joint_trajectory_controller`, `gripper_controller`) загружаются **только** для `arm_type == follower`.
- Для `leader` доступны только состояние и телеметрия.
- Все параметры (порт, скорость, ускорение) передаются через launch-аргументы.

---

## Лицензия

Пакет распространяется под лицензией **MIT** (см. файл [LICENSE](LICENSE) в корне пакета).

---

## Версия

**3.0.0** – добавлены префиксы в именах нод и топиков (`leader`, `follower`)
**2.0.0** – добавлена поддержка leader/follower, новые launch-аргументы, условный запуск контроллеров.

---

## Поддержка

Вопросы и предложения оформляйте через [Issues](https://github.com/cyberbanana777/so-arm101-ros2-pkgs/issues).
