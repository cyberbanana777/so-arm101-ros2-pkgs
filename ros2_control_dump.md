# Пакет: 

Это пакет ****.

---

## Файл: `CMakeLists.txt`

```text
cmake_minimum_required(VERSION 3.8)
project(soarm101_ros2_control)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# find dependencies
find_package(ament_cmake REQUIRED)
find_package(soarm101_hardware REQUIRED)
find_package(soarm101_description REQUIRED) # hide integration in the configs
find_package(soarm101_telemetry_controller REQUIRED)
# uncomment the following section in order to fill in
# further dependencies manually.
# find_package(<dependency> REQUIRED)

# Устанавливаем все необходимые директории в share/${PROJECT_NAME}
install(
  DIRECTORY
    config
    urdf
  DESTINATION share/${PROJECT_NAME}
)

if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  # the following line skips the linter which checks for copyrights
  # comment the line when a copyright and license is added to all source files
  set(ament_cmake_copyright_FOUND TRUE)
  # the following line skips cpplint (only works in a git repo)
  # comment the line when this package is in a git repo and when
  # a copyright and license is added to all source files
  set(ament_cmake_cpplint_FOUND TRUE)
  ament_lint_auto_find_test_dependencies()
endif()

ament_package()
```

## Файл: `IMPROVEMENTS_SIM.md`

```markdown

# Улучшения конфигурации ros2_control для симуляции в Gazebo

В этом документе собраны рекомендации по доработке вашего XACRO‑файла и сопутствующих настроек. Каждое улучшение описано по трём критериям:

- **Сущность** – что именно меняется и как это реализовать.
- **Сложность** – оценка трудозатрат (низкая, средняя, высокая).
- **Плюсы** – какие преимущества вы получите.

---

## 1. Расширение набора командных интерфейсов

| **Сущность** | В тегах `<joint>` добавить интерфейсы `velocity` и `effort` в дополнение к существующему `position`. |
|--------------|------------------------------------------------------------------------------------------------------|
| **Сложность** | 🟢 **Низкая** – изменение касается только XACRO, не требует правки C++ кода. |
| **Плюсы**    | • Возможность использовать разные типы контроллеров (скоростные, усилий).<br>• Гибкость при переходе от позиционного управления к более сложным законам.<br>• Совместимость с большинством готовых контроллеров ROS 2 Control. |

**Пример реализации:**

```xml
<joint name="shoulder_pan_joint">
  <command_interface name="position"/>
  <command_interface name="velocity"/>
  <command_interface name="effort"/>
  <state_interface name="position"/>
  <state_interface name="velocity"/>
  <state_interface name="effort"/>
</joint>
```

---

## 2. Настройка ПИД‑регуляторов для каждого сочленения

| **Сущность** | Внутри тега `<joint>` добавить параметры ПИД для управления положением и скоростью. |
|--------------|--------------------------------------------------------------------------------------|
| **Сложность** | 🟡 **Средняя** – требует подбора коэффициентов под конкретную модель робота. |
| **Плюсы**    | • Более точное и плавное отслеживание траекторий.<br>• Уменьшение перерегулирования и колебаний.<br>• Возможность тонкой настройки динамики каждого сочленения независимо. |

**Пример реализации:**

```xml
<joint name="shoulder_pan_joint">
  <param name="pos_kp">10.0</param>
  <param name="pos_ki">1.0</param>
  <param name="pos_kd">2.0</param>
  <param name="pos_max_integral_error">10000.0</param>
  <param name="vel_kp">10.0</param>
  <param name="vel_ki">5.0</param>
  <param name="vel_kd">2.0</param>
  <param name="vel_max_integral_error">10000.0</param>
  <!-- интерфейсы ... -->
</joint>
```

---

## 3. Реализация ведомого (mimic) сочленения для захвата

| **Сущность** | В URDF‑описании (не в `ros2_control`) добавить атрибут `mimic` для второго пальца захвата. |
|--------------|-------------------------------------------------------------------------------------------|
| **Сложность** | 🟢 **Низкая** – добавляется один атрибут в существующий URDF. |
| **Плюсы**    | • Управление захватом через одну команду (ведомый палец повторяет движение ведущего).<br>• Упрощение контроллеров и траекторий.<br>• Естественная синхронизация пальцев. |

**Пример реализации (в основном URDF):**

```xml
<joint name="gripper_finger_right_joint" type="revolute">
  <!-- ... -->
  <mimic joint="gripper_finger_left_joint" multiplier="-1.0" offset="0.0" />
</joint>
```

---

## 4. Дополнительные параметры плагина gz_ros2_control

| **Сущность** | В теге `<gazebo>` добавить параметры `<hold_joints>` и `<position_proportional_gain>`. |
|--------------|---------------------------------------------------------------------------------------|
| **Сложность** | 🟢 **Низкая** – простое добавление строк в XACRO. |
| **Плюсы**    | • `hold_joints=true` – удержание позиции при отсутствии команд (робот не «провисает»).<br>• `position_proportional_gain` – тонкая настройка встроенного П‑регулятора для уменьшения ошибки по положению. |

**Пример реализации:**

```xml
<gazebo>
  <plugin filename="gz_ros2_control-system" name="gz_ros2_control::GazeboSimROS2ControlPlugin">
    <parameters>filename="package://soarm101_ros2_control/config/sim_controllers.yaml"</parameters>
    <hold_joints>true</hold_joints>
    <position_proportional_gain>0.1</position_proportional_gain>
  </plugin>
</gazebo>
```

---

## 5. Доработка YAML‑конфигурации контроллеров

| **Сущность** | В файле `sim_controllers.yaml` добавить `joint_state_broadcaster` и основной контроллер (например, `joint_trajectory_controller`), а также установить `use_sim_time=true` в launch‑файлах. |
|--------------|----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| **Сложность** | 🟡 **Средняя** – требует понимания работы контроллеров ROS 2 Control. |
| **Плюсы**    | • Публикация `/joint_states` для визуализации и навигации.<br>• Управление траекториями через стандартный интерфейс.<br>• Синхронизация с симуляционным временем – гарантирует корректную работу всех узлов. |

**Пример структуры YAML:**

```yaml
controller_manager:
  ros__parameters:
    update_rate: 100  # Гц

    joint_state_broadcaster:
      type: joint_state_broadcaster/JointStateBroadcaster

    joint_trajectory_controller:
      type: joint_trajectory_controller/JointTrajectoryController

joint_trajectory_controller:
  ros__parameters:
    joints:
      - shoulder_pan_joint
      - shoulder_lift_joint
      - elbow_flex_joint
      - wrist_flex_joint
      - wrist_roll_joint
      - gripper_jaw_joint
    interface_name: position
    command_interfaces:
      - position
    state_interfaces:
      - position
      - velocity
```

**В launch‑файле обязательно добавьте:**

```python
Node(
    package='controller_manager',
    executable='ros2_control_node',
    parameters=[{'use_sim_time': True}],
    ...
)
```

---

## Резюме

Применение всех перечисленных улучшений позволит вам:

- значительно повысить реалистичность и управляемость робота в симуляции;
- использовать широкий арсенал стандартных контроллеров ROS 2;
- упростить разработку и отладку алгоритмов управления.

Начинайте с пунктов с **низкой сложностью** – они дают быстрый эффект, затем переходите к настройке ПИД и YAML. Удачи в настройке!
```

## Файл: `LICENSE`

```
Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
```

## Файл: `README.md`

```markdown
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
```

## Файл: `config/real_controllers.yaml`

```yaml
controller_manager:
  ros__parameters:
    update_rate: 100
    use_sim_time: false
    # ========================================

    joint_state_broadcaster:
      type: joint_state_broadcaster/JointStateBroadcaster

    joint_trajectory_controller:
      type: joint_trajectory_controller/JointTrajectoryController

    gripper_controller:
      type: position_controllers/GripperActionController
    
    soarm101_telemetry_controller:
      type: soarm101_telemetry_controller/ServoTelemetryController

# ===== КОНТРОЛЛЕРЫ (без изменений) =====
joint_trajectory_controller:
  ros__parameters:
    joints:
      - shoulder_pan_joint
      - shoulder_lift_joint
      - elbow_flex_joint
      - wrist_flex_joint
      - wrist_roll_joint
    command_interfaces:
      - position
    state_interfaces:
      - position
      - velocity

gripper_controller:
  ros__parameters:
    joint: gripper_jaw_joint
    command_interfaces:
      - position
    state_interfaces:
      - position
      - velocity

soarm101_telemetry_controller:
  ros__parameters:
    joints:
      - shoulder_pan_joint
      - shoulder_lift_joint
      - elbow_flex_joint
      - wrist_flex_joint
      - wrist_roll_joint
      - gripper_jaw_joint
    motor_ids: [1, 2, 3, 4, 5, 6]
    state_interfaces:
      - position
      - velocity
      - effort
      - temperature
      - voltage
      - current
      - moving_flag
```

## Файл: `config/sim_controllers.yaml`

```yaml
controller_manager:
  ros__parameters:
    update_rate: 50  # Hz
    use_sim_time: true

    joint_state_broadcaster:
      type: joint_state_broadcaster/JointStateBroadcaster

    joint_trajectory_controller:
      type: joint_trajectory_controller/JointTrajectoryController

    gripper_controller:
      type: position_controllers/GripperActionController

joint_trajectory_controller:
  ros__parameters:
    joints:
      - shoulder_pan_joint
      - shoulder_lift_joint
      - elbow_flex_joint
      - wrist_flex_joint
      - wrist_roll_joint
    command_interfaces:
      - position
    state_interfaces:
      - position
      - velocity
    open_loop_control: true
    allow_partial_joints_goal: false

gripper_controller:
  ros__parameters:
    joint: gripper_jaw_joint
    command_interfaces:
      - position
    state_interfaces:
      - position
      - velocity
```

## Файл: `package.xml`

```xml
<?xml version="1.0"?>
<?xml-model href="http://download.ros.org/schema/package_format3.xsd" schematypens="http://www.w3.org/2001/XMLSchema"?>
<package format="3">
  <name>soarm101_ros2_control</name>
  <version>1.0.0</version>
  <description>ROS2_control configs for SO-ARM101 for simulation (Gazebo/Ignition) and real robot (custom hardware component)</description>
  <maintainer email="sashagrachev2005@gmail.com">banana-killer</maintainer>
  <license>MIT</license>

  <buildtool_depend>ament_cmake</buildtool_depend>

  <depend>soarm101_hardware</depend>
  <depend>soarm101_description</depend>
  <depend>soarm101_telemetry_controller</depend>

  <test_depend>ament_lint_auto</test_depend>
  <test_depend>ament_lint_common</test_depend>

  <export>
    <build_type>ament_cmake</build_type>
  </export>
</package>
```

## Файл: `urdf/ros2_control_real.xacro`

```xml
<?xml version="1.0"?>
<robot xmlns:xacro="http://www.ros.org/wiki/xacro">

  <xacro:macro name="joint_servo_with_command" params="joint_name">
    <joint name="${joint_name}">
      <command_interface name="position"/>
      <state_interface name="position"/>
      <state_interface name="velocity"/>
      <state_interface name="effort"/>
      <state_interface name="temperature"/>
      <state_interface name="voltage"/>
      <state_interface name="current"/>
      <state_interface name="moving_flag"/>
    </joint>
  </xacro:macro>

  <xacro:macro name="joint_servo_without_command" params="joint_name">
    <joint name="${joint_name}">
      <state_interface name="position"/>
      <state_interface name="velocity"/>
      <state_interface name="effort"/>
      <state_interface name="temperature"/>
      <state_interface name="voltage"/>
      <state_interface name="current"/>
      <state_interface name="moving_flag"/>
    </joint>
  </xacro:macro>

  <xacro:macro name="soarm101_hardware_leader" params="port max_speed max_accel">
    <ros2_control name="RealRobotSystem" type="system">
      <hardware>
        <plugin>soarm101_hardware/SOARM101SystemHardwareLeader</plugin>
        <!-- params for plugin -->
        <param name="port">${port}</param>
        <param name="baudrate">1000000</param>
        <param name="default_speed">${max_speed}</param>
        <param name="default_accel">${max_accel}</param>
        <param name="park_positions">[0.004, -1.712, 1.560, 1.141, -0.029, 0.461]</param>
        <param name="max_torques">[2.69, 1.91, 2.69, 1.41, 1.41, 1.41]</param>
        <param name="calibration_file">$(find converter_calibration_data)/config/leader_motor_calibration.yaml</param>
       
      </hardware>
      <!-- Interfaces for each joint -->
      <xacro:joint_servo_without_command joint_name="shoulder_pan_joint" />
      <xacro:joint_servo_without_command joint_name="shoulder_lift_joint" />
      <xacro:joint_servo_without_command joint_name="elbow_flex_joint" />
      <xacro:joint_servo_without_command joint_name="wrist_flex_joint" />
      <xacro:joint_servo_without_command joint_name="wrist_roll_joint" />
      <xacro:joint_servo_without_command joint_name="gripper_jaw_joint" />

    </ros2_control>
  </xacro:macro>

  <xacro:macro name="soarm101_hardware_follower" params="port max_speed max_accel">
    <ros2_control name="RealRobotSystem" type="system">
      <hardware>
        <plugin>soarm101_hardware/SOARM101SystemHardwareFollower</plugin>
        <!-- params for plugin -->
        <param name="port">${port}</param>
        <param name="baudrate">1000000</param>
        <param name="default_speed">${max_speed}</param>
        <param name="default_accel">${max_accel}</param>
        <param name="park_positions">[0.004, -1.712, 1.560, 1.141, -0.029, 0.461]</param>
        <param name="max_torques">[2.94, 2.94, 2.94, 2.94, 2.94, 2.94]</param>

        <param name="calibration_file">$(find converter_calibration_data)/config/follower_motor_calibration.yaml</param>
       
      </hardware>
      <!-- Interfaces for each joint -->
      <xacro:joint_servo_with_command joint_name="shoulder_pan_joint" />
      <xacro:joint_servo_with_command joint_name="shoulder_lift_joint" />
      <xacro:joint_servo_with_command joint_name="elbow_flex_joint" />
      <xacro:joint_servo_with_command joint_name="wrist_flex_joint" />
      <xacro:joint_servo_with_command joint_name="wrist_roll_joint" />
      <xacro:joint_servo_with_command joint_name="gripper_jaw_joint" />

    </ros2_control>
  </xacro:macro>

</robot>
```

## Файл: `urdf/ros2_control_sim.xacro`

```xml
<?xml version="1.0"?>
<robot xmlns:xacro="http://www.ros.org/wiki/xacro">

  <!-- Плагин для Ignition Gazebo (gz_ros2_control) -->
  <gazebo>
    <plugin filename="gz_ros2_control-system" name="gz_ros2_control::GazeboSimROS2ControlPlugin">
      <parameters>filename="package://soarm101_ros2_control/config/sim_controllers.yaml"</parameters>
    </plugin>
  </gazebo>

  <!-- ros2_control tag -->
  <ros2_control name="GazeboSimSystem" type="system">
    <hardware>
      <plugin>gz_ros2_control/GazeboSimSystem</plugin>
    </hardware>
    <!-- Joints (имена остаются те же) -->
    <joint name="shoulder_pan_joint">
      <command_interface name="position"/>
      <state_interface name="position"/>
      <state_interface name="velocity"/>
    </joint>
    <joint name="shoulder_lift_joint">
      <command_interface name="position"/>
      <state_interface name="position"/>
      <state_interface name="velocity"/>
    </joint>
    <joint name="elbow_flex_joint">
      <command_interface name="position"/>
      <state_interface name="position"/>
      <state_interface name="velocity"/>
    </joint>
    <joint name="wrist_flex_joint">
      <command_interface name="position"/>
      <state_interface name="position"/>
      <state_interface name="velocity"/>
    </joint>
    <joint name="wrist_roll_joint">
      <command_interface name="position"/>
      <state_interface name="position"/>
      <state_interface name="velocity"/>
    </joint>
    <joint name="gripper_jaw_joint">
      <command_interface name="position"/>
      <state_interface name="position"/>
      <state_interface name="velocity"/>
    </joint>
  </ros2_control>

</robot>
```

