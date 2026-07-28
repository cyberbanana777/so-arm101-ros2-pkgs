# Пакет: soarm101_interfaces

Это пакет **soarm101_interfaces**.

---

## Файл: `CMakeLists.txt`

```text
cmake_minimum_required(VERSION 3.8)
project(soarm101_interfaces)
find_package(ament_cmake REQUIRED)
find_package(rosidl_default_generators REQUIRED)

rosidl_generate_interfaces(${PROJECT_NAME}
  "msg/MotorState.msg"
  "msg/MotorStates.msg"
)
ament_package()
```

## Файл: `msg/MotorState.msg`

```text
int32 motor_id
string joint_name
float64 position
float64 velocity
float64 effort
float64 temperature
float64 voltage
float64 current
bool moving_flag
```

## Файл: `msg/MotorStates.msg`

```text
soarm101_interfaces/MotorState[] motors
```

## Файл: `package.xml`

```xml
<?xml version="1.0"?>
<?xml-model href="http://download.ros.org/schema/package_format3.xsd" schematypens="http://www.w3.org/2001/XMLSchema"?>
<package format="3">
  <name>soarm101_interfaces</name>
  <version>1.0.0</version>
  <description>Interfaces for work with SOARM101</description>
  <maintainer email="sashagrachev2005@gmail.com">banana-killer</maintainer>
  <license>MIT</license>

  <buildtool_depend>ament_cmake</buildtool_depend>

  <depend>rosidl_default_generators</depend>
  
  <exec_depend>rosidl_default_runtime</exec_depend>

  <member_of_group>rosidl_interface_packages</member_of_group>

  <test_depend>ament_lint_auto</test_depend>
  <test_depend>ament_lint_common</test_depend>

  <export>
    <build_type>ament_cmake</build_type>
  </export>
</package>
```

