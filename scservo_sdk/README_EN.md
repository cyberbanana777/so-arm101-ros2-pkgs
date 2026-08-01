# scservo_sdk

The package provides a low-level SDK for controlling **Feetech** servos from the SCSCL, SMS_STS, HLSCL, and SCS0009 series over a serial port. It is used within the SOARM101 stack for hardware control of the robot arm.

---

## Package Contents

The package is built as a **static library** (`libscservo_sdk.a`) and contains the following classes:

- **`SCSerial`** – serial port communication (POSIX, Linux).
- **`SCS`** – base exchange protocol (packets, checksum, synchronous operations).
- **`SMS_STS`** – control of SMS/STS servos (modes: servo, closed‑loop wheel, open‑loop wheel).
- **`SCSCL`** – control of SCSCL servos (position, PWM).
- **`HLSCL`** – control of HLS servos (position, velocity, effort).
- **`SCS0009`** – control of SCS0009 servos (used, for example, in the AmazingHand arm).

**Utility modules:**
- `ServoUtils` – encoding/decoding of signed values with a direction bit.
- `SyncWriteBuffer` – RAII buffer for synchronous writes.
- `ServoErrors` – standardized error codes.

---

## Usage in other packages

### In `package.xml` (dependency)

```xml
<depend>scservo_sdk</depend>
```

### In `CMakeLists.txt`

```cmake
find_package(scservo_sdk REQUIRED)

target_link_libraries(your_node
  scservo_sdk
)
```

Include headers:

```cpp
#include "SCServo.h"
```

### Example initialization and motor control (SMS_STS)

```cpp
#include "SCServo.h"

SMS_STS servo;
servo.begin(1000000, "/dev/ttyUSB0");  // 1 Mbps, port
servo.InitMotor(1, 0, 1);              // ID=1, servo mode, enable effort
servo.WritePosEx(1, 2048, 1000, 50);   // move to centre
```

---

## Dependencies

- ROS2 Humble (only for build; the package does not use ROS interfaces).
- POSIX system (Linux) with `termios` support.
- C++17 standard library.

---

## License

The package is distributed under the **MIT** license (see the [LICENSE](LICENSE) file in the package root).

---

## Source Code

This SDK is an adaptation of the repository:  
[https://github.com/adityakamath/SCServo_Linux](https://github.com/adityakamath/SCServo_Linux)  
Changes have been made for integration into a ROS2 workspace and for improved readability (DRY, RAII, error standardisation).

---

## Support

For questions and suggestions, please use [Issues](https://github.com/cyberbanana777/so-arm101-ros2-pkgs/issues).