# soarm101_ros2_control

Пакет содержит конфигурационные файлы для **ros2_control** – как для реального робота SOARM101 (с использованием аппаратных плагинов из `soarm101_hardware`), так и для симуляции в Gazebo (с использованием `gz_ros2_control`). Включает xacro‑макросы, которые подключаются в основное URDF‑описание для подстановки правильного hardware‑плагина в зависимости от режима запуска и типа руки (leader/follower).

---

## Состав пакета

- **`config/`** – YAML‑конфигурации контроллеров:
  - `real_controllers.yaml` – для работы с реальным роботом (100 Гц, отключено `use_sim_time`). Загружаются контроллеры: `joint_state_broadcaster`, `joint_trajectory_controller` (только для follower), `gripper_controller` (только для follower), `soarm101_telemetry_controller`.
  - `sim_controllers.yaml` – для симуляции в Gazebo (50 Гц, включено `use_sim_time: true`). Загружаются `joint_state_broadcaster`, `joint_trajectory_controller`, `gripper_controller`.

- **`urdf/`** – xacro‑макросы для включения в основной URDF:
  - **`ros2_control_real.xacro`** – содержит макросы для реального робота:
    - `joint_servo_with_command` – объявляет command и state интерфейсы для follower.
    - `joint_servo_without_command` – объявляет только state интерфейсы для leader (без команды).
    - `soarm101_hardware_leader` – макрос для подключения плагина `SOARM101SystemHardwareLeader` с параметрами (порт, скорость, калибровка, парковка, max_torques).
    - `soarm101_hardware_follower` – макрос для подключения плагина `SOARM101SystemHardwareFollower` с параметрами.
  - **`ros2_control_sim.xacro`** – макрос для симуляции в Gazebo с плагином `gz_ros2_control/GazeboSimSystem`. Экспортирует только position и velocity.

- **`IMPROVEMENTS_SIM.md`** – документ с рекомендациями по улучшению конфигурации для симуляции: добавление ПИД‑регуляторов, расширение командных интерфейсов, mimic-сочленения и т.д.

---

## Использование

**Важно!** Эти конфигурационные файлы предназначены для использования через пакет `soarm101_bringup`, который автоматически выбирает нужный макрос на основе аргумента `arm_type`. Если вы используете их отдельно – следуйте инструкциям ниже.

### Для реального робота

1. Убедитесь, что файлы калибровки созданы пакетом `converter_calibration_data` и лежат по путям, указанным в макросах:
   - Для лидера: `leader_motor_calibration.yaml`
   - Для фолловера: `follower_motor_calibration.yaml`

2. В основном URDF (например, `full.xacro` из `soarm101_bringup`) добавьте включение нужного макроса в зависимости от `arm_type`:
   ```xml
   <xacro:if value="$(arg arm_type) == 'leader'">
     <xacro:soarm101_hardware_leader port="$(arg port)" max_speed="$(arg max_speed)" max_accel="$(arg max_accel)"/>
   </xacro:if>
   <xacro:if value="$(arg arm_type) == 'follower'">
     <xacro:soarm101_hardware_follower port="$(arg port)" max_speed="$(arg max_speed)" max_accel="$(arg max_accel)"/>
   </xacro:if>
   ```

3. При запуске `ros2_control_node` передайте параметры из `real_controllers.yaml`:
   ```bash
   ros2 run controller_manager ros2_control_node \
     --ros-args --params-file $(find soarm101_ros2_control)/config/real_controllers.yaml \
     -p robot_description:=$(cat $(find soarm101_description)/urdf/soarm101.xacro)
   ```

### Для симуляции в Gazebo

1. В основном URDF подключите симуляционный макрос:
   ```xml
   <xacro:include filename="$(find soarm101_ros2_control)/urdf/ros2_control_sim.xacro"/>
   ```

2. Запустите Gazebo с загруженной моделью и контроллер-менеджером, используя `sim_controllers.yaml`. Не забудьте установить `use_sim_time:=true`.

---

## Параметры, задаваемые в xacro для реального робота

| Параметр | Тип | Описание | Пример |
|----------|-----|----------|--------|
| `port` | string | Последовательный порт для подключения к моторам | `/dev/ttyACM0` |
| `max_speed` | int | Максимальная скорость (0–3400) | `2400` |
| `max_accel` | int | Максимальное ускорение (0–254) | `50` |
| `park_positions` | список double | Парковочная позиция в радианах (порядок joints) | `[0.004, -1.712, 1.560, 1.141, -0.029, 0.461]` |
| `max_torques` | список double | Максимальные моменты для каждого сустава (Н·м) | `[2.94, 2.94, 2.94, 2.94, 2.94, 2.94]` |
| `calibration_file` | string | Путь к YAML-файлу калибровки | `$(find converter_calibration_data)/config/leader_motor_calibration.yaml` |

**Примечание:** для лидера и фолловера используются разные файлы калибровки и разные значения max_torques.

---

## Контроллеры, используемые в конфигурациях

- **`joint_state_broadcaster`** – публикует `/joint_states` для визуализации и других узлов (всегда включён).
- **`joint_trajectory_controller`** – основной контроллер для выполнения траекторий (включается только для follower).
- **`gripper_controller`** – контроллер для управления схватом (включается только для follower).
- **`soarm101_telemetry_controller`** – кастомный контроллер для публикации расширенной телеметрии (всегда включён для реального робота). В версии 2.1.0 добавлена поддержка интерфейсов `max_torque` и `enable_torque`, что позволяет отслеживать максимальный момент и состояние включения момента для каждого сустава.

---

## Важные замечания

### Для лидера (без управления)
- **Command интерфейсы не экспортируются** – макрос `joint_servo_without_command` не содержит тегов `<command_interface>`. Поэтому контроллеры управления (например, `joint_trajectory_controller`) **не могут быть загружены** для лидера. Это ожидаемое поведение.
- **`effort` и `moving_flag` всегда равны 0** – поскольку моторы лидера не получают команд через ros2_control, эти поля остаются нулевыми.
- Лидер используется только для чтения состояния (телеметрия).

### Для фолловера (с управлением)
- Экспортируются все интерфейсы (position, velocity, effort, temperature, voltage, current, moving_flag, max_torque, enable_torque) и command интерфейс (position).
- Контроллеры управления загружаются и активны.

---

## Доработки для симуляции

В файле `IMPROVEMENTS_SIM.md` собраны рекомендации по улучшению работы в Gazebo:
- Добавление ПИД-регуляторов.
- Расширение набора командных интерфейсов.
- Реализация mimic-сочленений.
- Настройка параметров плагина `gz_ros2_control`.

Эти улучшения помогут добиться более реалистичного поведения робота в симуляции.

---

## Зависимости

- `soarm101_hardware` – аппаратные плагины для реального робота.
- `soarm101_description` – URDF-описание робота.
- `soarm101_telemetry_controller` – контроллер телеметрии (версии >= 1.1.0, поддерживающей новые интерфейсы).
- (Для симуляции) `gz_ros2_control` – плагин для Gazebo.

---

## Лицензия

Пакет распространяется под лицензией **MIT** (см. файл [LICENSE](LICENSE) в корне пакета).

---

## Версия

**3.0.0** – изменена структура конфига для реального робота. Добавленно разделение по префиксу, т.е. есть отдельный конфиг для `leader` и отдельный для `follower`.

**2.1.0** – добавлена поддержка интерфейсов `max_torque` и `enable_torque` в конфигурациях для реального робота; обновлены xacro-макросы и YAML-конфигурация контроллера телеметрии.

---

## Поддержка

Вопросы и предложения оформляйте через [Issues](https://github.com/cyberbanana777/so-arm101-ros2-pkgs/issues).
