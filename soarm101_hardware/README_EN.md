# soarm101_hardware

The package implements hardware interfaces (`hardware_interface::SystemInterface` plugins) for the SOARM101 robot within the **ros2_control** ecosystem. It provides two classes:

- **`SOARM101SystemHardwareLeader`** – read‑only state (no control). Used for the leader arm in teleoperation.
- **`SOARM101SystemHardwareFollower`** – full position control with feedback. Used for the follower arm.

Both classes work with Feetech servos (SMS/STS series) over a serial port, load calibration from YAML, and publish extended telemetry.

---

## Purpose

- Connection to the real SOARM101 robot via USB/UART.
- Telemetry reading: position, velocity, effort, temperature, voltage, current, moving flag.
- Position control (only for **follower**) with configurable speed and acceleration.
- Support for a park position on deactivation.
- Export of interfaces for `ros2_control`:
  - **State**: `position`, `velocity`, `effort`, `temperature`, `voltage`, `current`, `moving_flag`, `max_torque`, `enable_torque`.
  - **Command (follower only)**: `position`.

---

## Architecture

Both classes inherit from `SystemInterface`. Internally, they use an `SMS_STS` object from `scservo_sdk` to communicate with the servos.

**Main components:**

- **`Motor`** – an aggregated structure for each motor (ID, name, command, sensor readings, calibration coefficients, maximum torque).
- **Calibration** – loaded from a YAML file using `yaml-cpp`. Specifies `range_min`/`range_max` for each joint. Based on these values and the URDF limits, coefficients are precomputed for fast raw ↔ radian conversion.
- **Parameters** – configurable via `ros2_control` `hardware_parameters`.
- **Parking** – on deactivation, the robot moves to a specified position (with half speed/acceleration), then torque is disabled.
- **Synchronous write (follower)** – commands are sent to all motors with a single `SyncWritePosEx` for minimal latency.

**Differences between Leader and Follower:**
- The Leader **does not export command interfaces** (`export_command_interfaces()` returns an empty vector), so control controllers (e.g., `joint_trajectory_controller`) cannot be loaded for it.
- The Leader **does not send commands** – its `write()` method is empty.
- The Leader may have its own calibration file if required.


**Important notes about the Leader:**

- Since the Leader **does not control the motors** (does not send commands), its `effort` and `moving_flag` states are **always 0**. This is expected behaviour, as the leader motors do not receive movement commands via ros2_control.
- The leader's actual effort and moving flag values are not used — these fields remain zero.

---

## Configuration Parameters (in `ros2_control`)

In the `ros2_control` description file, the following parameters must be specified:

| Parameter | Type | Description | Example |
|-----------|------|-------------|---------|
| `port` | string | Path to the serial port | `/dev/ttyACM0` |
| `baudrate` | int | Baud rate (typically 1000000) | `1000000` |
| `calibration_file` | string | Path to the YAML calibration file | `$(find-pkg-prefix converter_calibration_data)/config/leader_motor_calibration.yaml` |
| `default_speed` | int | Default movement speed (0–3400) | `2400` |
| `default_accel` | int | Default acceleration (0–254) | `50` |
| `park_positions` | list of double | Park position in radians (order of joints) | `[0.0, 0.0, 0.0, 0.0, 0.0, 0.0]` |
| `max_torques` | list of double | Maximum torques for each joint (N·m) | `[2.69, 1.91, 2.69, 1.41, 1.41, 1.41]` |

---

## Usage

The package is automatically loaded via `controller_manager`. In the `soarm101_bringup` launch files, the plugin selection is made via the `arm_type` argument:

- `arm_type:="leader"` – loads `soarm101_hardware/SOARM101SystemHardwareLeader` (read‑only).
- `arm_type:="follower"` – loads `soarm101_hardware/SOARM101SystemHardwareFollower` (control).

Example xacro snippet (in `ros2_control_real.xacro`):
```xml
<hardware>
  <plugin>soarm101_hardware/SOARM101SystemHardwareLeader</plugin>
  <param name="port">/dev/ttyACM0</param>
  <param name="calibration_file">$(find converter_calibration_data)/config/leader_motor_calibration.yaml</param>
  <!-- ... -->
</hardware>
```

After `ros2_control` activation, the interfaces become available to controllers.

---

## Dependencies

- `hardware_interface`, `pluginlib`, `rclcpp`, `rclcpp_lifecycle`, `std_srvs`
- `scservo_sdk` – SDK for Feetech servo communication.
- `yaml-cpp` – calibration parsing.

---

## License

The source code is based on developments by HuggingFace Inc. (Apache‑2.0) and supplemented with modifications by the authors (Alice Zenina, Alexander Grachev, RTU MIREA, 2026).  
The package is distributed under **Apache‑2.0** (see [LICENSE](LICENSE)).

---

## Version

**2.0.0** – architectural changes: split into leader/follower, leader without command interfaces, yaml-cpp support, fixed conversion and calibration errors.

---

## Support

For questions and suggestions, please use [Issues](https://github.com/cyberbanana777/so-arm101-ros2-pkgs/issues).
