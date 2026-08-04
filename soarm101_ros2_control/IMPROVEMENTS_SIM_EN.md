# Improvements to ros2_control Configuration for Gazebo Simulation

This document provides recommendations for refining your XACRO file and related settings. Each improvement is described by three criteria:

- **Entity** – what exactly changes and how to implement it.
- **Complexity** – estimated effort (low, medium, high).
- **Pros** – what benefits you gain.

---

## 1. Extend the Set of Command Interfaces

| **Entity** | Add `velocity` and `effort` interfaces within the `<joint>` tags, in addition to the existing `position`. |
|------------|------------------------------------------------------------------------------------------------------------|
| **Complexity** | 🟢 **Low** – changes only affect XACRO, no C++ code modifications required. |
| **Pros**    | • Ability to use different controller types (velocity-based, effort-based).<br>• Flexibility when switching from position control to more complex laws.<br>• Compatibility with most off-the-shelf ROS 2 Control controllers. |

**Example implementation:**

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

## 2. Configure PID Controllers for Each Joint

| **Entity** | Add PID parameters for position and velocity control inside the `<joint>` tag. |
|------------|--------------------------------------------------------------------------------|
| **Complexity** | 🟡 **Medium** – requires tuning coefficients for the specific robot model. |
| **Pros**    | • More accurate and smooth trajectory tracking.<br>• Reduced overshoot and oscillations.<br>• Ability to fine-tune the dynamics of each joint independently. |

**Example implementation:**

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
  <!-- interfaces ... -->
</joint>
```

---

## 3. Implement a Mimic Joint for the Gripper

| **Entity** | In the URDF description (not in `ros2_control`), add the `mimic` attribute to the second gripper finger. |
|------------|----------------------------------------------------------------------------------------------------------|
| **Complexity** | 🟢 **Low** – just one attribute added to the existing URDF. |
| **Pros**    | • Control the gripper with a single command (the follower finger mirrors the leader).<br>• Simplifies controllers and trajectories.<br>• Natural synchronisation of the fingers. |

**Example implementation (in the main URDF):**

```xml
<joint name="gripper_finger_right_joint" type="revolute">
  <!-- ... -->
  <mimic joint="gripper_finger_left_joint" multiplier="-1.0" offset="0.0" />
</joint>
```

---

## 4. Additional Parameters for the gz_ros2_control Plugin

| **Entity** | Add `<hold_joints>` and `<position_proportional_gain>` parameters inside the `<gazebo>` tag. |
|------------|----------------------------------------------------------------------------------------------|
| **Complexity** | 🟢 **Low** – simple additions to XACRO. |
| **Pros**    | • `hold_joints=true` – maintains position when no commands are received (prevents “drooping”).<br>• `position_proportional_gain` – fine‑tunes the built‑in P‑controller to reduce position error. |

**Example implementation:**

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

## 5. Refine the YAML Controller Configuration

| **Entity** | In `sim_controllers.yaml`, add the `joint_state_broadcaster` and a main controller (e.g., `joint_trajectory_controller`), and set `use_sim_time=true` in launch files. |
|------------|----------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| **Complexity** | 🟡 **Medium** – requires understanding of ROS 2 Control controllers. |
| **Pros**    | • Publish `/joint_states` for visualisation and navigation.<br>• Trajectory management via the standard interface.<br>• Synchronisation with simulation time – ensures correct operation of all nodes. |

**Example YAML structure:**

```yaml
controller_manager:
  ros__parameters:
    update_rate: 100  # Hz

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

**In the launch file, be sure to add:**

```python
Node(
    package='controller_manager',
    executable='ros2_control_node',
    parameters=[{'use_sim_time': True}],
    ...
)
```

---

## Summary

Applying all the improvements listed above will allow you to:

- significantly increase the realism and controllability of the robot in simulation;
- use a wide range of standard ROS 2 controllers;
- simplify development and debugging of control algorithms.

Start with the items of **low complexity** – they provide quick gains – then move on to PID tuning and YAML refinement. Good luck with your configuration!
