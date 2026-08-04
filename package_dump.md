# Пакет: 

Это пакет ****.

---

## Файл: `CMakeLists.txt`

```text
cmake_minimum_required(VERSION 3.16)
project(soarm101_hardware LANGUAGES CXX)

if(CMAKE_CXX_COMPILER_ID MATCHES "(GNU|Clang)")
  add_compile_options(-Wall -Wextra)
endif()

# ROS 2 dependencies for hardware interface
set(THIS_PACKAGE_INCLUDE_DEPENDS
  hardware_interface
  pluginlib
  rclcpp
  rclcpp_lifecycle
  std_srvs 
)

find_package(ament_cmake REQUIRED)
foreach(Dependency IN ITEMS ${THIS_PACKAGE_INCLUDE_DEPENDS})
  find_package(${Dependency} REQUIRED)
endforeach()

find_package(scservo_sdk REQUIRED)

## Build hardware-plugin
add_library(
  soarm101_hardware
  SHARED
  src/soarm101_hardware.cpp
)

add_library(
  soarm101_hardware
  SHARED
  src/soarm101_hardware.cpp
)

target_compile_features(soarm101_hardware PUBLIC cxx_std_17)
target_include_directories(soarm101_hardware PUBLIC
  $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include/${PROJECT_NAME}>
)

# std ROS2-dependencies
ament_target_dependencies(
  soarm101_hardware PUBLIC
  ${THIS_PACKAGE_INCLUDE_DEPENDS}
)

# link lib scservo_sdk from scservo_sdk
target_link_libraries(soarm101_hardware PUBLIC
  scservo_sdk::scservo_sdk
)

# registration plugin for ros2_control
pluginlib_export_plugin_description_file(hardware_interface soarm101_hardware.xml)

## Install
install(
  DIRECTORY include/
  DESTINATION include/${PROJECT_NAME}
)

install(TARGETS soarm101_hardware
  EXPORT export_soarm101_hardware
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)

install(FILES LICENSE NOTICE DESTINATION share/${PROJECT_NAME}/)

## Tests
if(BUILD_TESTING)
  find_package(ament_cmake_gtest REQUIRED)
endif()

## Export for others pkgs
ament_export_targets(export_soarm101_hardware HAS_LIBRARY_TARGET)
ament_export_dependencies(${THIS_PACKAGE_INCLUDE_DEPENDS})
ament_package()
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

## Файл: `NOTICE`

```
SOARM101 Hardware Component
Copyright 2024 The HuggingFace Inc. team.
Modifications and additional features:
  Copyright 2026 Alice Zenina and Alexander Grachev RTU MIREA (Russia)
This software includes code originally developed by HuggingFace Inc.,
distributed under the Apache License, Version 2.0.
```

## Файл: `README.md`

```markdown
# soarm101_hardware

Пакет реализует аппаратный интерфейс (`hardware_interface::SystemInterface`) для робота SOARM101 в экосистеме **ros2_control**. Он обеспечивает низкоуровневое управление шестью сервоприводами Feetech (серии SMS/STS) через последовательный порт, загрузку калибровки из YAML-файла и предоставляет стандартные интерфейсы состояния и команды для контроллеров.

---

## Назначение

- Подключение к реальному роботу SOARM101 через USB/UART.
- Управление позицией каждого сустава с задаваемой скоростью и ускорением.
- Чтение телеметрии: положение, скорость, усилие, температура, напряжение, ток, флаг движения.
- Поддержка парковочной позиции при деактивации.
- Экспорт интерфейсов для `ros2_control`:
  - **Состояние**: `position`, `velocity`, `effort`, `temperature`, `voltage`, `current`, `moving_flag`.
  - **Команда**: `position` (только позиционное управление).

---

## Архитектура и ключевые особенности

Пакет построен на базе класса `SOARM101SystemHardware`, наследующего `SystemInterface`. Внутри используется объект `SMS_STS` из `scservo_sdk` для общения с сервоприводами.

**Основные компоненты:**

- **`Motor`** – агрегированная структура для каждого мотора (ID, имя, команда, показания датчиков, калибровочные коэффициенты).
- **Калибровка** – загружается из YAML-файла, задаёт `range_min`/`range_max` для каждого сустава. На основе этих значений и лимитов из URDF предвычисляются коэффициенты для быстрого преобразования raw ↔ радианы.
- **Параметры** – все настраиваются через `ros2_control` `hardware_parameters` в файле конфигурации.
- **Парковка** – при деактивации робот перемещается в заданную позицию (с половинной скоростью/ускорением), затем отключается момент.
- **Синхронная запись** – команды отправляются всем моторам одной `SyncWritePosEx` для минимальной задержки.

---

## Параметры конфигурации (в `ros2_control`)

В файле описания `ros2_control` необходимо указать:

| Параметр | Тип | Описание | Пример |
|----------|-----|----------|--------|
| `port` | string | Путь к последовательному порту | `/dev/ttyACM0` |
| `baudrate` | int | Скорость в бод (обычно 1000000) | `1000000` |
| `calibration_file` | string | Путь к YAML-файлу калибровки (относительно корня workspace или абсолютный) | `$(find-pkg-prefix converter_calibration_data)/config/motor_calibration.yaml` |
| `default_speed` | int | Скорость движения по умолчанию (0–3400) | `2400` |
| `default_accel` | int | Ускорение по умолчанию (0–254) | `50` |
| `park_positions` | список double | Парковочная позиция в радианах для каждого сустава в порядке `joints` | `[0.0, 0.0, 0.0, 0.0, 0.0, 0.0]` |

---

## Использование

Пакет подключается автоматически при запуске `ros2_control` через `controller_manager`. Обычно он включается в launch-файл `soarm101_bringup`:

```python
from launch_ros.actions import Node

controller_manager = Node(
    package="controller_manager",
    executable="ros2_control_node",
    parameters=[{"robot_description": robot_description}, config_file],
    output="screen",
)
```

После активации `ros2_control` интерфейсы становятся доступными для контроллеров (например, `joint_trajectory_controller`). Состояния публикуются через стандартные топики (`/joint_states`), а дополнительные данные можно получить через `StateInterface` (например, `temperature`).

---

## Зависимости

- `hardware_interface` – интерфейс ros2_control.
- `pluginlib` – регистрация плагина.
- `rclcpp`, `rclcpp_lifecycle` – жизненный цикл узла.
- `scservo_sdk` – SDK для работы с Feetech-сервоприводами.
- `std_srvs` – (используется в коде, но не критично).

---

## Лицензия

Исходный код основан на разработках HuggingFace Inc. (Apache-2.0) и дополнен модификациями авторов (Alice Zenina, Alexander Grachev, RTU MIREA, 2026).  
Пакет распространяется под лицензией **Apache-2.0** (см. файл [LICENSE](LICENSE) в корне пакета).

---

## Поддержка

Вопросы и предложения оформляйте через [Issues](https://github.com/cyberbanana777/so-arm101-ros2-pkgs/issues).
```

## Файл: `README_EN.md`

```markdown
# soarm101_hardware

The package implements a hardware interface (`hardware_interface::SystemInterface`) for the SOARM101 robot within the **ros2_control** ecosystem. It provides low-level control of six Feetech servos (SMS/STS series) over a serial port, loads calibration from a YAML file, and exposes standard state and command interfaces for controllers.

---

## Purpose

- Connection to the real SOARM101 robot via USB/UART.
- Position control for each joint with configurable speed and acceleration.
- Telemetry reading: position, velocity, effort, temperature, voltage, current, moving flag.
- Support for a park position on deactivation.
- Export of interfaces for `ros2_control`:
  - **State**: `position`, `velocity`, `effort`, `temperature`, `voltage`, `current`, `moving_flag`.
  - **Command**: `position` (position control only).

---

## Architecture and Key Features

The package is built around the `SOARM101SystemHardware` class, which inherits from `SystemInterface`. Internally, it uses an `SMS_STS` object from `scservo_sdk` to communicate with the servos.

**Main components:**

- **`Motor`** – an aggregated structure for each motor (ID, name, command, sensor readings, calibration coefficients).
- **Calibration** – loaded from a YAML file, specifying `range_min`/`range_max` for each joint. Based on these values and the URDF limits, coefficients are precomputed for fast raw ↔ radian conversion.
- **Parameters** – all configurable via `ros2_control` `hardware_parameters` in the configuration file.
- **Parking** – on deactivation, the robot moves to a specified position (with half speed/acceleration), then torque is disabled.
- **Synchronous write** – commands are sent to all motors with a single `SyncWritePosEx` for minimal latency.

---

## Configuration Parameters (in `ros2_control`)

In the `ros2_control` description file, the following parameters must be specified:

| Parameter | Type | Description | Example |
|-----------|------|-------------|---------|
| `port` | string | Path to the serial port | `/dev/ttyACM0` |
| `baudrate` | int | Baud rate (typically 1000000) | `1000000` |
| `calibration_file` | string | Path to the YAML calibration file (relative to workspace root or absolute) | `$(find-pkg-prefix converter_calibration_data)/config/motor_calibration.yaml` |
| `default_speed` | int | Default movement speed (0–3400) | `2400` |
| `default_accel` | int | Default acceleration (0–254) | `50` |
| `park_positions` | list of double | Park position in radians for each joint in the order of `joints` | `[0.0, 0.0, 0.0, 0.0, 0.0, 0.0]` |

---

## Usage

The package is automatically loaded when `ros2_control` is started via `controller_manager`. It is typically included in the `soarm101_bringup` launch file:

```python
from launch_ros.actions import Node

controller_manager = Node(
    package="controller_manager",
    executable="ros2_control_node",
    parameters=[{"robot_description": robot_description}, config_file],
    output="screen",
)
```

After `ros2_control` activation, the interfaces become available to controllers (e.g., `joint_trajectory_controller`). States are published via standard topics (`/joint_states`), and additional data can be accessed through the `StateInterface` (e.g., `temperature`).

---

## Dependencies

- `hardware_interface` – ros2_control interface.
- `pluginlib` – plugin registration.
- `rclcpp`, `rclcpp_lifecycle` – node lifecycle.
- `scservo_sdk` – SDK for Feetech servo communication.
- `std_srvs` – (used in the code but not critical).

---

## License

The source code is based on developments by HuggingFace Inc. (Apache‑2.0) and supplemented with modifications by the authors (Alice Zenina, Alexander Grachev, RTU MIREA, 2026).  
The package is distributed under the **Apache‑2.0** license (see the [LICENSE](LICENSE) file in the package root).

---

## Support

For questions and suggestions, please use [Issues](https://github.com/cyberbanana777/so-arm101-ros2-pkgs/issues).
```

## Файл: `include/soarm101_hardware/soarm101_hardware_follower.hpp`

```cpp
// Copyright 2024 The HuggingFace Inc. team. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
//
// Copyright 2026 Alice Zenina and Alexander Grachev RTU MIREA (Russia). All rights reserved.
// Modifications to the original work are licensed under the same Apache 2.0 license.
//
// =============================================================================
// MODIFICATIONS (by Alice Zenina and Alexander Grachev, 2026-07-24)
// =============================================================================
// - Aggregated all motor data into single `Motor` struct.
// - Added park position support (`park_positions` parameter).
// - Added configurable default speed/accel (`default_speed`, `default_accel`).
// - Precomputed calibration coefficients for faster conversion.
// - Replaced dynamic vectors with std::array in write().
// - Unified read logic into `readMotorData()`.
// - Fixed motor_id vs index bug in sensor reading.
// - Changed moving_flag type to double for StateInterface compatibility.
// - Added `moveToParkPosition()` and `parseParkPositions()`.
// =============================================================================


#ifndef SOARM101_HARDWARE_FOLLOWER_SOARM101_SYSTEM_HPP_
#define SOARM101_HARDWARE_FOLLOWWR_SOARM101_SYSTEM_HPP_

#include <array>
#include <memory>
#include <string>
#include <vector>
#include <map>

#include "hardware_interface/handle.hpp"
#include "hardware_interface/hardware_info.hpp"
#include "hardware_interface/system_interface.hpp"
#include "hardware_interface/types/hardware_interface_return_values.hpp"
#include "rclcpp/macros.hpp"
#include "rclcpp_lifecycle/node_interfaces/lifecycle_node_interface.hpp"
#include "rclcpp_lifecycle/state.hpp"

#include "SCServo.h"

//! Macro to enable servo (Enable Torque)
#define ENABLE_SERVO  1
//! Macro to disable servo (Disable Torque)
#define DISABLE_SERVO 0
//! Fixed number of motors in the robot
#define NUM_MOTORS 6

namespace soarm101_hardware_follower
{

/**
 * @brief Main hardware interface class for the SO-ARM101 robot.
 * 
 * Implements hardware_interface::SystemInterface for controlling 6 Feetech STS
 * servos via a serial port. Provides state interfaces (position, velocity,
 * effort, temperature, voltage, current, moving flag) and command interfaces
 * (position).
 * 
 * Key features:
 * - Loads calibration from YAML file.
 * - Park position on deactivation.
 * - Configurable default speed and acceleration.
 * - Optimised raw ↔ radian conversions.
 */
class SOARM101SystemHardwareFollower : public hardware_interface::SystemInterface
{
public:
  RCLCPP_SHARED_PTR_DEFINITIONS(SOARM101SystemHardwareFollower)

  /**
   * @brief Constructor. Initialises flags.
   */
  SOARM101SystemHardwareFollower();

  /**
   * @brief Destructor. Disconnects the driver if initialised.
   */
  ~SOARM101SystemHardwareFollower();

  // ==================== Overridden methods from SystemInterface ====================

  /**
   * @brief Initialisation of the component.
   * @param info HardwareInfo structure with parameters from ROS 2.
   * @return SUCCESS on success, otherwise ERROR.
   * 
   * Called once when the plugin is loaded. Reads port, baud rate, calibration file,
   * as well as user parameters default_speed, default_accel, park_positions.
   * Creates motor structures.
   */
  hardware_interface::CallbackReturn on_init(
    const hardware_interface::HardwareInfo & info) override;

  /**
   * @brief Configuration of the component.
   * @param previous_state State before configuration (not used).
   * @return SUCCESS on success, otherwise ERROR.
   * 
   * Loads calibration, connects to the serial port, initialises commands with
   * current positions.
   */
  hardware_interface::CallbackReturn on_configure(
    const rclcpp_lifecycle::State & previous_state) override;

  /**
   * @brief Export state interfaces (reading data from motors).
   * @return Vector of StateInterface pointers to sensor fields of each motor.
   * 
   * Exports: position, velocity, effort, temperature, voltage, current, moving_flag.
   */
  std::vector<hardware_interface::StateInterface> export_state_interfaces() override;

  /**
   * @brief Export command interfaces (writing data to motors).
   * @return Vector of CommandInterface pointers to command_position of each motor.
   * 
   * Exports only position (position control mode).
   */
  std::vector<hardware_interface::CommandInterface> export_command_interfaces() override;

  /**
   * @brief Activation of the component.
   * @param previous_state State before activation (not used).
   * @return SUCCESS on success, otherwise ERROR.
   * 
   * Enables torque on all motors, reads current positions and sets commands
   * equal to current positions to prevent jerks.
   */
  hardware_interface::CallbackReturn on_activate(
    const rclcpp_lifecycle::State & previous_state) override;

  /**
   * @brief Deactivation of the component.
   * @param previous_state State before deactivation (not used).
   * @return SUCCESS on success, otherwise ERROR.
   * 
   * If a park position is set, moves the robot to it (with reduced speed),
   * then disables torque on all motors.
   */
  hardware_interface::CallbackReturn on_deactivate(
    const rclcpp_lifecycle::State & previous_state) override;

  /**
   * @brief Read data from motors.
   * @param time Current time (not used).
   * @param period Period (not used).
   * @return return_type::OK always.
   * 
   * Called cyclically (usually 100 Hz). Updates sensors.position etc.
   */
  hardware_interface::return_type read(
    const rclcpp::Time & time, const rclcpp::Duration & period) override;

  /**
   * @brief Write commands to motors.
   * @param time Current time (not used).
   * @param period Period (not used).
   * @return return_type::OK always.
   * 
   * Called cyclically. Sends position commands synchronously using SyncWritePosEx.
   * Speed and acceleration are taken from default_speed_ and default_accel_.
   */
  hardware_interface::return_type write(
    const rclcpp::Time & time, const rclcpp::Duration & period) override;

private:
  // ==================== Nested structures ====================

  /**
   * @brief Sensor readings of a single motor.
   */
  struct MotorSensor
  {
    double position = 0.0;      ///< Current position, radians
    double velocity = 0.0;      ///< Current velocity, rad/s
    double effort   = 0.0;      ///< Current effort (load), N*m
    double temperature = 0.0;   ///< Motor temperature, °C
    double voltage  = 0.0;      ///< Supply voltage, V
    double current  = 0.0;      ///< Current, A
    double moving_flag = 0.0;   ///< Motion flag (0 – stopped, 1 – moving)
    double enable_torque = 0.0;

  };

  /**
   * @brief Motor calibration parameters.
   * 
   * Contains raw limits (range_min, range_max) and precomputed coefficients
   * for fast raw ↔ radian conversion.
   */
  struct MotorCalibration
  {
    int drive_mode;                 ///< Motor drive mode (not used)
    int range_min;                  ///< Minimum raw encoder value
    int range_max;                  ///< Maximum raw encoder value
    double raw_to_rad_scale;        ///< Scale for raw->radians conversion
    double raw_to_rad_offset;       ///< Offset for raw->radians conversion
    double rad_to_raw_scale;        ///< Scale for radians->raw conversion
    double rad_to_raw_offset;       ///< Offset for radians->raw conversion
    double urdf_lower;              ///< Lower limit from URDF, radians
    double urdf_upper;              ///< Upper limit from URDF, radians
  };

  /**
   * @brief Aggregated structure for one motor.
   * 
   * Contains all information: ID, name, target command, max torque, flag 'enable torque',
   * sensor readings, and calibration.
   */
  struct Motor
  {
    int id;                         ///< Motor ID (1..6)
    std::string joint_name;         ///< Joint name from URDF
    double max_torque = 0.0;        ///< Max torque, N*m
    double command_position = 0.0;  ///< Target position, radians
    MotorSensor sensors;            ///< Sensor readings
    MotorCalibration calibration;   ///< Calibration parameters
  };

  // ==================== Class members ====================

  //! Parameters from configuration
  std::string port_;                ///< Serial port (e.g., /dev/ttyACM0)
  int         baudrate_;            ///< Baud rate
  std::string calibration_file_;    ///< Path to YAML calibration file

  //! Parameters loaded from info_.hardware_parameters
  u16 default_speed_ = 2400;        ///< Default speed for writing
  u8  default_accel_ = 50;          ///< Default acceleration for writing
  std::vector<double> park_positions_; ///< Park position (radians) in info_.joints order
  std::vector<double> max_torques_; ///< Max torques (N * m)

  //! Motor data
  std::vector<Motor> motors_;       ///< Vector of Motor structures, size = info_.joints.size()
  std::map<std::string, int> motor_ids_; ///< Mapping joint name -> motor ID

  //! SCServo SDK driver
  SMS_STS servo_driver_;
  bool driver_initialized_;         ///< Flag indicating successful driver connection

  // ==================== Private methods ====================

  /**
   * @brief Load calibration from YAML file.
   * @return true on success, false on error.
   * 
   * Parses the file; joint names must match keys in motor_ids_.
   * For each motor calls updateCalibrationCoefficients().
   */
  bool loadCalibration();

  /**
   * @brief Precompute coefficients for fast conversion.
   * @param motor Reference to Motor structure whose calibration will be updated.
   * 
   * Determines URDF limits based on motor.id, then computes raw_to_rad_scale etc.
   */
  void updateCalibrationCoefficients(Motor & motor);

  /**
   * @brief Read data from a single motor.
   * @param index Index in info_.joints and motors_ vectors.
   * 
   * Calls FeedBack, then ReadPos, ReadSpeed, etc., fills sensors.
   */
  void readMotorData(size_t index);

  /**
   * @brief Convert raw encoder value to radians.
   * @param raw_position Raw value (0-4095).
   * @param motor Reference to Motor containing calibration.
   * @return Angle in radians.
   * 
   * Uses precomputed coefficients from motor.calibration.
   */
  double rawToRadians(int raw_position, const Motor & motor);

  /**
   * @brief Convert radians to raw encoder value.
   * @param radians Angle in radians.
   * @param motor Reference to Motor containing calibration.
   * @return Raw value (0-4095), clipped to URDF limits.
   */
  int radiansToRaw(double radians, const Motor & motor);

  /**
   * @brief Move the robot to the park position.
   * 
   * Sets command_position from park_positions_, sends commands with half
   * speed/acceleration, then waits for movement completion (timeout 10 s).
   */
  void moveToParkPosition();

  /**
   * @brief Parse a string containing an array of numbers for park_positions.
   * @param str String like "[0.004, -1.712, ...]" or "0.004, -1.712, ..."
   * @return Vector of double numbers.
   */
  std::vector<double> parseParkPositions(const std::string & str);

  /**
   * @brief Parse a string containing an array of numbers for max_torques.
   * @param str String like "[0.004, 1.712, ...]" or "0.004, 1.712, ..."
   * @return Vector of double numbers.
   */
  std::vector<double> parseMaxTorques(const std::string & str);
};

}  // namespace soarm101_hardware_LEADER

#endif  // SOARM101_HARDWARE_LEADER_SOARM101_SYSTEM_HPP_
```

## Файл: `include/soarm101_hardware/soarm101_hardware_leader.hpp`

```cpp
// Copyright 2024 The HuggingFace Inc. team. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
//
// Copyright 2026 Alice Zenina and Alexander Grachev RTU MIREA (Russia). All rights reserved.
// Modifications to the original work are licensed under the same Apache 2.0 license.
//
// =============================================================================
// MODIFICATIONS (by Alice Zenina and Alexander Grachev, 2026-07-24)
// =============================================================================
// - Aggregated all motor data into single `Motor` struct.
// - Added park position support (`park_positions` parameter).
// - Added configurable default speed/accel (`default_speed`, `default_accel`).
// - Precomputed calibration coefficients for faster conversion.
// - Replaced dynamic vectors with std::array in write().
// - Unified read logic into `readMotorData()`.
// - Fixed motor_id vs index bug in sensor reading.
// - Changed moving_flag type to double for StateInterface compatibility.
// - Added `moveToParkPosition()` and `parseParkPositions()`.
// =============================================================================


#ifndef SOARM101_HARDWARE__SOARM101_SYSTEM_HPP_
#define SOARM101_HARDWARE__SOARM101_SYSTEM_HPP_

#include <array>
#include <memory>
#include <string>
#include <vector>
#include <map>

#include "hardware_interface/handle.hpp"
#include "hardware_interface/hardware_info.hpp"
#include "hardware_interface/system_interface.hpp"
#include "hardware_interface/types/hardware_interface_return_values.hpp"
#include "rclcpp/macros.hpp"
#include "rclcpp_lifecycle/node_interfaces/lifecycle_node_interface.hpp"
#include "rclcpp_lifecycle/state.hpp"

#include "SCServo.h"

//! Macro to enable servo (Enable Torque)
#define ENABLE_SERVO  1
//! Macro to disable servo (Disable Torque)
#define DISABLE_SERVO 0
//! Fixed number of motors in the robot
#define NUM_MOTORS 6

namespace soarm101_hardware_leader
{

/**
 * @brief Main hardware interface class for the SO-ARM101 robot.
 * 
 * Implements hardware_interface::SystemInterface for controlling 6 Feetech STS
 * servos via a serial port. Provides state interfaces (position, velocity,
 * effort, temperature, voltage, current, moving flag) and command interfaces
 * (position).
 * 
 * Key features:
 * - Loads calibration from YAML file.
 * - Park position on deactivation.
 * - Configurable default speed and acceleration.
 * - Optimised raw ↔ radian conversions.
 */
class SOARM101SystemHardwareLeader : public hardware_interface::SystemInterface
{
public:
  RCLCPP_SHARED_PTR_DEFINITIONS(SOARM101SystemHardwareLeader)

  /**
   * @brief Constructor. Initialises flags.
   */
  SOARM101SystemHardwareLeader();

  /**
   * @brief Destructor. Disconnects the driver if initialised.
   */
  ~SOARM101SystemHardwareLeader();

  // ==================== Overridden methods from SystemInterface ====================

  /**
   * @brief Initialisation of the component.
   * @param info HardwareInfo structure with parameters from ROS 2.
   * @return SUCCESS on success, otherwise ERROR.
   * 
   * Called once when the plugin is loaded. Reads port, baud rate, calibration file,
   * as well as user parameters default_speed, default_accel, park_positions.
   * Creates motor structures.
   */
  hardware_interface::CallbackReturn on_init(
    const hardware_interface::HardwareInfo & info) override;

  /**
   * @brief Configuration of the component.
   * @param previous_state State before configuration (not used).
   * @return SUCCESS on success, otherwise ERROR.
   * 
   * Loads calibration, connects to the serial port, initialises commands with
   * current positions.
   */
  hardware_interface::CallbackReturn on_configure(
    const rclcpp_lifecycle::State & previous_state) override;

  /**
   * @brief Export state interfaces (reading data from motors).
   * @return Vector of StateInterface pointers to sensor fields of each motor.
   * 
   * Exports: position, velocity, effort, temperature, voltage, current, moving_flag.
   */
  std::vector<hardware_interface::StateInterface> export_state_interfaces() override;

  /**
   * @brief Export command interfaces (writing data to motors).
   * @return Vector of CommandInterface pointers to command_position of each motor.
   * 
   * Exports only position (position control mode).
   */
  std::vector<hardware_interface::CommandInterface> export_command_interfaces() override;

  /**
   * @brief Activation of the component.
   * @param previous_state State before activation (not used).
   * @return SUCCESS on success, otherwise ERROR.
   * 
   * Enables torque on all motors, reads current positions and sets commands
   * equal to current positions to prevent jerks.
   */
  hardware_interface::CallbackReturn on_activate(
    const rclcpp_lifecycle::State & previous_state) override;

  /**
   * @brief Deactivation of the component.
   * @param previous_state State before deactivation (not used).
   * @return SUCCESS on success, otherwise ERROR.
   * 
   * If a park position is set, moves the robot to it (with reduced speed),
   * then disables torque on all motors.
   */
  hardware_interface::CallbackReturn on_deactivate(
    const rclcpp_lifecycle::State & previous_state) override;

  /**
   * @brief Read data from motors.
   * @param time Current time (not used).
   * @param period Period (not used).
   * @return return_type::OK always.
   * 
   * Called cyclically (usually 100 Hz). Updates sensors.position etc.
   */
  hardware_interface::return_type read(
    const rclcpp::Time & time, const rclcpp::Duration & period) override;

  /**
   * @brief Write commands to motors.
   * @param time Current time (not used).
   * @param period Period (not used).
   * @return return_type::OK always.
   * 
   * Called cyclically. Sends position commands synchronously using SyncWritePosEx.
   * Speed and acceleration are taken from default_speed_ and default_accel_.
   */
  hardware_interface::return_type write(
    const rclcpp::Time & time, const rclcpp::Duration & period) override;

private:
  // ==================== Nested structures ====================

  /**
   * @brief Sensor readings of a single motor.
   */
  struct MotorSensor
  {
    double position = 0.0;      ///< Current position, radians
    double velocity = 0.0;      ///< Current velocity, rad/s
    double effort   = 0.0;      ///< Current effort (load), N*m
    double temperature = 0.0;   ///< Motor temperature, °C
    double voltage  = 0.0;      ///< Supply voltage, V
    double current  = 0.0;      ///< Current, A
    double moving_flag = 0.0;   ///< Motion flag (0 – stopped, 1 – moving)
    double enable_torque = 0.0;

  };

  /**
   * @brief Motor calibration parameters.
   * 
   * Contains raw limits (range_min, range_max) and precomputed coefficients
   * for fast raw ↔ radian conversion.
   */
  struct MotorCalibration
  {
    int drive_mode;                 ///< Motor drive mode (not used)
    int range_min;                  ///< Minimum raw encoder value
    int range_max;                  ///< Maximum raw encoder value
    double raw_to_rad_scale;        ///< Scale for raw->radians conversion
    double raw_to_rad_offset;       ///< Offset for raw->radians conversion
    double rad_to_raw_scale;        ///< Scale for radians->raw conversion
    double rad_to_raw_offset;       ///< Offset for radians->raw conversion
    double urdf_lower;              ///< Lower limit from URDF, radians
    double urdf_upper;              ///< Upper limit from URDF, radians
  };

  /**
   * @brief Aggregated structure for one motor.
   * 
   * Contains all information: ID, name, target command, max torque, flag 'enable torque',
   * sensor readings, and calibration.
   */
  struct Motor
  {
    int id;                         ///< Motor ID (1..6)
    std::string joint_name;         ///< Joint name from URDF
    double max_torque = 0.0;        ///< Max torque, N*m
    double command_position = 0.0;  ///< Target position, radians
    MotorSensor sensors;            ///< Sensor readings
    MotorCalibration calibration;   ///< Calibration parameters
  };

  // ==================== Class members ====================

  //! Parameters from configuration
  std::string port_;                ///< Serial port (e.g., /dev/ttyACM0)
  int         baudrate_;            ///< Baud rate
  std::string calibration_file_;    ///< Path to YAML calibration file

  //! Parameters loaded from info_.hardware_parameters
  u16 default_speed_ = 2400;        ///< Default speed for writing
  u8  default_accel_ = 50;          ///< Default acceleration for writing
  std::vector<double> park_positions_; ///< Park position (radians) in info_.joints order
  std::vector<double> max_torques_; ///< Max torques (N * m)

  //! Motor data
  std::vector<Motor> motors_;       ///< Vector of Motor structures, size = info_.joints.size()
  std::map<std::string, int> motor_ids_; ///< Mapping joint name -> motor ID

  //! SCServo SDK driver
  SMS_STS servo_driver_;
  bool driver_initialized_;         ///< Flag indicating successful driver connection

  // ==================== Private methods ====================

  /**
   * @brief Load calibration from YAML file.
   * @return true on success, false on error.
   * 
   * Parses the file; joint names must match keys in motor_ids_.
   * For each motor calls updateCalibrationCoefficients().
   */
  bool loadCalibration();

  /**
   * @brief Precompute coefficients for fast conversion.
   * @param motor Reference to Motor structure whose calibration will be updated.
   * 
   * Determines URDF limits based on motor.id, then computes raw_to_rad_scale etc.
   */
  void updateCalibrationCoefficients(Motor & motor);

  /**
   * @brief Read data from a single motor.
   * @param index Index in info_.joints and motors_ vectors.
   * 
   * Calls FeedBack, then ReadPos, ReadSpeed, etc., fills sensors.
   */
  void readMotorData(size_t index);

  /**
   * @brief Convert raw encoder value to radians.
   * @param raw_position Raw value (0-4095).
   * @param motor Reference to Motor containing calibration.
   * @return Angle in radians.
   * 
   * Uses precomputed coefficients from motor.calibration.
   */
  double rawToRadians(int raw_position, const Motor & motor);

  /**
   * @brief Convert radians to raw encoder value.
   * @param radians Angle in radians.
   * @param motor Reference to Motor containing calibration.
   * @return Raw value (0-4095), clipped to URDF limits.
   */
  int radiansToRaw(double radians, const Motor & motor);

  /**
   * @brief Move the robot to the park position.
   * 
   * Sets command_position from park_positions_, sends commands with half
   * speed/acceleration, then waits for movement completion (timeout 10 s).
   */
  void moveToParkPosition();

  /**
   * @brief Parse a string containing an array of numbers for park_positions.
   * @param str String like "[0.004, -1.712, ...]" or "0.004, -1.712, ..."
   * @return Vector of double numbers.
   */
  std::vector<double> parseParkPositions(const std::string & str);

  /**
   * @brief Parse a string containing an array of numbers for max_torques.
   * @param str String like "[0.004, 1.712, ...]" or "0.004, 1.712, ..."
   * @return Vector of double numbers.
   */
  std::vector<double> parseMaxTorques(const std::string & str);
};

}  // namespace soarm101_hardware_leader

#endif  // SOARM101_HARDWARE_LEADER_SOARM101_SYSTEM_HPP_
```

## Файл: `package.xml`

```xml
<?xml version="1.0"?>
<?xml-model href="http://download.ros.org/schema/package_format3.xsd" schematypens="http://www.w3.org/2001/XMLSchema"?>
<package format="3">
  <name>soarm101_hardware</name>
  <version>1.1.0</version>
  <description>Hardware interface for real SO-ARM101 via SerialPort. ROS2_control hardware component plugin.</description>
  <maintainer email="sashagrachev2005@gmail.com">banana-killer</maintainer>
  <license>Apache License 2.0</license>

  <buildtool_depend>ament_cmake_ros</buildtool_depend>

  <depend>pluginlib</depend>
  <depend>hardware_interface</depend>
  <depend>rclcpp</depend>
  <depend>rclcpp_lifecycle</depend>
  <depend>std_msgs</depend>
  <depend>scservo_sdk</depend>
  <depend>std_srvs</depend>

  <test_depend>ament_lint_auto</test_depend>
  <test_depend>ament_lint_common</test_depend>

  <export>
    <build_type>ament_cmake</build_type>
  </export>
</package>
```

## Файл: `soarm101_hardware_follower.xml`

```xml
<?xml version="1.0"?>
<library path="soarm101_hardware">
  <class name="soarm101_hardware/SOARM101SystemHardwareFollower" type="soarm101_hardware::SOARM101SystemHardwareFollower" base_class_type="hardware_interface::SystemInterface">
    <description>
      ros2_control hardware interface for follower SOARM-101 robot with Feetech servos
    </description>
  </class>
</library>
```

## Файл: `soarm101_hardware_leader.xml`

```xml
<?xml version="1.0"?>
<library path="soarm101_hardware">
  <class name="soarm101_hardware/SOARM101SystemHardwareLeader" type="soarm101_hardware::SOARM101SystemHardwareLeader" base_class_type="hardware_interface::SystemInterface">
    <description>
      ros2_control hardware interface for leader SOARM-101 robot with Feetech servos
    </description>
  </class>
</library>
```

## Файл: `src/soarm101_hardware_follower.cpp`

```cpp
// Copyright 2024 The HuggingFace Inc. team. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
//
// Copyright 2026 Alice Zenina and Alexander Grachev RTU MIREA (Russia). All rights reserved.
// Modifications to the original work are licensed under the same Apache 2.0 license.
//
// =============================================================================
// MODIFICATIONS (by Alice Zenina and Alexander Grachev, 2026-07-24)
// =============================================================================
// - Aggregated all motor data into single `Motor` struct.
// - Added park position support (`park_positions` parameter).
// - Added configurable default speed/accel (`default_speed`, `default_accel`).
// - Precomputed calibration coefficients for faster conversion.
// - Replaced dynamic vectors with std::array in write().
// - Unified read logic into `readMotorData()`.
// - Fixed motor_id vs index bug in sensor reading.
// - Changed moving_flag type to double for StateInterface compatibility.
// - Added `moveToParkPosition()` and `parseParkPositions()`.
// =============================================================================  


#include "soarm101_hardware/soarm101_hardware_follower.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <limits>
#include <memory>
#include <sstream>
#include <thread>
#include <vector>

#include "hardware_interface/types/hardware_interface_type_values.hpp"
#include "rclcpp/rclcpp.hpp"

// For convenience
using hardware_interface::HW_IF_POSITION;
using hardware_interface::HW_IF_VELOCITY;
using hardware_interface::HW_IF_EFFORT;

namespace soarm101_hardware {

SOARM101SystemHardwareFollower::SOARM101SystemHardwareFollower()
: driver_initialized_(false)
{
}

SOARM101SystemHardwareFollower::~SOARM101SystemHardwareFollower()
{
  if (driver_initialized_) {
    servo_driver_.end();
  }
}

// ----------------------------------------------------------------------------
// on_init
// ----------------------------------------------------------------------------
hardware_interface::CallbackReturn
SOARM101SystemHardwareFollower::on_init(const hardware_interface::HardwareInfo & info)
{
  if (hardware_interface::SystemInterface::on_init(info) !=
      hardware_interface::CallbackReturn::SUCCESS) {
    return hardware_interface::CallbackReturn::ERROR;
  }


  // --- Port ---
  port_ = info_.hardware_parameters["port"];
  if (port_.empty()) {
    port_ = "/dev/ttyACM0";
  }

  // --- Baudrate ---
  std::string baudrate_str = info_.hardware_parameters["baudrate"];
  if (baudrate_str.empty()) {
    baudrate_ = 1000000;
  } else {
    baudrate_ = std::stoi(baudrate_str);
  }

  // --- Calibration file ---
  calibration_file_ = info_.hardware_parameters["calibration_file"];

  // --- Default speed ---
  std::string speed_str = info_.hardware_parameters["default_speed"];
  if (!speed_str.empty()) {
    default_speed_ = static_cast<u16>(std::stoi(speed_str));
    RCLCPP_INFO(rclcpp::get_logger("SOARM101SystemHardwareFollower"), "default_speed = %d", default_speed_);
  }

  // --- Default acceleration ---
  std::string accel_str = info_.hardware_parameters["default_accel"];
  if (!accel_str.empty()) {
    default_accel_ = static_cast<u8>(std::stoi(accel_str));
    RCLCPP_INFO(rclcpp::get_logger("SOARM101SystemHardwareFollower"), "default_accel = %d", default_accel_);
  }

  // --- Park position ---
  std::string park_str = info_.hardware_parameters["park_positions"];
  if (!park_str.empty()) {
    park_positions_ = parseParkPositions(park_str);
    if (park_positions_.size() == info_.joints.size()) {
      RCLCPP_INFO(rclcpp::get_logger("SOARM101SystemHardwareFollower"), "Park positions loaded.");
    } else {
      RCLCPP_WARN(rclcpp::get_logger("SOARM101SystemHardwareFollower"),
                  "park_positions size (%zu) != joints size (%zu), ignoring",
                  park_positions_.size(), info_.joints.size());
      park_positions_.clear();
    }
  }

  // --- Max torques ---
  std::string torqs_str = info_.hardware_parameters["max_torques"];
  if (!torqs_str.empty()) {
    max_torques_ = parseMaxTorques(torqs_str);
    if (max_torques_.size() == info_.joints.size()) {
      RCLCPP_INFO(rclcpp::get_logger("SOARM101SystemHardwareFollower"), "Max torques loaded.");
    } else {
      RCLCPP_WARN(rclcpp::get_logger("SOARM101SystemHardwareFollower"),
                  "max_torques size (%zu) != joints size (%zu), ignoring",
                  max_torques_.size(), info_.joints.size());
      max_torques_.clear();
    }
  }

  // --- Name mapping ---
  motor_ids_["shoulder_pan_joint"]   = 1;
  motor_ids_["shoulder_lift_joint"]  = 2;
  motor_ids_["elbow_flex_joint"]     = 3;
  motor_ids_["wrist_flex_joint"]     = 4;
  motor_ids_["wrist_roll_joint"]     = 5;
  motor_ids_["gripper_jaw_joint"]    = 6;

  motors_.resize(info_.joints.size());

  for (size_t i = 0; i < info_.joints.size(); ++i) {
    const auto & joint = info_.joints[i];
    auto & motor = motors_[i];
    motor.id = motor_ids_[joint.name];
    motor.joint_name = joint.name;

    if (joint.parameters.find("initial_position") != joint.parameters.end()) {
      motor.sensors.position = std::stod(joint.parameters.at("initial_position"));
      motor.command_position = motor.sensors.position;
      RCLCPP_INFO(
        rclcpp::get_logger("SOARM101SystemHardwareFollower"),
        "Joint '%s' initial position set to: %.3f rad",
        joint.name.c_str(), motor.sensors.position);
    } else {
      motor.sensors.position = 0.0;
      motor.command_position = 0.0;
    }
  }

  RCLCPP_INFO(rclcpp::get_logger("SOARM101SystemHardwareFollower"), "on_init() finished successfully");
  return hardware_interface::CallbackReturn::SUCCESS;
}

// ----------------------------------------------------------------------------
// on_configure
// ----------------------------------------------------------------------------
hardware_interface::CallbackReturn
SOARM101SystemHardwareFollower::on_configure(const rclcpp_lifecycle::State & /*previous_state*/)
{
  RCLCPP_INFO(rclcpp::get_logger("SOARM101SystemHardwareFollower"), "Configuring...");

  // --- Load calibration ---
  if (!calibration_file_.empty() && !loadCalibration()) {
    RCLCPP_ERROR(
      rclcpp::get_logger("SOARM101SystemHardwareFollower"),
      "Failed to load calibration from: %s", calibration_file_.c_str());
    return hardware_interface::CallbackReturn::ERROR;
  }

  // --- Connect to bus ---
  if (!servo_driver_.begin(baudrate_, port_.c_str())) {
    RCLCPP_ERROR(
      rclcpp::get_logger("SOARM101SystemHardwareFollower"),
      "Failed to connect to motor bus on port %s.", port_.c_str());
    return hardware_interface::CallbackReturn::ERROR;
  }
  driver_initialized_ = true;

  // --- Initialise commands ---
  for (size_t i = 0; i < motors_.size(); ++i) {
    auto & motor = motors_[i];
    if (std::isnan(motor.sensors.position)) {
      motor.sensors.position = 0.0;
    }
    motor.command_position = motor.sensors.position;
  }

  for (size_t i = 0; i < motors_.size(); ++i) {
    auto & motor = motors_[i];
    if (std::isnan(motor.max_torque)) {
      motor.max_torque = max_torques_[i];
    }
    motor.max_torque = 0.0;
  }

  RCLCPP_INFO(rclcpp::get_logger("SOARM101SystemHardwareFollower"), "Configuration completed");
  return hardware_interface::CallbackReturn::SUCCESS;
}

// ----------------------------------------------------------------------------
// export_state_interfaces
// ----------------------------------------------------------------------------
std::vector<hardware_interface::StateInterface>
SOARM101SystemHardwareFollower::export_state_interfaces()
{
  std::vector<hardware_interface::StateInterface> state_interfaces;
  for (size_t i = 0; i < info_.joints.size(); ++i) {
    const auto & joint = info_.joints[i];
    auto & motor = motors_[i];

    state_interfaces.emplace_back(joint.name, HW_IF_POSITION, &motor.sensors.position);
    state_interfaces.emplace_back(joint.name, HW_IF_VELOCITY, &motor.sensors.velocity);
    state_interfaces.emplace_back(joint.name, HW_IF_EFFORT,   &motor.sensors.effort);
    state_interfaces.emplace_back(joint.name, "temperature",  &motor.sensors.temperature);
    state_interfaces.emplace_back(joint.name, "voltage",      &motor.sensors.voltage);
    state_interfaces.emplace_back(joint.name, "current",      &motor.sensors.current);
    state_interfaces.emplace_back(joint.name, "moving_flag",  &motor.sensors.moving_flag);
    state_interfaces.emplace_back(joint.name, "max_torque",  &motor.max_torque);
    state_interfaces.emplace_back(joint.name, "enable_torque",  &motor.sensors.enable_torque);
  }
  return state_interfaces;
}

// ----------------------------------------------------------------------------
// export_command_interfaces
// ----------------------------------------------------------------------------
std::vector<hardware_interface::CommandInterface>
SOARM101SystemHardwareFollower::export_command_interfaces()
{
  std::vector<hardware_interface::CommandInterface> command_interfaces;
  for (size_t i = 0; i < info_.joints.size(); ++i) {
    command_interfaces.emplace_back(
      info_.joints[i].name, HW_IF_POSITION, &motors_[i].command_position);
  }
  return command_interfaces;
}

// ----------------------------------------------------------------------------
// on_activate
// ----------------------------------------------------------------------------
hardware_interface::CallbackReturn
SOARM101SystemHardwareFollower::on_activate(const rclcpp_lifecycle::State & /*previous_state*/)
{
  RCLCPP_INFO(rclcpp::get_logger("SOARM101SystemHardwareFollower"), "Activating...");

  for (const auto & pair : motor_ids_) {
    int motor_id = pair.second;
    if (servo_driver_.EnableTorque(motor_id, ENABLE_SERVO) != 1) {
      motors_[motor_id].sensors.enable_torque = 0.0;
      RCLCPP_ERROR(
        rclcpp::get_logger("SOARM101SystemHardwareFollower"),
        "Failed to enable torque for motor %d", motor_id);
      return hardware_interface::CallbackReturn::ERROR;
    }
    else{
      motors_[motor_id].sensors.enable_torque = 1.0;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }

  RCLCPP_INFO(rclcpp::get_logger("SOARM101SystemHardwareFollower"), "Current motor positions:");
  for (size_t i = 0; i < info_.joints.size(); ++i) {
    readMotorData(i);
    motors_[i].command_position = motors_[i].sensors.position;
    RCLCPP_INFO(
      rclcpp::get_logger("SOARM101SystemHardwareFollower"),
      "  %s: %.3f rad",
      info_.joints[i].name.c_str(),
      motors_[i].sensors.position);
  }

  RCLCPP_INFO(rclcpp::get_logger("SOARM101SystemHardwareFollower"), "Activation completed");
  return hardware_interface::CallbackReturn::SUCCESS;
}

// ----------------------------------------------------------------------------
// on_deactivate
// ----------------------------------------------------------------------------
hardware_interface::CallbackReturn
SOARM101SystemHardwareFollower::on_deactivate(const rclcpp_lifecycle::State & /*previous_state*/)
{
  RCLCPP_INFO(rclcpp::get_logger("SOARM101SystemHardwareFollower"), "Deactivating...");

  // --- Park ---
  if (!park_positions_.empty()) {
    moveToParkPosition();
  } else {
    RCLCPP_INFO(rclcpp::get_logger("SOARM101SystemHardwareFollower"), "No park position set, skipping movement.");
  }

  // --- Disable torque ---
  for (const auto & pair : motor_ids_) {
    int motor_id = pair.second;
    if (servo_driver_.EnableTorque(motor_id, DISABLE_SERVO) == 0) {
      RCLCPP_ERROR(
        rclcpp::get_logger("SOARM101SystemHardwareFollower"),
        "Failed to disable torque for motor %d", motor_id);
      return hardware_interface::CallbackReturn::ERROR;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }

  RCLCPP_INFO(rclcpp::get_logger("SOARM101SystemHardwareFollower"), "Deactivation completed");
  return hardware_interface::CallbackReturn::SUCCESS;
}

// ----------------------------------------------------------------------------
// read
// ----------------------------------------------------------------------------
hardware_interface::return_type
SOARM101SystemHardwareFollower::read(const rclcpp::Time & /*time*/, const rclcpp::Duration & /*period*/)
{
  for (size_t i = 0; i < info_.joints.size(); ++i) {
    readMotorData(i);
  }
  return hardware_interface::return_type::OK;
}

// ----------------------------------------------------------------------------
// write
// ----------------------------------------------------------------------------
hardware_interface::return_type
SOARM101SystemHardwareFollower::write(const rclcpp::Time & /*time*/, const rclcpp::Duration & /*period*/)
{
  std::array<u8, NUM_MOTORS> motor_ids;
  std::array<s16, NUM_MOTORS> positions;
  std::array<u16, NUM_MOTORS> speeds;
  std::array<u8, NUM_MOTORS> accelerations;
  size_t count = 0;

  for (size_t i = 0; i < info_.joints.size(); ++i) {
    double cmd = motors_[i].command_position;
    if (!std::isnan(cmd)) {
      const auto & motor = motors_[i];
      int raw = radiansToRaw(cmd, motor);
      motor_ids[count] = static_cast<u8>(motor.id);
      positions[count] = static_cast<s16>(raw);
      speeds[count] = default_speed_;
      accelerations[count] = default_accel_;
      ++count;
    }
  }

  if (count > 0) {
    servo_driver_.SyncWritePosEx(
        motor_ids.data(), count,
        positions.data(), speeds.data(), accelerations.data());
  }
  return hardware_interface::return_type::OK;
}

// ----------------------------------------------------------------------------
// loadCalibration
// ----------------------------------------------------------------------------
bool SOARM101SystemHardwareFollower::loadCalibration()
{
  if (calibration_file_.empty()) {
    RCLCPP_INFO(rclcpp::get_logger("SOARM101SystemHardwareFollower"), "No calibration file specified, using default values.");
    return true;
  }

  RCLCPP_INFO(rclcpp::get_logger("SOARM101SystemHardwareFollower"), "Loading calibration from: %s", calibration_file_.c_str());

  std::ifstream file(calibration_file_);
  if (!file.is_open()) {
    RCLCPP_ERROR(rclcpp::get_logger("SOARM101SystemHardwareFollower"), "Failed to open calibration file: %s", calibration_file_.c_str());
    return false;
  }

  std::string line;
  std::string current_motor_name;
  MotorCalibration current_calib = {0, 0, 0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
  bool in_block = false;

  while (std::getline(file, line)) {
    size_t start = line.find_first_not_of(" \t");
    if (start == std::string::npos) continue;
    std::string trimmed = line.substr(start);
    size_t end = trimmed.find_last_not_of(" \t");
    if (end != std::string::npos) trimmed = trimmed.substr(0, end + 1);

    if (trimmed.empty() || trimmed[0] == '#') continue;

    if (trimmed.back() == ':') {
      if (!current_motor_name.empty()) {
        auto it = motor_ids_.find(current_motor_name);
        if (it != motor_ids_.end()) {
          int motor_id = it->second;
          for (auto & motor : motors_) {
            if (motor.id == motor_id) {
              motor.calibration = current_calib;
              updateCalibrationCoefficients(motor);
              RCLCPP_INFO(rclcpp::get_logger("SOARM101SystemHardwareFollower"),
                  "✅ Applied calibration for motor '%s' (ID %d): range_min=%d, range_max=%d",
                  current_motor_name.c_str(), motor_id, current_calib.range_min, current_calib.range_max);
              break;
            }
          }
        } else {
          RCLCPP_WARN(rclcpp::get_logger("SOARM101SystemHardwareFollower"),
              "⚠️ Unknown motor name '%s' in calibration file", current_motor_name.c_str());
        }
      }

      current_motor_name = trimmed.substr(0, trimmed.length() - 1);
      size_t ns = current_motor_name.find_first_not_of(" \t");
      size_t ne = current_motor_name.find_last_not_of(" \t");
      if (ns != std::string::npos && ne != std::string::npos) {
        current_motor_name = current_motor_name.substr(ns, ne - ns + 1);
      }
      current_calib = {0, 0, 0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
      in_block = true;
      continue;
    }

    if (in_block && !current_motor_name.empty()) {
      size_t colon_pos = trimmed.find(':');
      if (colon_pos != std::string::npos) {
        std::string key = trimmed.substr(0, colon_pos);
        std::string value = trimmed.substr(colon_pos + 1);
        key.erase(0, key.find_first_not_of(" \t"));
        key.erase(key.find_last_not_of(" \t") + 1);
        value.erase(0, value.find_first_not_of(" \t"));
        value.erase(value.find_last_not_of(" \t") + 1);

        if (key == "drive_mode") {
          current_calib.drive_mode = std::stoi(value);
        } else if (key == "range_min") {
          current_calib.range_min = std::stoi(value);
        } else if (key == "range_max") {
          current_calib.range_max = std::stoi(value);
        }
      }
    }
  }

  // Save the last motor
  if (!current_motor_name.empty()) {
    auto it = motor_ids_.find(current_motor_name);
    if (it != motor_ids_.end()) {
      int motor_id = it->second;
      for (auto & motor : motors_) {
        if (motor.id == motor_id) {
          motor.calibration = current_calib;
          updateCalibrationCoefficients(motor);
          RCLCPP_INFO(rclcpp::get_logger("SOARM101SystemHardwareFollower"),
              "✅ Applied calibration for motor '%s' (ID %d): range_min=%d, range_max=%d",
              current_motor_name.c_str(), motor_id, current_calib.range_min, current_calib.range_max);
          break;
        }
      }
    } else {
      RCLCPP_WARN(rclcpp::get_logger("SOARM101SystemHardwareFollower"),
          "⚠️ Unknown motor name '%s' in calibration file", current_motor_name.c_str());
    }
  }

  file.close();
  RCLCPP_INFO(rclcpp::get_logger("SOARM101SystemHardwareFollower"), "Calibration loading finished.");
  return true;
}

// ----------------------------------------------------------------------------
// updateCalibrationCoefficients
// ----------------------------------------------------------------------------
void SOARM101SystemHardwareFollower::updateCalibrationCoefficients(Motor & motor)
{
  auto & calib = motor.calibration;
  double urdf_lower, urdf_upper;
  switch (motor.id) {
    case 1:  urdf_lower = -1.91986; urdf_upper =  1.91986; break;
    case 2:  urdf_lower = -1.74533; urdf_upper =  1.74533; break;
    case 3:  urdf_lower = -1.74533; urdf_upper =  1.5708;  break;
    case 4:  urdf_lower = -1.65806; urdf_upper =  1.65806; break;
    case 5:  urdf_lower = -2.79253; urdf_upper =  2.79253; break;
    case 6:  urdf_lower = -0.1745;  urdf_upper =  1.4483;  break;
    default: urdf_lower = -M_PI;    urdf_upper =  M_PI;    break;
  }
  calib.urdf_lower = urdf_lower;
  calib.urdf_upper = urdf_upper;

  double raw_range = calib.range_max - calib.range_min;
  double urdf_range = urdf_upper - urdf_lower;
  if (raw_range != 0.0) {
    calib.raw_to_rad_scale = urdf_range / raw_range;
    calib.rad_to_raw_scale = raw_range / urdf_range;
  } else {
    calib.raw_to_rad_scale = 1.0;
    calib.rad_to_raw_scale = 1.0;
    RCLCPP_WARN(rclcpp::get_logger("SOARM101SystemHardwareFollower"),
                "Zero raw range for motor %d, using scale=1", motor.id);
  }
  calib.raw_to_rad_offset = calib.range_min;
  calib.rad_to_raw_offset = urdf_lower;
}

// ----------------------------------------------------------------------------
// readMotorData
// ----------------------------------------------------------------------------
void SOARM101SystemHardwareFollower::readMotorData(size_t index)
{
  const std::string & joint_name = info_.joints[index].name;
  int motor_id = motor_ids_[joint_name];
  auto & motor = motors_[index];

  if (servo_driver_.FeedBack(motor_id) != 0) {
    int raw_pos         = servo_driver_.ReadPos(motor_id);
    int raw_velocity    = servo_driver_.ReadSpeed(motor_id);
    int raw_effort      = servo_driver_.ReadLoad(motor_id);
    int raw_temperature = servo_driver_.ReadTemper(motor_id);
    int raw_voltage     = servo_driver_.ReadVoltage(motor_id);
    int raw_current     = servo_driver_.ReadCurrent(motor_id);
    int raw_moving_flag = servo_driver_.ReadMove(motor_id);

    motor.sensors.position = rawToRadians(raw_pos, motor);
    motor.sensors.velocity = static_cast<double>(raw_velocity) / 4096.0 * 2.0 * M_PI;
    motor.sensors.effort   = static_cast<double>(raw_effort)  / 1000.0 * motor.max_torque;
    motor.sensors.temperature = static_cast<double>(raw_temperature);
    motor.sensors.voltage  = static_cast<double>(raw_voltage) / 10.0;
    motor.sensors.current  = static_cast<double>(raw_current) / 1000.0;
    motor.sensors.moving_flag = static_cast<double>(raw_moving_flag);
  }
}

// ----------------------------------------------------------------------------
// rawToRadians
// ----------------------------------------------------------------------------
double SOARM101SystemHardwareFollower::rawToRadians(int raw_position, const Motor & motor)
{
  const auto & calib = motor.calibration;
  int clamped = std::max(calib.range_min, std::min(calib.range_max, raw_position));
  return (static_cast<double>(clamped) - calib.raw_to_rad_offset) * calib.raw_to_rad_scale + calib.urdf_lower;
}

// ----------------------------------------------------------------------------
// radiansToRaw
// ----------------------------------------------------------------------------
int SOARM101SystemHardwareFollower::radiansToRaw(double radians, const Motor & motor)
{
  const auto & calib = motor.calibration;
  double clamped = std::min(calib.urdf_upper, std::max(calib.urdf_lower, radians));
  return static_cast<int>((clamped - calib.rad_to_raw_offset) * calib.rad_to_raw_scale + calib.range_min);
}

// ----------------------------------------------------------------------------
// moveToParkPosition
// ----------------------------------------------------------------------------
void SOARM101SystemHardwareFollower::moveToParkPosition()
{
  if (park_positions_.size() != info_.joints.size()) {
    RCLCPP_WARN(rclcpp::get_logger("SOARM101SystemHardwareFollower"), 
                "Park positions size mismatch, skipping.");
    return;
  }

  RCLCPP_INFO(rclcpp::get_logger("SOARM101SystemHardwareFollower"), 
              "Moving to park position...");

  // Set commands
  for (size_t i = 0; i < info_.joints.size(); ++i) {
    motors_[i].command_position = park_positions_[i];
  }

  // Send commands with reduced speed
  const u16 speed = default_speed_ / 2;
  const u8 accel = default_accel_ / 2;

  std::array<u8, NUM_MOTORS> motor_ids;
  std::array<s16, NUM_MOTORS> positions;
  std::array<u16, NUM_MOTORS> speeds;
  std::array<u8, NUM_MOTORS> accelerations;
  size_t count = 0;

  for (size_t i = 0; i < info_.joints.size(); ++i) {
    double cmd = motors_[i].command_position;
    if (!std::isnan(cmd)) {
      const auto & motor = motors_[i];
      int raw = radiansToRaw(cmd, motor);
      motor_ids[count] = static_cast<u8>(motor.id);
      positions[count] = static_cast<s16>(raw);
      speeds[count] = speed;
      accelerations[count] = accel;
      ++count;
    }
  }

  if (count > 0) {
    servo_driver_.SyncWritePosEx(
        motor_ids.data(), count,
        positions.data(), speeds.data(), accelerations.data());
  }

  // Wait for movement completion (timeout 10 seconds)
  const int timeout_ms = 10000;
  const int sleep_ms = 50;
  int elapsed_ms = 0;
  bool all_stopped = false;

  while (elapsed_ms < timeout_ms) {
    bool moving = false;
    for (size_t i = 0; i < info_.joints.size(); ++i) {
      int motor_id = motors_[i].id;
      if (servo_driver_.FeedBack(motor_id) != 0) {
        int flag = servo_driver_.ReadMove(motor_id);
        if (flag != 0) {
          moving = true;
          break;
        }
      }
    }
    if (!moving) {
      all_stopped = true;
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(sleep_ms));
    elapsed_ms += sleep_ms;
  }

  if (all_stopped) {
    RCLCPP_INFO(rclcpp::get_logger("SOARM101SystemHardwareFollower"), "Park position reached.");
  } else {
    RCLCPP_WARN(rclcpp::get_logger("SOARM101SystemHardwareFollower"), 
                "Park position timeout after %d seconds.", timeout_ms/1000);
  }
}

// ----------------------------------------------------------------------------
// parseParkPositions
// ----------------------------------------------------------------------------
std::vector<double> SOARM101SystemHardwareFollower::parseParkPositions(const std::string & str)
{
  std::vector<double> result;
  std::string s = str;
  // Remove square brackets if present
  if (s.front() == '[' && s.back() == ']') {
    s = s.substr(1, s.length() - 2);
  }
  std::stringstream ss(s);
  std::string token;
  while (std::getline(ss, token, ',')) {
    // Remove spaces
    token.erase(0, token.find_first_not_of(" \t"));
    token.erase(token.find_last_not_of(" \t") + 1);
    if (!token.empty()) {
      try {
        result.push_back(std::stod(token));
      } catch (...) {
        RCLCPP_WARN(rclcpp::get_logger("SOARM101SystemHardwareFollower"),
                    "Failed to parse park position token: '%s'", token.c_str());
      }
    }
  }
  return result;
}

// ----------------------------------------------------------------------------
// parseMaxTorques
// ----------------------------------------------------------------------------
std::vector<double> SOARM101SystemHardwareFollower::parseMaxTorques(const std::string & str)
{
  std::vector<double> result;
  std::string s = str;
  // Remove square brackets if present
  if (s.front() == '[' && s.back() == ']') {
    s = s.substr(1, s.length() - 2);
  }
  std::stringstream ss(s);
  std::string token;
  while (std::getline(ss, token, ',')) {
    // Remove spaces
    token.erase(0, token.find_first_not_of(" \t"));
    token.erase(token.find_last_not_of(" \t") + 1);
    if (!token.empty()) {
      try {
        result.push_back(std::stod(token));
      } catch (...) {
        RCLCPP_WARN(rclcpp::get_logger("SOARM101SystemHardwareFollower"),
                    "Failed to parse max torque token: '%s'", token.c_str());
      }
    }
  }
  return result;
}

}  // namespace soarm101_hardware

#include "pluginlib/class_list_macros.hpp"
PLUGINLIB_EXPORT_CLASS(soarm101_hardware::SOARM101SystemHardwareFollowerFollower, hardware_interface::SystemInterface)
```

## Файл: `src/soarm101_hardware_leader.cpp`

```cpp
// Copyright 2024 The HuggingFace Inc. team. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
//
// Copyright 2026 Alice Zenina and Alexander Grachev RTU MIREA (Russia). All rights reserved.
// Modifications to the original work are licensed under the same Apache 2.0 license.
//
// =============================================================================
// MODIFICATIONS (by Alice Zenina and Alexander Grachev, 2026-07-24)
// =============================================================================
// - Aggregated all motor data into single `Motor` struct.
// - Added park position support (`park_positions` parameter).
// - Added configurable default speed/accel (`default_speed`, `default_accel`).
// - Precomputed calibration coefficients for faster conversion.
// - Replaced dynamic vectors with std::array in write().
// - Unified read logic into `readMotorData()`.
// - Fixed motor_id vs index bug in sensor reading.
// - Changed moving_flag type to double for StateInterface compatibility.
// - Added `moveToParkPosition()` and `parseParkPositions()`.
// =============================================================================  


#include "soarm101_hardware/soarm101_hardware_leader.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <limits>
#include <memory>
#include <sstream>
#include <thread>
#include <vector>

#include "hardware_interface/types/hardware_interface_type_values.hpp"
#include "rclcpp/rclcpp.hpp"

// For convenience
using hardware_interface::HW_IF_POSITION;
using hardware_interface::HW_IF_VELOCITY;
using hardware_interface::HW_IF_EFFORT;

namespace soarm101_hardware {

SOARM101SystemHardwareLeader::SOARM101SystemHardwareLeader()
: driver_initialized_(false)
{
}

SOARM101SystemHardwareLeader::~SOARM101SystemHardwareLeader()
{
  if (driver_initialized_) {
    servo_driver_.end();
  }
}

// ----------------------------------------------------------------------------
// on_init
// ----------------------------------------------------------------------------
hardware_interface::CallbackReturn
SOARM101SystemHardwareLeader::on_init(const hardware_interface::HardwareInfo & info)
{
  if (hardware_interface::SystemInterface::on_init(info) !=
      hardware_interface::CallbackReturn::SUCCESS) {
    return hardware_interface::CallbackReturn::ERROR;
  }

  // --- Port ---
  port_ = info_.hardware_parameters["port"];
  if (port_.empty()) {
    port_ = "/dev/ttyACM0";
  }

  // --- Baudrate ---
  std::string baudrate_str = info_.hardware_parameters["baudrate"];
  if (baudrate_str.empty()) {
    baudrate_ = 1000000;
  } else {
    baudrate_ = std::stoi(baudrate_str);
  }

  // --- Calibration file ---
  calibration_file_ = info_.hardware_parameters["calibration_file"];

  // --- Default speed ---
  std::string speed_str = info_.hardware_parameters["default_speed"];
  if (!speed_str.empty()) {
    default_speed_ = static_cast<u16>(std::stoi(speed_str));
    RCLCPP_INFO(rclcpp::get_logger("SOARM101SystemHardwareLeader"), "default_speed = %d", default_speed_);
  }

  // --- Default acceleration ---
  std::string accel_str = info_.hardware_parameters["default_accel"];
  if (!accel_str.empty()) {
    default_accel_ = static_cast<u8>(std::stoi(accel_str));
    RCLCPP_INFO(rclcpp::get_logger("SOARM101SystemHardwareLeader"), "default_accel = %d", default_accel_);
  }

  // --- Park position ---
  std::string park_str = info_.hardware_parameters["park_positions"];
  if (!park_str.empty()) {
    park_positions_ = parseParkPositions(park_str);
    if (park_positions_.size() == info_.joints.size()) {
      RCLCPP_INFO(rclcpp::get_logger("SOARM101SystemHardwareLeader"), "Park positions loaded.");
    } else {
      RCLCPP_WARN(rclcpp::get_logger("SOARM101SystemHardwareLeader"),
                  "park_positions size (%zu) != joints size (%zu), ignoring",
                  park_positions_.size(), info_.joints.size());
      park_positions_.clear();
    }
  }

  // --- Max torques ---
  std::string torqs_str = info_.hardware_parameters["max_torques"];
  if (!torqs_str.empty()) {
    max_torques_ = parseMaxTorques(torqs_str);
    if (max_torques_.size() == info_.joints.size()) {
      RCLCPP_INFO(rclcpp::get_logger("SOARM101SystemHardwareLeader"), "Max torques loaded.");
    } else {
      RCLCPP_WARN(rclcpp::get_logger("SOARM101SystemHardwareLeader"),
                  "max_torques size (%zu) != joints size (%zu), ignoring",
                  max_torques_.size(), info_.joints.size());
      max_torques_.clear();
    }
  }

  // --- Name mapping ---
  motor_ids_["shoulder_pan_joint"]   = 1;
  motor_ids_["shoulder_lift_joint"]  = 2;
  motor_ids_["elbow_flex_joint"]     = 3;
  motor_ids_["wrist_flex_joint"]     = 4;
  motor_ids_["wrist_roll_joint"]     = 5;
  motor_ids_["gripper_jaw_joint"]    = 6;

  motors_.resize(info_.joints.size());

  RCLCPP_INFO(rclcpp::get_logger("SOARM101SystemHardwareLeader"), "on_init() finished successfully");
  return hardware_interface::CallbackReturn::SUCCESS;
}

// ----------------------------------------------------------------------------
// on_configure
// ----------------------------------------------------------------------------
hardware_interface::CallbackReturn
SOARM101SystemHardwareLeader::on_configure(const rclcpp_lifecycle::State & /*previous_state*/)
{
  RCLCPP_INFO(rclcpp::get_logger("SOARM101SystemHardwareLeader"), "Configuring...");

  // --- Load calibration ---
  if (!calibration_file_.empty() && !loadCalibration()) {
    RCLCPP_ERROR(
      rclcpp::get_logger("SOARM101SystemHardwareLeader"),
      "Failed to load calibration from: %s", calibration_file_.c_str());
    return hardware_interface::CallbackReturn::ERROR;
  }

  // --- Connect to bus ---
  if (!servo_driver_.begin(baudrate_, port_.c_str())) {
    RCLCPP_ERROR(
      rclcpp::get_logger("SOARM101SystemHardwareLeader"),
      "Failed to connect to motor bus on port %s.", port_.c_str());
    return hardware_interface::CallbackReturn::ERROR;
  }
  driver_initialized_ = true;

  // --- Initialise commands ---
  for (size_t i = 0; i < motors_.size(); ++i) {
    auto & motor = motors_[i];
    if (std::isnan(motor.sensors.position)) {
      motor.sensors.position = 0.0;
    }
    motor.command_position = motor.sensors.position;
  }

  for (size_t i = 0; i < motors_.size(); ++i) {
    auto & motor = motors_[i];
    if (std::isnan(motor.max_torque)) {
      motor.max_torque = max_torques_[i];
    }
    motor.max_torque = 0.0;
  }

  RCLCPP_INFO(rclcpp::get_logger("SOARM101SystemHardwareLeader"), "Configuration completed");
  return hardware_interface::CallbackReturn::SUCCESS;
}

// ----------------------------------------------------------------------------
// export_state_interfaces
// ----------------------------------------------------------------------------
std::vector<hardware_interface::StateInterface>
SOARM101SystemHardwareLeader::export_state_interfaces()
{
  std::vector<hardware_interface::StateInterface> state_interfaces;
  for (size_t i = 0; i < info_.joints.size(); ++i) {
    const auto & joint = info_.joints[i];
    auto & motor = motors_[i];

    state_interfaces.emplace_back(joint.name, HW_IF_POSITION, &motor.sensors.position);
    state_interfaces.emplace_back(joint.name, HW_IF_VELOCITY, &motor.sensors.velocity);
    state_interfaces.emplace_back(joint.name, HW_IF_EFFORT,   &motor.sensors.effort);
    state_interfaces.emplace_back(joint.name, "temperature",  &motor.sensors.temperature);
    state_interfaces.emplace_back(joint.name, "voltage",      &motor.sensors.voltage);
    state_interfaces.emplace_back(joint.name, "current",      &motor.sensors.current);
    state_interfaces.emplace_back(joint.name, "moving_flag",  &motor.sensors.moving_flag);
    state_interfaces.emplace_back(joint.name, "max_torque",  &motor.max_torque);
    state_interfaces.emplace_back(joint.name, "enable_torque",  &motor.sensors.enable_torque);
  }
  return state_interfaces;
}

// ----------------------------------------------------------------------------
// export_command_interfaces
// ----------------------------------------------------------------------------
std::vector<hardware_interface::CommandInterface>
SOARM101SystemHardwareLeader::export_command_interfaces()
{
  std::vector<hardware_interface::CommandInterface> command_interfaces;
  for (size_t i = 0; i < info_.joints.size(); ++i) {
    command_interfaces.emplace_back(
      info_.joints[i].name, HW_IF_POSITION, &motors_[i].command_position);
  }
  return command_interfaces;
}

// ----------------------------------------------------------------------------
// on_activate
// ----------------------------------------------------------------------------
hardware_interface::CallbackReturn
SOARM101SystemHardwareLeader::on_activate(const rclcpp_lifecycle::State & /*previous_state*/)
{
  RCLCPP_INFO(rclcpp::get_logger("SOARM101SystemHardwareLeader"), "Activating...");

  RCLCPP_INFO(rclcpp::get_logger("SOARM101SystemHardwareLeader"), "Current motor positions:");
  for (size_t i = 0; i < info_.joints.size(); ++i) {
    readMotorData(i);
    motors_[i].command_position = motors_[i].sensors.position;
    RCLCPP_INFO(
      rclcpp::get_logger("SOARM101SystemHardwareLeader"),
      "  %s: %.3f rad",
      info_.joints[i].name.c_str(),
      motors_[i].sensors.position);
  }

  RCLCPP_INFO(rclcpp::get_logger("SOARM101SystemHardwareLeader"), "Activation completed");
  return hardware_interface::CallbackReturn::SUCCESS;
}

// ----------------------------------------------------------------------------
// on_deactivate
// ----------------------------------------------------------------------------
hardware_interface::CallbackReturn
SOARM101SystemHardwareLeader::on_deactivate(const rclcpp_lifecycle::State & /*previous_state*/)
{
  RCLCPP_INFO(rclcpp::get_logger("SOARM101SystemHardwareLeader"), "Deactivating...");

  // --- Park ---
  if (!park_positions_.empty()) {
    moveToParkPosition();
  } else {
    RCLCPP_INFO(rclcpp::get_logger("SOARM101SystemHardwareLeader"), "No park position set, skipping movement.");
  }

  // --- Disable torque ---
  for (const auto & pair : motor_ids_) {
    int motor_id = pair.second;
    if (servo_driver_.EnableTorque(motor_id, DISABLE_SERVO) == 0) {
      RCLCPP_ERROR(
        rclcpp::get_logger("SOARM101SystemHardwareLeader"),
        "Failed to disable torque for motor %d", motor_id);
      return hardware_interface::CallbackReturn::ERROR;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }

  RCLCPP_INFO(rclcpp::get_logger("SOARM101SystemHardwareLeader"), "Deactivation completed");
  return hardware_interface::CallbackReturn::SUCCESS;
}

// ----------------------------------------------------------------------------
// read
// ----------------------------------------------------------------------------
hardware_interface::return_type
SOARM101SystemHardwareLeader::read(const rclcpp::Time & /*time*/, const rclcpp::Duration & /*period*/)
{
  for (size_t i = 0; i < info_.joints.size(); ++i) {
    readMotorData(i);
  }
  return hardware_interface::return_type::OK;
}

// ----------------------------------------------------------------------------
// write
// ----------------------------------------------------------------------------
hardware_interface::return_type
SOARM101SystemHardwareLeader::write(const rclcpp::Time & /*time*/, const rclcpp::Duration & /*period*/)
{
  std::array<u8, NUM_MOTORS> motor_ids;
  std::array<s16, NUM_MOTORS> positions;
  std::array<u16, NUM_MOTORS> speeds;
  std::array<u8, NUM_MOTORS> accelerations;
  size_t count = 0;

  for (size_t i = 0; i < info_.joints.size(); ++i) {
    double cmd = motors_[i].command_position;
    if (!std::isnan(cmd)) {
      const auto & motor = motors_[i];
      int raw = radiansToRaw(cmd, motor);
      motor_ids[count] = static_cast<u8>(motor.id);
      positions[count] = static_cast<s16>(raw);
      speeds[count] = default_speed_;
      accelerations[count] = default_accel_;
      ++count;
    }
  }

  // if (count > 0) {
  //   servo_driver_.SyncWritePosEx(
  //       motor_ids.data(), count,
  //       positions.data(), speeds.data(), accelerations.data());
  // }
  return hardware_interface::return_type::OK;
}

// ----------------------------------------------------------------------------
// loadCalibration
// ----------------------------------------------------------------------------
bool SOARM101SystemHardwareLeader::loadCalibration()
{
  if (calibration_file_.empty()) {
    RCLCPP_INFO(rclcpp::get_logger("SOARM101SystemHardwareLeader"), "No calibration file specified, using default values.");
    return true;
  }

  RCLCPP_INFO(rclcpp::get_logger("SOARM101SystemHardwareLeader"), "Loading calibration from: %s", calibration_file_.c_str());

  std::ifstream file(calibration_file_);
  if (!file.is_open()) {
    RCLCPP_ERROR(rclcpp::get_logger("SOARM101SystemHardwareLeader"), "Failed to open calibration file: %s", calibration_file_.c_str());
    return false;
  }

  std::string line;
  std::string current_motor_name;
  MotorCalibration current_calib = {0, 0, 0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
  bool in_block = false;

  while (std::getline(file, line)) {
    size_t start = line.find_first_not_of(" \t");
    if (start == std::string::npos) continue;
    std::string trimmed = line.substr(start);
    size_t end = trimmed.find_last_not_of(" \t");
    if (end != std::string::npos) trimmed = trimmed.substr(0, end + 1);

    if (trimmed.empty() || trimmed[0] == '#') continue;

    if (trimmed.back() == ':') {
      if (!current_motor_name.empty()) {
        auto it = motor_ids_.find(current_motor_name);
        if (it != motor_ids_.end()) {
          int motor_id = it->second;
          for (auto & motor : motors_) {
            if (motor.id == motor_id) {
              motor.calibration = current_calib;
              updateCalibrationCoefficients(motor);
              RCLCPP_INFO(rclcpp::get_logger("SOARM101SystemHardwareLeader"),
                  "✅ Applied calibration for motor '%s' (ID %d): range_min=%d, range_max=%d",
                  current_motor_name.c_str(), motor_id, current_calib.range_min, current_calib.range_max);
              break;
            }
          }
        } else {
          RCLCPP_WARN(rclcpp::get_logger("SOARM101SystemHardwareLeader"),
              "⚠️ Unknown motor name '%s' in calibration file", current_motor_name.c_str());
        }
      }

      current_motor_name = trimmed.substr(0, trimmed.length() - 1);
      size_t ns = current_motor_name.find_first_not_of(" \t");
      size_t ne = current_motor_name.find_last_not_of(" \t");
      if (ns != std::string::npos && ne != std::string::npos) {
        current_motor_name = current_motor_name.substr(ns, ne - ns + 1);
      }
      current_calib = {0, 0, 0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
      in_block = true;
      continue;
    }

    if (in_block && !current_motor_name.empty()) {
      size_t colon_pos = trimmed.find(':');
      if (colon_pos != std::string::npos) {
        std::string key = trimmed.substr(0, colon_pos);
        std::string value = trimmed.substr(colon_pos + 1);
        key.erase(0, key.find_first_not_of(" \t"));
        key.erase(key.find_last_not_of(" \t") + 1);
        value.erase(0, value.find_first_not_of(" \t"));
        value.erase(value.find_last_not_of(" \t") + 1);

        if (key == "drive_mode") {
          current_calib.drive_mode = std::stoi(value);
        } else if (key == "range_min") {
          current_calib.range_min = std::stoi(value);
        } else if (key == "range_max") {
          current_calib.range_max = std::stoi(value);
        }
      }
    }
  }

  // Save the last motor
  if (!current_motor_name.empty()) {
    auto it = motor_ids_.find(current_motor_name);
    if (it != motor_ids_.end()) {
      int motor_id = it->second;
      for (auto & motor : motors_) {
        if (motor.id == motor_id) {
          motor.calibration = current_calib;
          updateCalibrationCoefficients(motor);
          RCLCPP_INFO(rclcpp::get_logger("SOARM101SystemHardwareLeader"),
              "✅ Applied calibration for motor '%s' (ID %d): range_min=%d, range_max=%d",
              current_motor_name.c_str(), motor_id, current_calib.range_min, current_calib.range_max);
          break;
        }
      }
    } else {
      RCLCPP_WARN(rclcpp::get_logger("SOARM101SystemHardwareLeader"),
          "⚠️ Unknown motor name '%s' in calibration file", current_motor_name.c_str());
    }
  }

  file.close();
  RCLCPP_INFO(rclcpp::get_logger("SOARM101SystemHardwareLeader"), "Calibration loading finished.");
  return true;
}

// ----------------------------------------------------------------------------
// updateCalibrationCoefficients
// ----------------------------------------------------------------------------
void SOARM101SystemHardwareLeader::updateCalibrationCoefficients(Motor & motor)
{
  auto & calib = motor.calibration;
  double urdf_lower, urdf_upper;
  switch (motor.id) {
    case 1:  urdf_lower = -1.91986; urdf_upper =  1.91986; break;
    case 2:  urdf_lower = -1.74533; urdf_upper =  1.74533; break;
    case 3:  urdf_lower = -1.74533; urdf_upper =  1.5708;  break;
    case 4:  urdf_lower = -1.65806; urdf_upper =  1.65806; break;
    case 5:  urdf_lower = -2.79253; urdf_upper =  2.79253; break;
    case 6:  urdf_lower = -0.1745;  urdf_upper =  1.4483;  break;
    default: urdf_lower = -M_PI;    urdf_upper =  M_PI;    break;
  }
  calib.urdf_lower = urdf_lower;
  calib.urdf_upper = urdf_upper;

  double raw_range = calib.range_max - calib.range_min;
  double urdf_range = urdf_upper - urdf_lower;
  if (raw_range != 0.0) {
    calib.raw_to_rad_scale = urdf_range / raw_range;
    calib.rad_to_raw_scale = raw_range / urdf_range;
  } else {
    calib.raw_to_rad_scale = 1.0;
    calib.rad_to_raw_scale = 1.0;
    RCLCPP_WARN(rclcpp::get_logger("SOARM101SystemHardwareLeader"),
                "Zero raw range for motor %d, using scale=1", motor.id);
  }
  calib.raw_to_rad_offset = calib.range_min;
  calib.rad_to_raw_offset = urdf_lower;
}

// ----------------------------------------------------------------------------
// readMotorData
// ----------------------------------------------------------------------------
void SOARM101SystemHardwareLeader::readMotorData(size_t index)
{
  const std::string & joint_name = info_.joints[index].name;
  int motor_id = motor_ids_[joint_name];
  auto & motor = motors_[index];

  if (servo_driver_.FeedBack(motor_id) != 0) {
    int raw_pos         = servo_driver_.ReadPos(motor_id);
    int raw_velocity    = servo_driver_.ReadSpeed(motor_id);
    int raw_effort      = servo_driver_.ReadLoad(motor_id);
    int raw_temperature = servo_driver_.ReadTemper(motor_id);
    int raw_voltage     = servo_driver_.ReadVoltage(motor_id);
    int raw_current     = servo_driver_.ReadCurrent(motor_id);
    int raw_moving_flag = servo_driver_.ReadMove(motor_id);

    motor.sensors.position = rawToRadians(raw_pos, motor);
    motor.sensors.velocity = static_cast<double>(raw_velocity) / 4096.0 * 2.0 * M_PI;
    motor.sensors.effort   = static_cast<double>(raw_effort)  / 1000.0 * motor.max_torque;
    motor.sensors.temperature = static_cast<double>(raw_temperature);
    motor.sensors.voltage  = static_cast<double>(raw_voltage) / 10.0;
    motor.sensors.current  = static_cast<double>(raw_current) / 1000.0;
    motor.sensors.moving_flag = static_cast<double>(raw_moving_flag);
  }
}

// ----------------------------------------------------------------------------
// rawToRadians
// ----------------------------------------------------------------------------
double SOARM101SystemHardwareLeader::rawToRadians(int raw_position, const Motor & motor)
{
  const auto & calib = motor.calibration;
  int clamped = std::max(calib.range_min, std::min(calib.range_max, raw_position));
  return (static_cast<double>(clamped) - calib.raw_to_rad_offset) * calib.raw_to_rad_scale + calib.urdf_lower;
}

// ----------------------------------------------------------------------------
// radiansToRaw
// ----------------------------------------------------------------------------
int SOARM101SystemHardwareLeader::radiansToRaw(double radians, const Motor & motor)
{
  const auto & calib = motor.calibration;
  double clamped = std::min(calib.urdf_upper, std::max(calib.urdf_lower, radians));
  return static_cast<int>((clamped - calib.rad_to_raw_offset) * calib.rad_to_raw_scale + calib.range_min);
}

// ----------------------------------------------------------------------------
// moveToParkPosition
// ----------------------------------------------------------------------------
void SOARM101SystemHardwareLeader::moveToParkPosition()
{
  if (park_positions_.size() != info_.joints.size()) {
    RCLCPP_WARN(rclcpp::get_logger("SOARM101SystemHardwareLeader"), 
                "Park positions size mismatch, skipping.");
    return;
  }

  RCLCPP_INFO(rclcpp::get_logger("SOARM101SystemHardwareLeader"), 
              "Moving to park position...");

  // Set commands
  for (size_t i = 0; i < info_.joints.size(); ++i) {
    motors_[i].command_position = park_positions_[i];
  }

  // Send commands with reduced speed
  const u16 speed = default_speed_ / 2;
  const u8 accel = default_accel_ / 2;

  std::array<u8, NUM_MOTORS> motor_ids;
  std::array<s16, NUM_MOTORS> positions;
  std::array<u16, NUM_MOTORS> speeds;
  std::array<u8, NUM_MOTORS> accelerations;
  size_t count = 0;

  for (size_t i = 0; i < info_.joints.size(); ++i) {
    double cmd = motors_[i].command_position;
    if (!std::isnan(cmd)) {
      const auto & motor = motors_[i];
      int raw = radiansToRaw(cmd, motor);
      motor_ids[count] = static_cast<u8>(motor.id);
      positions[count] = static_cast<s16>(raw);
      speeds[count] = speed;
      accelerations[count] = accel;
      ++count;
    }
  }

  if (count > 0) {
    servo_driver_.SyncWritePosEx(
        motor_ids.data(), count,
        positions.data(), speeds.data(), accelerations.data());
  }

  // Wait for movement completion (timeout 10 seconds)
  const int timeout_ms = 10000;
  const int sleep_ms = 50;
  int elapsed_ms = 0;
  bool all_stopped = false;

  while (elapsed_ms < timeout_ms) {
    bool moving = false;
    for (size_t i = 0; i < info_.joints.size(); ++i) {
      int motor_id = motors_[i].id;
      if (servo_driver_.FeedBack(motor_id) != 0) {
        int flag = servo_driver_.ReadMove(motor_id);
        if (flag != 0) {
          moving = true;
          break;
        }
      }
    }
    if (!moving) {
      all_stopped = true;
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(sleep_ms));
    elapsed_ms += sleep_ms;
  }

  if (all_stopped) {
    RCLCPP_INFO(rclcpp::get_logger("SOARM101SystemHardwareLeader"), "Park position reached.");
  } else {
    RCLCPP_WARN(rclcpp::get_logger("SOARM101SystemHardwareLeader"), 
                "Park position timeout after %d seconds.", timeout_ms/1000);
  }
}

// ----------------------------------------------------------------------------
// parseParkPositions
// ----------------------------------------------------------------------------
std::vector<double> SOARM101SystemHardwareLeader::parseParkPositions(const std::string & str)
{
  std::vector<double> result;
  std::string s = str;
  // Remove square brackets if present
  if (s.front() == '[' && s.back() == ']') {
    s = s.substr(1, s.length() - 2);
  }
  std::stringstream ss(s);
  std::string token;
  while (std::getline(ss, token, ',')) {
    // Remove spaces
    token.erase(0, token.find_first_not_of(" \t"));
    token.erase(token.find_last_not_of(" \t") + 1);
    if (!token.empty()) {
      try {
        result.push_back(std::stod(token));
      } catch (...) {
        RCLCPP_WARN(rclcpp::get_logger("SOARM101SystemHardwareLeader"),
                    "Failed to parse park position token: '%s'", token.c_str());
      }
    }
  }
  return result;
}

// ----------------------------------------------------------------------------
// parseMaxTorques
// ----------------------------------------------------------------------------
std::vector<double> SOARM101SystemHardwareLeader::parseMaxTorques(const std::string & str)
{
  std::vector<double> result;
  std::string s = str;
  // Remove square brackets if present
  if (s.front() == '[' && s.back() == ']') {
    s = s.substr(1, s.length() - 2);
  }
  std::stringstream ss(s);
  std::string token;
  while (std::getline(ss, token, ',')) {
    // Remove spaces
    token.erase(0, token.find_first_not_of(" \t"));
    token.erase(token.find_last_not_of(" \t") + 1);
    if (!token.empty()) {
      try {
        result.push_back(std::stod(token));
      } catch (...) {
        RCLCPP_WARN(rclcpp::get_logger("SOARM101SystemHardwareLeader"),
                    "Failed to parse max torque token: '%s'", token.c_str());
      }
    }
  }
  return result;
}

}  // namespace soarm101_hardware_leader

#include "pluginlib/class_list_macros.hpp"
PLUGINLIB_EXPORT_CLASS(soarm101_hardware::SOARM101SystemHardwareLeader, hardware_interface::SystemInterface)
```

