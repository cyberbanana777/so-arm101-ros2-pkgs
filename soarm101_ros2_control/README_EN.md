# soarm101_ros2_control

The package contains configuration files for **ros2_control** – both for the real SOARM101 robot (using hardware plugins from `soarm101_hardware`) and for Gazebo simulation (using `gz_ros2_control`). It includes xacro macros that are included in the main URDF description to substitute the correct hardware plugin depending on the launch mode and arm type (leader/follower).

---

## Package contents

- **`config/`** – YAML controller configurations:
  - `real_controllers.yaml` – for working with the real robot (100 Hz, `use_sim_time` disabled). Controllers loaded: `joint_state_broadcaster`, `joint_trajectory_controller` (follower only), `gripper_controller` (follower only), `soarm101_telemetry_controller`.
  - `sim_controllers.yaml` – for Gazebo simulation (50 Hz, `use_sim_time: true`). Loads `joint_state_broadcaster`, `joint_trajectory_controller`, `gripper_controller`.

- **`urdf/`** – xacro macros included in the main URDF:
  - **`ros2_control_real.xacro`** – contains macros for the real robot:
    - `joint_servo_with_command` – declares command and state interfaces for the follower.
    - `joint_servo_without_command` – declares only state interfaces for the leader (no command).
    - `soarm101_hardware_leader` – macro for loading the `SOARM101SystemHardwareLeader` plugin with parameters (port, speed, calibration, parking, max_torques).
    - `soarm101_hardware_follower` – macro for loading the `SOARM101SystemHardwareFollower` plugin with parameters.
  - **`ros2_control_sim.xacro`** – macro for Gazebo simulation with the `gz_ros2_control/GazeboSimSystem` plugin. Exports only position and velocity.

- **`IMPROVEMENTS_SIM.md`** – document with recommendations for improving the simulation configuration: adding PID controllers, extending command interfaces, mimic joints, etc.

---

## Usage

**Important!** These configuration files are intended to be used through the `soarm101_bringup` package, which automatically selects the correct macro based on the `arm_type` argument. If you use them separately, follow the instructions below.

### For the real robot

1. Make sure the calibration files are created by the `converter_calibration_data` package and are located at the paths specified in the macros:
   - For the leader: `leader_motor_calibration.yaml`
   - For the follower: `follower_motor_calibration.yaml`

2. In the main URDF (for example, `full.xacro` from `soarm101_bringup`), include the required macro depending on `arm_type`:
   ```xml
   <xacro:if value="$(arg arm_type) == 'leader'">
     <xacro:soarm101_hardware_leader port="$(arg port)" max_speed="$(arg max_speed)" max_accel="$(arg max_accel)"/>
   </xacro:if>
   <xacro:if value="$(arg arm_type) == 'follower'">
     <xacro:soarm101_hardware_follower port="$(arg port)" max_speed="$(arg max_speed)" max_accel="$(arg max_accel)"/>
   </xacro:if>
   ```

3. When starting `ros2_control_node`, pass parameters from `real_controllers.yaml`:
   ```bash
   ros2 run controller_manager ros2_control_node \
     --ros-args --params-file $(find soarm101_ros2_control)/config/real_controllers.yaml \
     -p robot_description:=$(cat $(find soarm101_description)/urdf/soarm101.xacro)
   ```

### For Gazebo simulation

1. In the main URDF, include the simulation macro:
   ```xml
   <xacro:include filename="$(find soarm101_ros2_control)/urdf/ros2_control_sim.xacro"/>
   ```

2. Launch Gazebo with the loaded model and controller manager using `sim_controllers.yaml`. Don't forget to set `use_sim_time:=true`.

---

## Parameters set in xacro for the real robot

| Parameter | Type | Description | Example |
|----------|-----|----------|--------|
| `port` | string | Serial port for connecting to the motors | `/dev/ttyACM0` |
| `max_speed` | int | Maximum speed (0–3400) | `2400` |
| `max_accel` | int | Maximum acceleration (0–254) | `50` |
| `park_positions` | list of double | Parking position in radians (joint order) | `[0.004, -1.712, 1.560, 1.141, -0.029, 0.461]` |
| `max_torques` | list of double | Maximum torques for each joint (N·m) | `[2.94, 2.94, 2.94, 2.94, 2.94, 2.94]` |
| `calibration_file` | string | Path to the YAML calibration file | `$(find converter_calibration_data)/config/leader_motor_calibration.yaml` |

**Note:** different calibration files and different `max_torques` values are used for the leader and the follower.

---

## Controllers used in the configurations

- **`joint_state_broadcaster`** – publishes `/joint_states` for visualization and other nodes (always enabled).
- **`joint_trajectory_controller`** – the main controller for executing trajectories (enabled only for the follower).
- **`gripper_controller`** – controller for controlling the gripper (enabled only for the follower).
- **`soarm101_telemetry_controller`** – custom controller for publishing extended telemetry (always enabled for the real robot). In version 2.1.0, support for the `max_torque` and `enable_torque` interfaces was added, allowing monitoring of the maximum torque and torque-enable state for each joint.

---

## Important notes

### For the leader (without control)
- **Command interfaces are not exported** – the `joint_servo_without_command` macro does not contain `<command_interface>` tags. Therefore, control controllers (for example, `joint_trajectory_controller`) **cannot be loaded** for the leader. This is expected behavior.
- **`effort` and `moving_flag` are always 0** – since the leader's motors do not receive commands through ros2_control, these fields remain zero.
- The leader is used only for reading state (telemetry).

### For the follower (with control)
- All interfaces are exported (position, velocity, effort, temperature, voltage, current, moving_flag, max_torque, enable_torque) and the command interface (position).
- Control controllers are loaded and active.

---

## Dependencies

- `soarm101_hardware` – hardware plugins for the real robot.
- `soarm101_description` – URDF description of the robot.
- `soarm101_telemetry_controller` – telemetry controller (version >= 1.1.0 that supports the new interfaces).
- (For simulation) `gz_ros2_control` – plugin for Gazebo.

---

## License

The package is distributed under the **MIT** license (see the [LICENSE](LICENSE) file in the package root).

---

## Version

**3.0.0** – changed the structure of the real robot config. Added separation by prefix, i.e. there is a separate config for `leader` and a separate one for `follower`.

**2.1.0** – added support for the `max_torque` and `enable_torque` interfaces in the real robot configurations; updated xacro macros and the telemetry controller YAML configuration.

---

## Support

Questions and suggestions should be submitted via [Issues](https://github.com/cyberbanana777/so-arm101-ros2-pkgs/issues).