# soarm101_ros2_control

Пакет содержит конфигурационные файлы для **ros2_control** – как для реального робота SOARM101 (с использованием аппаратного компонента `soarm101_hardware`), так и для симуляции в Gazebo (с использованием `gz_ros2_control`). Также включает xacro‑файлы, которые подключаются в основное URDF‑описание для подстановки правильного hardware‑плагина в зависимости от режима запуска.

---

## Состав пакета

- **`config/`** – YAML‑конфигурации контроллеров:
  - `real_controllers.yaml` – для работы с реальным роботом (обновление 100 Гц, отключено симуляционное время, загружаются контроллеры: `joint_state_broadcaster`, `joint_trajectory_controller`, `gripper_controller`, `soarm101_telemetry_controller`).
  - `sim_controllers.yaml` – для симуляции (обновление 50 Гц, включено `use_sim_time: true`, те же контроллеры, но с параметрами, подходящими для Gazebo).

- **`urdf/`** – xacro‑фрагменты для включения в основной URDF:
  - `ros2_control_real.xacro` – определяет тег `<ros2_control>` с системным плагином `soarm101_hardware/SOARM101SystemHardware` и всеми необходимыми параметрами (порт, скорость, файл калибровки, парковочная позиция и т.д.). Экспортирует все state‑интерфейсы (position, velocity, effort, temperature, voltage, current, moving_flag) и command‑интерфейс (position).
  - `ros2_control_sim.xacro` – определяет тег `<ros2_control>` с плагином `gz_ros2_control/GazeboSimSystem` для работы в Gazebo. Экспортирует только position и velocity (для симуляции достаточно).

- **`IMPROVEMENTS_SIM.md`** – документ с рекомендациями по улучшению конфигурации для симуляции: добавление ПИД‑регуляторов, расширение командных интерфейсов, реализация ведомого сочленения для захвата и т.д.

---

## Использование

**Важно! Информация по использоваию ниже актуальна, если Вы хотите использовать отдельно эти конфигурацинные файлы. Агрегация описаний (геометрического и ros2_control описания робота) происходит в пакете soarm101_brongup**

### Для реального робота

1. Убедитесь, что файл калибровки (`motor_calibration.yaml`) создан пакетом `converter_calibration_data` и лежит по пути, указанному в `ros2_control_real.xacro` (по умолчанию `$(find converter_calibration_data)/config/motor_calibration.yaml`).

2. В основном URDF (обычно `soarm101.xacro`) добавьте включение:

```xml
<xacro:include filename="$(find soarm101_ros2_control)/urdf/ros2_control_real.xacro"/>
```

3. При запуске `ros2_control_node` передайте параметры из `real_controllers.yaml`:

```bash
ros2 run controller_manager ros2_control_node \
  --ros-args --params-file $(find soarm101_ros2_control)/config/real_controllers.yaml \
  -p robot_description:=$(cat $(find soarm101_description)/urdf/soarm101.xacro)
```

Либо используйте launch‑файлы, где это уже настроено (например, в `soarm101_bringup`).

### Для симуляции в Gazebo

1. В основном URDF вместо реального подключите симуляционный xacro:

```xml
<xacro:include filename="$(find soarm101_ros2_control)/urdf/ros2_control_sim.xacro"/>
```

2. Запустите Gazebo с загруженной моделью и контроллер‑менеджером, используя `sim_controllers.yaml`. Не забудьте установить `use_sim_time:=true` во всех узлах.

---

## Параметры, задаваемые в xacro для реального робота

| Параметр | Значение по умолчанию | Описание |
|----------|-----------------------|----------|
| `port` | `/dev/ttyACM0` | Последовательный порт для подключения к шине моторов |
| `baudrate` | `1000000` | Скорость в бод |
| `default_speed` | `2400` | Скорость движения по умолчанию (0–3400) |
| `default_accel` | `50` | Ускорение по умолчанию (0–254) |
| `park_positions` | `[0.004, -1.712, 1.560, 1.141, -0.029, 0.461]` | Парковочная позиция в радианах (соответствует порядку суставов) |
| `calibration_file` | `$(find converter_calibration_data)/config/motor_calibration.yaml` | Путь к YAML‑файлу калибровки |

Все эти параметры можно изменить прямо в xacro или передать через launch‑файл.

---

## Контроллеры, используемые в конфигурациях

- **`joint_state_broadcaster`** – публикует `/joint_states` для визуализации и других узлов.
- **`joint_trajectory_controller`** – основной контроллер для выполнения траекторий движения (позиционное управление).
- **`gripper_controller`** – специализированный контроллер для управления схватом (на основе `position_controllers/GripperActionController`).
- **`soarm101_telemetry_controller`** – кастомный контроллер из пакета `soarm101_telemetry_controller`, публикующий расширенную телеметрию (температура, напряжение, ток и т.д.) в топик `soarm101_telemetry_controller/motor_states`.

---

## Доработки для симуляции

В файле `IMPROVEMENTS_SIM.md` собраны предложения по улучшению работы в Gazebo:
- Добавление ПИД‑регуляторов для каждого сустава.
- Расширение набора командных интерфейсов (velocity, effort).
- Реализация ведомого сочленения для пальцев захвата.
- Настройка параметров плагина `gz_ros2_control` (`hold_joints`, `position_proportional_gain`).
- Правильная настройка `use_sim_time`.

Эти улучшения помогут добиться более реалистичного поведения робота в симуляции и упростят отладку алгоритмов управления.

---

## Зависимости

- `soarm101_hardware` – аппаратный компонент для реального робота.
- `soarm101_description` – URDF‑описание робота.
- `soarm101_telemetry_controller` – контроллер телеметрии.
- (Для симуляции) `gz_ros2_control` – плагин для Gazebo.

---

## Лицензия

Пакет распространяется под лицензией **MIT** (см. файл [LICENSE](LICENSE) в корне пакета).

---

## Поддержка

Вопросы и предложения оформляйте через [Issues](https://github.com/cyberbanana777/so-arm101-ros2-pkgs/issues).