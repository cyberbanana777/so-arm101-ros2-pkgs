# soarm101_telemetry_controller

The package provides a custom controller for collecting and publishing telemetry from the SOARM101 robot's servos. It reads state interfaces from `ros2_control` (exported by the `soarm101_hardware` component) and produces a convenient aggregated `MotorStates` message containing all measured parameters of each motor, including the additional `max_torque` and `enable_torque` fields.

---

## Purpose

- Obtain state data for each joint through state interfaces (position, velocity, effort, temperature, voltage, current, moving_flag, max_torque, enable_torque).
- Publish them in a single topic of type `soarm101_interfaces/msg/MotorStates` for convenient monitoring and logging.
- Separate telemetry from control controllers (for example, `joint_trajectory_controller`), which simplifies the architecture.
- Provide flexible configuration through parameters: you can choose the set of published fields, specify motor IDs, etc.

---

## Configuration parameters

The controller is configured through ROS parameters (usually in the `ros2_control` configuration file or in a launch file). The following are supported:

| Parameter | Type | Description | Example |
|----------|-----|----------|--------|
| `joints` | array of strings | Joint names in the order corresponding to the motor order in the hardware interface. | `["shoulder_pan_joint", "shoulder_lift_joint", ...]` |
| `motor_ids` | array of integers | Motor identifiers (IDs in the Feetech protocol) for each joint. Must match the size of `joints`. If not specified, indices 1,2,3,... are used. | `[1, 2, 3, 4, 5, 6]` |
| `interface_names` | array of strings | Which state interfaces to read. By default: `["position", "velocity", "effort", "temperature", "voltage", "current", "moving_flag", "max_torque", "enable_torque"]`. Can be reduced if not all are needed. | `["position", "temperature", "current", "max_torque"]` |

Example configuration fragment for `controller_manager` (file `soarm101_controllers.yaml`):

```yaml
controller_manager:
  ros__parameters:
    update_rate: 100  # Hz

    soarm101_telemetry_controller:
      type: soarm101_telemetry_controller/ServoTelemetryController

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
      - max_torque
      - enable_torque
```

---

## Published topic

- **Topic:** `~/motor_states` (when used inside a component, it resolves as `<node_namespace>/motor_states`).
- **Type:** `soarm101_interfaces/msg/MotorStates`.
- **QoS:** `best_effort`, `SystemDefaultQoS`.

The message contains an array of `MotorState` structures for each joint. Message fields:

| Field | Type | Description |
|------|-----|----------|
| `motor_id` | `int32` | Motor identifier |
| `joint_name` | `string` | Joint name |
| `position` | `float64` | Current position (radians) |
| `velocity` | `float64` | Current velocity (rad/s) |
| `effort` | `float64` | Current effort (N·m) |
| `temperature` | `float64` | Motor temperature (°C) |
| `voltage` | `float64` | Supply voltage (V) |
| `current` | `float64` | Current (A) |
| `moving_flag` | `bool` | `true` – motor is moving, `false` – stopped |
| `max_torque` | `float64` | Maximum torque for the given joint (N·m), exported from the `soarm101_hardware` state interfaces |
| `enable_torque` | `bool` | Torque enable state: `true` – torque is enabled, `false` – disabled |

---

## Usage in the system

The controller is activated together with other controllers through `controller_manager`. It is usually launched in the `soarm101_bringup` launch file.

After activation, data appears in the `/soarm101_telemetry_controller/motor_states` topic and can be viewed:

```bash
ros2 topic echo /soarm101_telemetry_controller/motor_states
```

Example Python subscription:

```python
from soarm101_interfaces.msg import MotorStates

def callback(msg: MotorStates):
    for motor in msg.motors:
        print(f"Motor {motor.motor_id}: pos={motor.position:.3f}, "
              f"temp={motor.temperature:.1f}°C, "
              f"max_torque={motor.max_torque:.2f} N·m, "
              f"torque_enabled={motor.enable_torque}")

sub = node.create_subscription(MotorStates, '/soarm101_telemetry_controller/motor_states', callback, 10)
```
---

## Dependencies

- `controller_interface` – base class for ROS2 controllers.
- `pluginlib` – plugin registration.
- `rclcpp`, `rclcpp_lifecycle` – lifecycle.
- `realtime_tools` – helper utilities (not used explicitly, but listed in package.xml).
- `soarm101_interfaces` – custom messages (version 2.0.0 and higher, containing the `max_torque` and `enable_torque` fields).

---

## License

The package is distributed under the **MIT** license (see the [LICENSE](LICENSE) file in the package root).

---

## Support

Questions and suggestions should be submitted via [Issues](https://github.com/cyberbanana777/so-arm101-ros2-pkgs/issues).