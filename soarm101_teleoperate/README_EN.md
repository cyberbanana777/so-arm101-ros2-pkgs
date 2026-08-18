# soarm101_teleoperate

Package for teleoperating the **SOARM101** robotic arm: the follower arm repeats the movements of the leader arm. It launches both arms, relays joint and gripper positions, publishes static transforms and markers for RViz, and allows simultaneous visualization of the leader and follower arms in RViz.

---

## Purpose

- Launch two SOARM101 arms simultaneously: `leader` — read-only, `follower` — control.
- Relay joint positions from the leader to the follower via `JointTrajectory`.
- Relay the leader's gripper position to the follower's gripper via the `GripperCommand` action.
- Publish static transforms from `world_frame` to `leader/zero_point_link` and `follower/zero_point_link`.
- Publish markers for visually identifying the arms in RViz.

---

## Architecture and key features

The package contains four executable nodes and one launch file.

### Nodes

- **`arm_relay`** — subscribes to `/leader/soarm101_telemetry_controller/motor_states`, filters out ignored joints, and publishes `JointTrajectory` to `/follower/joint_trajectory_controller/joint_trajectory`.
- **`gripper_relay`** — subscribes to the same leader telemetry, finds the `gripper_jaw_joint`, and sends a goal via the `/follower/gripper_controller/gripper_cmd` action.
- **`marker_publisher`** — reads `config/dual_arm_setup.yaml` and publishes a `MarkerArray` to `/markers` with text labels and "diamonds" for each arm.
- **`static_transform_publisher_world_to_arms`** — publishes static transforms from `world_frame` to `leader/zero_point_link` and `follower/zero_point_link` based on the YAML configuration.

### Launch file

`teleoperate.launch.py`:
- includes `soarm101_bringup` twice — for the leader and the follower;
- launches the static transform, marker, `arm_relay`, and `gripper_relay` nodes;
- launches RViz with the `dual_arm.rviz` configuration.

### Configuration

The `config/dual_arm_setup.yaml` file contains:
- the position of `zero_point_link` for each arm in the world;
- marker parameters (text, color, height, shape).

---

## Configuration parameters

### Launch arguments

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `leader_port` | string | `/dev/soarm101_leader` | Serial port for the leader |
| `follower_port` | string | `/dev/soarm101_follower` | Serial port for the follower |

### `arm_relay` node parameters

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `command_timeout` | double | `0.0015` | Time to reach the target point, sec |
| `publish_rate` | double | `50.0` | Maximum publishing rate, Hz |
| `ignored_joints` | list of strings | `['gripper_jaw_joint']` | Joints that are not relayed |

### `gripper_relay` node parameters

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `max_effort` | double | `10.0` | Maximum gripper effort |
| `joint_name` | string | `gripper_jaw_joint` | Gripper joint name |

### YAML configuration `dual_arm_setup.yaml`

| Section | Key | Type | Description |
|---------|-----|------|-------------|
| `world.<arm>` | `translation.x/y/z` | float | Arm position in the world, m |
| `world.<arm>` | `rotation.roll/pitch/yaw` | float | Arm orientation, rad |
| `markers.<arm>` | `text` | string | Marker text |
| `markers.<arm>` | `color` | [r,g,b,a] | Marker color |
| `markers.<arm>` | `offset_z` | float | Marker height above `zero_point_link`, m |
| `markers.<arm>` | `shape` | string | Marker shape (`diamond` or other) |
| `markers.<arm>` | `diamond_size` | float | Base size of the diamond |

---

## Usage

### Starting teleoperation

```bash
source install/setup.bash
ros2 launch soarm101_teleoperate teleoperate.launch.py
```

With specified ports:

```bash
ros2 launch soarm101_teleoperate teleoperate.launch.py \
  leader_port:=/dev/soarm101_leader \
  follower_port:=/dev/soarm101_follower
```

After launch:
- RViz opens with two arm models;
- the leader arm publishes telemetry;
- `arm_relay` forwards joint positions to the follower;
- `gripper_relay` forwards the gripper position.

### Useful checks

```bash
# Leader telemetry
ros2 topic echo /leader/soarm101_telemetry_controller/motor_states

# Follower command topic
ros2 topic echo /follower/joint_trajectory_controller/joint_trajectory

# Markers
ros2 topic echo /markers
```

---

## Dependencies

- **ROS 2 Humble:**
  - `rclpy`
  - `soarm101_bringup`
  - `soarm101_interfaces`
  - `trajectory_msgs`
  - `control_msgs`
  - `visualization_msgs`
  - `geometry_msgs`
  - `tf2_ros`
  - `tf_transformations`
  - `std_msgs`
  - `rviz2`
- **Python:**
  - `PyYAML`

---

## License

The package is distributed under the **MIT** license (see the `LICENSE` file in the package root).

---

## Version

**1.0.0** — package released.

---

## Support

Questions and suggestions should be submitted via [Issues](https://github.com/cyberbanana777/so-arm101-ros2-pkgs/issues).