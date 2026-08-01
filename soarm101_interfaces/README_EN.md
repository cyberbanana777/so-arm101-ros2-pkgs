# soarm101_interfaces

The package contains custom ROS2 messages for publishing telemetry data of all robot motors.  
It defines the telemetry data format for all motors and is used by stack nodes.

---

## Messages

### `MotorState.msg`
Message about the state of a single motor.

| Field | Type | Description |
|-------|------|-------------|
| `motor_id` | `int32` | Motor ID (assigned during initial setup) |
| `joint_name` | `string` | Joint name (e.g., `"joint1"`) |
| `position` | `float64` | Current position (radians) |
| `velocity` | `float64` | Current velocity (rad/s) |
| `effort` | `float64` | Current effort (arbitrary units; see servo documentation for details) |
| `temperature` | `float64` | Motor temperature (°C) |
| `voltage` | `float64` | Supply voltage (V) |
| `current` | `float64` | Current (A) |
| `moving_flag` | `bool` | Motor state: `true` – motor is moving, `false` – stopped |

### `MotorStates.msg`
Array of all motor states, containing messages of type `MotorState.msg`:

```text
soarm101_interfaces/MotorState[] motors
```

---

## Usage in other packages

**Add the dependency in `package.xml`:**

```xml
<depend>soarm101_interfaces</depend>
```

**In `CMakeLists.txt`:**

```cmake
find_package(soarm101_interfaces REQUIRED)
```

### Example subscription to the topic (Python)

```python
from soarm101_interfaces.msg import MotorStates

def callback(msg: MotorStates):
    for motor in msg.motors:
        print(f"Motor {motor.motor_id}: pos={motor.position:.3f}")

sub = node.create_subscription(MotorStates, '/soarm101_telemetry_controller/motor_states', callback, 10)
```

---

## Topic

Within the complete SOARM101 stack, the package publishes telemetry to the topic:

**`/soarm101_telemetry_controller/motor_states`** – type `soarm101_interfaces/msg/MotorStates`

---

## License

Distributed under the **MIT** license (see [LICENSE](LICENSE) in the repository root).

---

## Support

For questions and suggestions, please use [Issues](https://github.com/cyberbanana777/so-arm101-ros2-pkgs/issues).