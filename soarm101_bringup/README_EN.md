# soarm101_bringup

An aggregator package that provides a **single launch point** for the entire SOARM101 robot software stack. It combines the launch of `robot_state_publisher`, `ros2_control_node` (or Gazebo), and all required controllers depending on the selected mode: **simulation** or **real robot**, as well as the arm type (**leader** or **follower**).

---

## Purpose

- Provide a single entry point for launching the entire system.
- Automatically select the `ros2_control` configuration (real hardware or Gazebo) based on the `use_sim` argument.
- Automatically select the hardware plugin (leader or follower) based on the `arm_type` argument.
- Load and activate controllers selectively:
  - `joint_state_broadcaster` – always.
  - `joint_trajectory_controller` and `gripper_controller` – only for follower.
  - `soarm101_telemetry_controller` – only for the real robot.
- Pass parameters (port, speed, acceleration) through launch arguments to xacro.
- Process the xacro file, substituting the required includes for `ros2_control` and resolving mesh paths (replacing `package://` with absolute paths).

---

## Launch file: `bringup.launch.py`

### Arguments

| Argument | Type | Default | Description |
|----------|------|---------|-------------|
| `use_sim` | bool | `false` | `true` – launch in Gazebo, `false` – real robot |
| `arm_type` | string | `follower` | Arm type: `follower` (controlled) or `leader` (read-only) |
| `port` | string | `/dev/ttyACM0` | Serial port for connecting to the motors |
| `max_speed` | int | `2400` | Maximum speed (0–3400) for each joint |
| `max_accel` | int | `50` | Maximum acceleration (0–254) for each joint |

### Operation logic

1. **xacro processing** – reads `urdf/full.xacro`, passes all arguments to it (`use_sim`, `arm_type`, `port`, `max_speed`, `max_accel`), and obtains the complete URDF string.
2. **Replacing `package://`** – all paths of the form `package://soarm101_description/meshes/...` are replaced with absolute paths prefixed with `file://`, so that Gazebo and other components can load the meshes.
3. **Launching `robot_state_publisher`** – publishes `<arm_prefix>/robot_description` and broadcasts `tf`, where `arm_prefix` can be either `leader` or `follower`.
4. **Branching based on `use_sim`**:

#### Simulation mode (`use_sim:=true`)
- Launches **Gazebo** via `ros_gz_sim` (empty world).
- Spawns the robot via `ros_gz_sim create`.
- Loads controllers from `sim_controllers.yaml`:
  - `joint_state_broadcaster` – always.
  - `joint_trajectory_controller` – only for follower.
  - `gripper_controller` – only for follower.

#### Real robot mode (`use_sim:=false`)
- Launches **`ros2_control_node`** with parameters:
  - `robot_description` (processed URDF)
  - `real_controllers.yaml`
- Loads controllers via spawner:
  - `joint_state_broadcaster` – always.
  - `soarm101_telemetry_controller` – always.
  - `joint_trajectory_controller` – only for follower.
  - `gripper_controller` – only for follower.

---

## Full URDF: `urdf/full.xacro`

The file includes:
- The main robot URDF (`soarm101_description/urdf/soarm101.xacro`).
- Macros from `soarm101_ros2_control/urdf/ros2_control_real.xacro`.
- Logic for selecting the hardware plugin:
  - **Simulation** – includes `ros2_control_sim.xacro` (the `gz_ros2_control` plugin).
  - **Real robot + leader** – calls the `soarm101_hardware_leader` macro.
  - **Real robot + follower** – calls the `soarm101_hardware_follower` macro.

This allows using the same robot description for different modes and arm types, changing only the low-level hardware interface.

---

## Usage

### Launching the real robot (follower)

```bash
ros2 launch soarm101_bringup bringup.launch.py use_sim:=false arm_type:=follower
```

After launch:
- Connects to the motor bus (port and speed are set by arguments).
- Activates all controllers (including control controllers).
- The robot is ready to receive trajectories via `joint_trajectory_controller`.

### Launching the real robot (leader – read-only)

```bash
ros2 launch soarm101_bringup bringup.launch.py use_sim:=false arm_type:=leader
```

After launch:
- Connects to the motor bus (read-only).
- Control controllers are **not loaded**.
- Only telemetry is available (position, velocity, temperature, voltage, current).

### Launching simulation in Gazebo

```bash
ros2 launch soarm101_bringup bringup.launch.py use_sim:=true arm_type:=follower
```

After launch:
- A Gazebo window opens with an empty world.
- The robot appears in the world at the initial position.
- Controllers operate in simulation mode.

### Changing port, speed, acceleration parameters

```bash
ros2 launch soarm101_bringup bringup.launch.py \
    use_sim:=false \
    arm_type:=follower \
    port:=/dev/ttyUSB0 \
    max_speed:=2000 \
    max_accel:=40
```

---

## Dependencies

- `robot_state_publisher` – publishes robot state.
- `controller_manager` – controller management.
- `soarm101_description` – URDF description.
- `soarm101_hardware` – hardware plugins for the real robot.
- `soarm101_ros2_control` – `ros2_control` configurations.
- `soarm101_telemetry_controller` – custom telemetry controller.

For simulation, the following are additionally required:
- `ros_gz_sim` – Gazebo-ROS2 integration.
- `gz_ros2_control` – control plugin in Gazebo.

---

## Notes

- Control controllers (`joint_trajectory_controller`, `gripper_controller`) are loaded **only** for `arm_type == follower`.
- For `leader`, only state and telemetry are available.
- All parameters (port, speed, acceleration) are passed through launch arguments.

---

## License

The package is distributed under the **MIT** license (see the [LICENSE](LICENSE) file in the package root).

---

## Version

**3.0.0** – added prefixes to node and topic names (`leader`, `follower`)
**2.0.0** – added leader/follower support, new launch arguments, conditional controller launching.

---

## Support

Questions and suggestions should be submitted via [Issues](https://github.com/cyberbanana777/so-arm101-ros2-pkgs/issues).