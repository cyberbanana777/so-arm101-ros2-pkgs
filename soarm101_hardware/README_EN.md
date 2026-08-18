# soarm101_hardware

The package implements hardware interfaces (`hardware_interface::SystemInterface` plugins) for the SOARM101 robot in the **ros2_control** ecosystem. It is a “bridge” between the hardware (Feetech servos) and software control (ROS 2 controllers).

It provides two classes:

- **`SOARM101SystemHardwareLeader`** – read-only state (no control). Used for the leader arm in teleoperation.
- **`SOARM101SystemHardwareFollower`** – full position control with feedback. Used for the follower arm.

Both classes work with Feetech servos (SMS/STS series) over a serial port, load calibration from YAML, and publish extended telemetry.

---

## Package purpose

- Connect to the real SOARM101 robot via USB/UART.
- Read telemetry: position, velocity, effort, temperature, voltage, current, motion flag.
- Position control (only for **follower**) with configurable velocity and acceleration.
- Return to parking position on shutdown.
- Export interfaces for `ros2_control`:
  - **State**: `position`, `velocity`, `effort`, `temperature`, `voltage`, `current`, `moving_flag`, `max_torque`, `enable_torque`.
  - **Command (follower only)**: `position`.

---

## How it relates to ros2_control

`ros2_control` is the standard ROS 2 framework for robot control. It requires a **hardware plugin** to be written for each robot that can communicate with the real hardware. The plugin exports:

- **State interfaces** – variables that controllers can read (for example, the current joint angle).
- **Command interfaces** – variables that controllers can write (for example, the desired joint angle).

This package is such a plugin. It is registered with `controller_manager` and allows standard controllers (for example, `joint_state_broadcaster` or `joint_trajectory_controller`) to work with SOARM101 arms.

- **Leader** (leader arm) exports only state interfaces → it can be connected to `joint_state_broadcaster` to visualize positions in RViz, but it cannot be controlled through ros2_control.
- **Follower** (follower arm) exports both state and command interfaces → it can be connected to a motion controller that will set target positions.

---

## Architecture

Both classes inherit from `SystemInterface`. Internally, an `SMS_STS` object from the `scservo_sdk` library is used to communicate with the servos over a serial port.

### Main code components

- **`Motor`** – a structure that aggregates everything related to a single motor: ID, joint name, current command, sensor readings, calibration coefficients, maximum torque.
- **Calibration** – loaded from a YAML file (via `yaml-cpp`). For each joint, `range_min` and `range_max` are defined: the raw encoder values corresponding to the extreme angles from the URDF. Based on this data, coefficients are precomputed for fast conversion “raw value ↔ radians”.
- **Parameters** – configured through the `<ros2_control>` section in the xacro file (see the table below).
- **Parking** – on deactivation, if `park_positions` are specified, the robot smoothly moves to the given position at half speed and acceleration, and then torque is disabled.
- **Synchronous write (follower)** – commands to all motors are sent in a single `SyncWritePosEx` packet so that they start moving simultaneously.

### Plugin lifecycle

The plugin is managed by `controller_manager` and passes through the standard stages:

1. **`on_init()`** – reads parameters from the URDF, creates `Motor` structures, maps joint names to motor IDs (1–6). If `initial_position` is specified, it is used as the starting value.
2. **`on_configure()`** – loads calibration from YAML, opens the port, initializes the `SMS_STS` driver, sets maximum torques.
3. **`export_state_interfaces()` / `export_command_interfaces()`** – register pointers to the variables through which controllers will communicate with the plugin.
4. **`on_activate()`** – for the follower, enables torque on the motors and reads current positions (so that the command starts from the actual position). The leader does not enable torque.
5. **Cyclic calls to `read()` and `write()`** – executed at each control step:
   - `read()` polls each servo, obtains raw data (position, velocity, load, temperature, voltage, current, motion flag) and converts them to physical quantities.
   - `write()` (follower only) takes target positions from the command interface, converts them to raw values, and sends them to the motors synchronously.
6. **`on_deactivate()`** – if necessary, performs parking, then disables torque on all motors.

---

## Configuration parameters (in `ros2_control`)

In the `ros2_control` description file (usually xacro), the following must be specified:

| Parameter | Type | Description | Example |
|----------|-----|----------|--------|
| `port` | string | Serial port path | `/dev/ttyACM0` |
| `baudrate` | int | Baud rate (usually 1000000) | `1000000` |
| `calibration_file` | string | Path to the YAML calibration file | `$(find converter_calibration_data)/config/leader_motor_calibration.yaml` |
| `default_speed` | int | Default movement speed (0–3400) | `2400` |
| `default_accel` | int | Default acceleration (0–254) | `50` |
| `park_positions` | list of double | Parking position in radians (joint order) | `[0.0, 0.0, 0.0, 0.0, 0.0, 0.0]` |
| `max_torques` | list of double | Maximum torques for each joint (N·m) | `[2.69, 1.91, 2.69, 1.41, 1.41, 1.41]` |

**Important:** `calibration_file` is required for a real robot. Without it, the conversion coefficients will be 1.0, and raw encoder values will be incorrectly interpreted as radians.

---

## Usage

The package is loaded automatically through `controller_manager`. In the `soarm101_bringup` launch files, the plugin is selected via the `arm_type` argument:

- `arm_type:="leader"` – `soarm101_hardware/SOARM101SystemHardwareLeader` is loaded (read-only).
- `arm_type:="follower"` – `soarm101_hardware/SOARM101SystemHardwareFollower` is loaded (control).

Example xacro fragment (in `ros2_control_real.xacro`):

```xml
<hardware>
  <plugin>soarm101_hardware/SOARM101SystemHardwareLeader</plugin>
  <param name="port">/dev/ttyACM0</param>
  <param name="calibration_file">$(find converter_calibration_data)/config/leader_motor_calibration.yaml</param>
  <!-- ... -->
</hardware>
```