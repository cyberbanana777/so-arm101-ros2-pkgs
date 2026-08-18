# soarm101_examples

The package contains example nodes for controlling the SOARM101 robot through standard ROS2 action interfaces. It is intended for demonstrating the stack's capabilities, testing controllers, and quickly debugging robot motions.

---

## Purpose

- Provide ready-made examples for sending motion trajectories to the robot's joints.
- Show how to interact with `joint_trajectory_controller` via the `FollowJointTrajectory` action.
- Show how to control the gripper via the `GripperCommand` action.
- Provide a convenient launch for the `rqt_joint_trajectory_controller` GUI for manual joint control through sliders.

---

## Example nodes and launch files

### 1. `joint_goal_sender`

A node that sends a single motion goal for a group of robot joints (excluding the gripper). It uses the `/joint_trajectory_controller/follow_joint_trajectory` action (the standard `control_msgs/action/FollowJointTrajectory` interface).

**Parameters (ROS parameters):**

| Parameter | Type | Default | Description |
|----------|-----|--------------|----------|
| `shoulder_pan` | double | 0.0 | Target position of the `shoulder_pan_joint`, radians |
| `shoulder_lift` | double | 0.0 | Target position of the `shoulder_lift_joint`, radians |
| `elbow_flex` | double | 0.0 | Target position of the `elbow_flex_joint`, radians |
| `wrist_flex` | double | 0.0 | Target position of the `wrist_flex_joint`, radians |
| `wrist_roll` | double | 0.0 | Target position of the `wrist_roll_joint`, radians |
| `goal_time` | double | 2.0 | Motion execution time, seconds |

**Usage:**

```bash
# Basic launch (all positions = 0)
ros2 run soarm101_examples joint_goal_sender

# With specified target angles (for example, a vertical pose)
ros2 run soarm101_examples joint_goal_sender \
  --ros-args -p shoulder_lift:=0.5 -p elbow_flex:=-0.8 \
  -p goal_time:=3.0
```

**What happens inside:**
1. The node connects to the controller's action server.
2. Creates a `JointTrajectory` message with a single point (the final position).
3. Sends the goal and waits for the result.
4. After the motion completes, the node shuts down.

---

### 2. `gripper_goal_sender`

A node for controlling the robot's gripper through the `/gripper_controller/gripper_cmd` action (type `control_msgs/action/GripperCommand`).

**Parameters (ROS parameters):**

| Parameter | Type | Default | Description |
|----------|-----|--------------|----------|
| `position` | double | 0.0 | Target angle (-0.07 – fully closed, ~1.16 – open) |
| `max_effort` | double | 10.0 | Maximum effort during squeezing |

**Usage:**

```bash
# Close the gripper
ros2 run soarm101_examples gripper_goal_sender

# Open the gripper
ros2 run soarm101_examples gripper_goal_sender \
  --ros-args -p position:=1.16

# With a specified effort
ros2 run soarm101_examples gripper_goal_sender \
  --ros-args -p position:=0.02 -p max_effort:=5.0
```

---

### 3. Launch file: `rqt_joint_trajectory_controller.launch.py`

Launches the `rqt_joint_trajectory_controller` GUI for manual joint control through sliders. Convenient for testing and visual debugging.

**Usage:**

```bash
ros2 launch soarm101_examples rqt_joint_trajectory_controller.launch.py
```

After launch, an rqt window will open with the `joint_trajectory_controller` control panel, where you can:
- Select joints to control,
- Set target positions using sliders,
- Send the trajectory for execution.

---

### 4. Launch file: `rqt_controller_manager.launch.py`

Launches the `rqt_controller_manager` GUI for analyzing the system from the perspective of the ros2_control controller_manager. Convenient for analyzing the state of system components.

**Usage:**

```bash
ros2 launch soarm101_examples rqt_controller_manager.launch.py
```

---

## Interaction with the system

All examples use standard action interfaces provided by the controllers from the `soarm101_ros2_control` package:
- **`joint_trajectory_controller`** – controls a group of joints (5 DOF).
- **`gripper_controller`** – controls the gripper (1 DOF).

Before running the examples, make sure that:
1. The full stack is running (via `soarm101_bringup`).
2. The controllers are activated (their state can be checked via `ros2 control list_controllers`).
3. For a real robot – it is connected and calibrated.

---

## Examples of use in scripts

### Python (via `rclpy.action`)

```python
import rclpy
from rclpy.action import ActionClient
from control_msgs.action import FollowJointTrajectory
from trajectory_msgs.msg import JointTrajectory, JointTrajectoryPoint

node = rclpy.create_node('example_client')
client = ActionClient(node, FollowJointTrajectory, '/joint_trajectory_controller/follow_joint_trajectory')

goal = FollowJointTrajectory.Goal()
goal.trajectory.joint_names = ['shoulder_pan_joint', 'shoulder_lift_joint', ...]
point = JointTrajectoryPoint()
point.positions = [0.5, 0.0, -1.0, 0.0, 0.0]
point.time_from_start = rclpy.duration.Duration(seconds=2.0)
goal.trajectory.points.append(point)

client.wait_for_server()
client.send_goal_async(goal)
```

---

## Dependencies

- `rclcpp`, `rclcpp_action` – base ROS2 components.
- `trajectory_msgs` – trajectory messages.
- `control_msgs` – controller action interfaces.
- `rqt_joint_trajectory_controller` – GUI plugin for manual control.
- `rqt_controller_manager` – GUI plugin for analyzing the ros2_control system.

---

## License

The package is distributed under the **MIT** license (see the [LICENSE](LICENSE) file in the package root).

---

## Support

Questions and suggestions should be submitted via [Issues](https://github.com/cyberbanana777/so-arm101-ros2-pkgs/issues).