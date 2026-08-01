# soarm101_description

The package contains the geometric description (URDF) of the **SOARM101** robotic arm, as well as 3D models (STL) for visualisation and collision calculations. The description is provided in **xacro** format for flexible parameter tuning (e.g., simulation vs. real hardware).

---

## Package Contents

- **`urdf/soarm101.xacro`** – main robot description file (xacro). It defines:
  - the kinematic chain (7 joints: shoulder_pan, shoulder_lift, elbow_flex, wrist_flex, wrist_roll, gripper_jaw, plus a fixed world–base connection),
  - inertial properties of each link,
  - visual and collision geometries (references to STL meshes),
  - effort limits, velocity limits, and movement ranges for each joint.

- **`meshes/`** – contains STL files for each link and component:
  - base holders, motor housings, shoulders, forearm, wrist, gripper, etc.
  - All meshes are used for both visualisation and collision.

---

## Usage in other packages

The package does not contain executable nodes, only description files. To load the robot model in ROS2, use standard tools:

### In launch files (e.g., in `soarm101_bringup`):

```python
from launch_ros.parameter_descriptions import ParameterValue
from launch.substitutions import Command

robot_description = ParameterValue(
    Command(['xacro ', find_package('soarm101_description'), '/urdf/soarm101.xacro']),
    value_type=str
)
```

### In C++ / Python nodes to obtain the URDF:

```cpp
#include <urdf/model.h>
urdf::Model model;
model.initParam("robot_description");
```

### In RViz / MoveIt / Gazebo

The package automatically installs files to `share/soarm101_description/`, so the paths to meshes inside the URDF use `package://soarm101_description/meshes/...`, which is correctly resolved in ROS2. For use in Gazebo, additional processing of the URDF occurs in the simulation launch file.

---

## Xacro arguments

- **`use_sim`** – default `true`. Determines whether to use simulation parameters (e.g., for Gazebo). Can be overridden in the launch file if needed.

---

## Dependencies

- `urdf` – for URDF parsing,
- `xacro` – for processing xacro files.

---

## Origin

The original URDF file was taken from the repository [TheRobotStudio/SO-ARM100](https://github.com/TheRobotStudio/SO-ARM100) (file `Simulation/SO101/so101_new_calib.urdf`).  
The following changes have been made:
- adaptation to our stack (added a world link and a world_to_base_joint),
- migration to xacro for ease of parameterisation,
- joint and link names aligned for integration with `ros2_control` and `soarm101_hardware`.

---

## License

The package is distributed under the **Apache‑2.0** license (see the [LICENSE](LICENSE) file in the package root).

---

## Support

For questions and suggestions, please use [Issues](https://github.com/cyberbanana777/so-arm101-ros2-pkgs/issues).