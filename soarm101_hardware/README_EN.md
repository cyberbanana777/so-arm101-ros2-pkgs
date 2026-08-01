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