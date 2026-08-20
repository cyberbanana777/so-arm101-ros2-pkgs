# Third-Party Notices and Attributions

This repository (SOARM101 ROS 2 Workspace) contains code developed by various authors. 
Below is a list of third-party components, their original authors, and the applied licenses.

Please refer to the `LICENSE` and `NOTICE` files within each specific package (`src/<package_name>/`) for full license texts and detailed modification logs.

## 1. scservo_sdk
- **Original Authors:** FTServo (2024), Aditya Kamath / Kamath Robotics (2025)
- **Original Repository:** https://github.com/adityakamath/SCServo_Linux
- **License:** MIT License
- **Our Modifications:** Adaptation for ROS 2, readability improvements (DRY, RAII principles), and standardization of error handling.

## 2. soarm101_description
- **Original Author:** The Robot Studio (2024)
- **Original Repository:** https://github.com/TheRobotStudio/SO-ARM100
- **License:** Apache License 2.0
- **Our Modifications:** Conversion to Xacro format for flexible parameterization, addition of world links/joints, and renaming of joints/links for seamless `ros2_control` compatibility.

## 3. soarm101_hardware
- **Original Author:** The HuggingFace Inc. team (2024)
- **License:** Apache License 2.0
- **Our Modifications:** Aggregation of motor data into a single struct, support for park positions, precomputation of calibration coefficients, and fixes for sensor reading bugs.

---
*All modifications made by Alice Zenina and Alexander Grachev (RTU MIREA, 2026) are distributed under the same licenses (MIT or Apache 2.0) as the original code, unless otherwise specified within a particular package.*