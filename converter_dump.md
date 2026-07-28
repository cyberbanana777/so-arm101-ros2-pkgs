# Пакет: converter_calibration_data

Это пакет **converter_calibration_data**.

---

## Файл: `HOW_TO_USE.md`

```markdown
1. Install deps
```bash
pip install -r pip_requirements.txt
```

2. Calibrate your arm
[Lerobot guide]()
Example of command
```bash
lerobot-calibrate \
    --teleop.type=so101_leader \
    --teleop.port=/dev/ttyACM0 \ # <- The port of your robot
    --teleop.id=my_awesome_leader_arm # <- Give the robot a unique name

```

3. Run the app
```bash
# Your current dir = dir of pkg converter_calibration_data 
cd converter_calibration_data
python3 lerobot_to_custom_format.py ./my_awesome_follower_arm.json ../config/motor_calibration.yaml
cd ..
```

4. Rebuild your ROS2 ws
```bash
cd ../..
colcon build --packages-select converter_calibration_data
source install/local_setup.bash
```


## Файл: `LICENSE`

```
MIT License

Copyright (c) 2026 Alice Zenina and Alexander Grachev RTU MIREA (Russia)

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
```

## Файл: `config/motor_calibration.yaml`

```yaml
shoulder_pan_joint:
  drive_mode: 0
  range_min: 732
  range_max: 3459
shoulder_lift_joint:
  drive_mode: 0
  range_min: 824
  range_max: 3209
elbow_flex_joint:
  drive_mode: 0
  range_min: 866
  range_max: 3082
wrist_flex_joint:
  drive_mode: 0
  range_min: 866
  range_max: 3233
wrist_roll_joint:
  drive_mode: 0
  range_min: 0
  range_max: 4095
gripper_jaw_joint:
  drive_mode: 0
  range_min: 1973
  range_max: 3462
```

## Файл: `converter_calibration_data/__init__.py`

```python

```

## Файл: `converter_calibration_data/lerobot_to_custom_format.py`

```python
#!/usr/bin/env python3

# Copyright (c) 2026 Alice Zenina and Alexander Grachev RTU MIREA (Russia)
# SPDX-License-Identifier: MIT
# Details in the LICENSE file in the root of the package.

"""
Convert robot arm configuration from JSON to YAML.

Usage:
    python3 lerobot_to_custom_format.py <path_to_input.json> <path_to_output.yaml>

Example:
    python3 lerobot_to_custom_format.py ./my_awesome_follower_arm.json ../config/motor_calibration.yaml
"""

import json
import os
import sys

import yaml


def convert_config(input_file: str, output_file: str) -> None:
    """
    Read JSON configuration, transform joint names, and write to YAML.

    Args:
        input_file: Path to the input JSON file.
        output_file: Path to the output YAML file.

    Raises:
        SystemExit: On file errors or conversion issues.
    """
    # Mapping from JSON keys to output YAML joint names
    joint_mapping = {
        "shoulder_pan": "shoulder_pan_joint",
        "shoulder_lift": "shoulder_lift_joint",
        "elbow_flex": "elbow_flex_joint",
        "wrist_flex": "wrist_flex_joint",
        "wrist_roll": "wrist_roll_joint",
        "gripper": "gripper_jaw_joint",
    }

    # Read JSON
    try:
        with open(input_file, 'r', encoding='utf-8') as f:
            data = json.load(f)
    except json.JSONDecodeError as e:
        print(f"JSON parse error: {e}")
        sys.exit(1)
    except Exception as e:
        print(f"Failed to read input file: {e}")
        sys.exit(1)

    # Transform data
    output_data = {}
    for json_key, yaml_key in joint_mapping.items():
        if json_key not in data:
            print(f"Warning: key '{json_key}' not found in JSON, skipping.")
            continue

        joint_data = data[json_key]
        output_data[yaml_key] = {
            "drive_mode": joint_data.get("drive_mode", 0),
            "range_min": joint_data.get("range_min"),
            "range_max": joint_data.get("range_max"),
        }

        # Warn if required fields are missing
        if output_data[yaml_key]["range_min"] is None or output_data[yaml_key]["range_max"] is None:
            print(f"Warning: range_min or range_max missing for '{json_key}', written as None.")

    # Create output directory if needed
    output_dir = os.path.dirname(output_file)
    if output_dir and not os.path.exists(output_dir):
        try:
            os.makedirs(output_dir)
        except OSError as e:
            print(f"Failed to create output directory '{output_dir}': {e}")
            sys.exit(1)

    # Write YAML
    try:
        with open(output_file, 'w', encoding='utf-8') as f:
            yaml.dump(
                output_data,
                f,
                default_flow_style=False,
                sort_keys=False,
                allow_unicode=True,
                indent=2
            )
    except Exception as e:
        print(f"Failed to write YAML file: {e}")
        sys.exit(1)

    print("Conversion completed successfully.")
    print(f"Input file: {input_file}")
    print(f"Output file: {output_file}")


def main():
    """Parse command line arguments and run conversion."""
    if len(sys.argv) != 3:
        print("Error: Please specify input JSON and output YAML paths.")
        print(f"Usage: {sys.argv[0]} <input_json> <output_yaml>")
        sys.exit(1)

    input_file = sys.argv[1]
    output_file = sys.argv[2]

    if not os.path.isfile(input_file):
        print(f"Error: Input file not found: {input_file}")
        sys.exit(1)

    convert_config(input_file, output_file)


if __name__ == "__main__":
    main()
```

## Файл: `package.xml`

```xml
<?xml version="1.0"?>
<?xml-model href="http://download.ros.org/schema/package_format3.xsd" schematypens="http://www.w3.org/2001/XMLSchema"?>
<package format="3">
  <name>converter_calibration_data</name>
  <version>1.0.0</version>
  <description>Pkg with calibration data and scripts for fast adaptation and deploy.</description>
  <maintainer email="sashagrachev2005@gmail.com">banana-killer</maintainer>
  <license>MIT</license>

  <test_depend>ament_copyright</test_depend>
  <test_depend>ament_flake8</test_depend>
  <test_depend>ament_pep257</test_depend>
  <test_depend>python3-pytest</test_depend>

  <export>
    <build_type>ament_python</build_type>
  </export>
</package>
```

## Файл: `pip_requirements.txt`

```text
pyyaml
```

## Файл: `resource/converter_calibration_data`

```

```

## Файл: `setup.cfg`

```
[develop]
script_dir=$base/lib/converter_calibration_data
[install]
install_scripts=$base/lib/converter_calibration_data
```

## Файл: `setup.py`

```python
import os
from glob import glob
from setuptools import find_packages, setup

package_name = 'converter_calibration_data'

setup(
    name=package_name,
    version='1.0.0',
    packages=find_packages(exclude=['test']),
    data_files=[
        ('share/ament_index/resource_index/packages',
            ['resource/' + package_name]),
        ('share/' + package_name, ['package.xml']),
        (
            os.path.join("share", package_name, "config"),
            glob("config/*.yaml"),
        ),
    ],
    install_requires=['setuptools'],
    zip_safe=True,
    maintainer='banana-killer',
    maintainer_email='sashagrachev2005@gmail.com',
    description='Pkg with calibration data and scripts for fast adaptation and deploy.',
    license='MIT',
    extras_require={
        'test': [
            'pytest',
        ],
    },
    entry_points={
        'console_scripts': [
            'lerobot_to_custom_format = converter_calibration_data.lerobot_to_custom_format:main'
        ],
    },
)
```

## Файл: `test/test_copyright.py`

```python
# Copyright 2015 Open Source Robotics Foundation, Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

from ament_copyright.main import main
import pytest


# Remove the `skip` decorator once the source file(s) have a copyright header
@pytest.mark.skip(reason='No copyright header has been placed in the generated source file.')
@pytest.mark.copyright
@pytest.mark.linter
def test_copyright():
    rc = main(argv=['.', 'test'])
    assert rc == 0, 'Found errors'
```

## Файл: `test/test_flake8.py`

```python
# Copyright 2017 Open Source Robotics Foundation, Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

from ament_flake8.main import main_with_errors
import pytest


@pytest.mark.flake8
@pytest.mark.linter
def test_flake8():
    rc, errors = main_with_errors(argv=[])
    assert rc == 0, \
        'Found %d code style errors / warnings:\n' % len(errors) + \
        '\n'.join(errors)
```

## Файл: `test/test_pep257.py`

```python
# Copyright 2015 Open Source Robotics Foundation, Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

from ament_pep257.main import main
import pytest


@pytest.mark.linter
@pytest.mark.pep257
def test_pep257():
    rc = main(argv=['.', 'test'])
    assert rc == 0, 'Found code style errors / warnings'
```

