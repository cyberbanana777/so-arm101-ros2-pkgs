# Пакет: 

Это пакет ****.

---

## Файл: `LICENSE`

```
Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
```

## Файл: `README.md`

```markdown
# soarm101_bringup

Пакет-агрегатор, который обеспечивает **единый запуск** всего программного стека робота SOARM101. Он объединяет запуск `robot_state_publisher`, `ros2_control_node` (или Gazebo) и всех необходимых контроллеров в зависимости от выбранного режима: **симуляция** или **реальный робот**.

---

## Назначение

- Предоставить одну точку входа для запуска всей системы.
- Автоматически подбирать конфигурацию `ros2_control` (реальное железо или Gazebo) на основе аргумента `use_sim`.
- Загружать и активировать все контроллеры (`joint_state_broadcaster`, `joint_trajectory_controller`, `gripper_controller`, а для реального робота – ещё и `soarm101_telemetry_controller`).
- Обрабатывать xacro-файл, подставляя нужные include для `ros2_control` и разрешая пути к мешам (заменяя `package://` на абсолютные пути).

---

## Структура пакета

```
soarm101_bringup/
├── launch/
│   └── bringup.launch.py          # Главный launch-файл
├── urdf/
│   └── full.xacro                 # Сборка полного URDF с выбором ros2_control
├── test/                          # Тесты (flake8, pep257, copyright)
├── package.xml
├── setup.py
├── setup.cfg
├── LICENSE
└── resource/                      # Файл для ament_index
```

---

## Launch-файл: `bringup.launch.py`

### Аргументы

| Аргумент | Тип | По умолчанию | Описание |
|----------|-----|--------------|----------|
| `use_sim` | bool | `false` | Если `true` – запускается Gazebo с пустым миром и симуляционные контроллеры; если `false` – запускается `ros2_control_node` для реального робота. |

### Логика работы

1. **Разбор xacro** – читает `urdf/full.xacro`, передаёт в него значение `use_sim`, получает полную URDF-строку.
2. **Замена `package://`** – все пути вида `package://soarm101_description/meshes/...` заменяются на абсолютные пути с префиксом `file://`, чтобы Gazebo и другие компоненты могли загрузить меши.
3. **Запуск `robot_state_publisher`** – публикует `/robot_description` и транслирует `tf` на основе `joint_state` (которые будут приходить от контроллеров).
4. **Ветвление по `use_sim`**:

#### Режим симуляции (`use_sim:=true`)
- Запускает **Gazebo** через `ros_gz_sim` (пустой мир).
- Спавнит робота через `ros_gz_sim create`.
- Загружает контроллеры из `sim_controllers.yaml` (пакет `soarm101_ros2_control`):
  - `joint_state_broadcaster`
  - `joint_trajectory_controller`
  - `gripper_controller`

#### Режим реального робота (`use_sim:=false`)
- Запускает **`ros2_control_node`** с параметрами:
  - `robot_description` (обработанный URDF)
  - `real_controllers.yaml` (из `soarm101_ros2_control`)
- Спавнит контроллеры:
  - `joint_state_broadcaster`
  - `joint_trajectory_controller`
  - `gripper_controller`
  - `soarm101_telemetry_controller` – кастомный контроллер для публикации расширенной телеметрии.

---

## Полный URDF: `urdf/full.xacro`

Файл включает:
- Основной URDF робота (`soarm101_description/urdf/soarm101.xacro`).
- В зависимости от аргумента `use_sim` подключает либо:
  - `ros2_control_sim.xacro` – плагин `gz_ros2_control` для Gazebo,
  - `ros2_control_real.xacro` – плагин `soarm101_hardware/SOARM101SystemHardware` для реального железа.

Это позволяет использовать одно и то же описание робота для обоих режимов, меняя только низкоуровневый hardware-интерфейс.

---

## Использование

### Запуск реального робота

```bash
ros2 launch soarm101_bringup bringup.launch.py use_sim:=false
```

После запуска:
- Подключается к шине моторов (порт и скорость задаются в `ros2_control_real.xacro`).
- Активируются все контроллеры.
- Робот готов к приёму траекторий через `joint_trajectory_controller` или команд через другие интерфейсы.

### Запуск симуляции в Gazebo

```bash
ros2 launch soarm101_bringup bringup.launch.py use_sim:=true
```

После запуска:
- Открывается окно Gazebo с пустым миром.
- Робот появляется в мире с начальной позицией.
- Контроллеры работают в симуляционном режиме (используется `gz_ros2_control`).

### Дополнительные опции

Можно добавить аргументы для изменения параметров, например, указать другой файл калибровки или порт, но они уже заданы в xacro-файлах `soarm101_ros2_control`. При необходимости их можно переопределить через launch-аргументы (сейчас не реализовано, но легко добавить).

---

## Зависимости

Пакет явно зависит от:

- `robot_state_publisher` – публикация состояния робота.
- `controller_manager` – управление контроллерами.
- `soarm101_description` – URDF-описание.
- `soarm101_hardware` – аппаратный компонент для реального робота.
- `soarm101_ros2_control` – конфигурации `ros2_control`.
- `soarm101_telemetry_controller` – кастомный контроллер телеметрии.

Для симуляции дополнительно требуются:
- `ros_gz_sim` – интеграция Gazebo с ROS2.
- `gz_ros2_control` – плагин управления в Gazebo.

---

## Примечания

- В реальном режиме контроллеры загружаются через spawner, а не через `ros2_control_node` напрямую, что обеспечивает более гибкое управление и возможность перезагрузки отдельных контроллеров.

---

## Лицензия

Пакет распространяется под лицензией **MIT** (см. файл [LICENSE](LICENSE) в корне пакета).

---

## Поддержка

Вопросы и предложения оформляйте через [Issues](https://github.com/cyberbanana777/so-arm101-ros2-pkgs/issues).
```

## Файл: `launch/bringup.launch.py`

```python
import os
import re
import xacro
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, OpaqueFunction
from launch.substitutions import LaunchConfiguration, Command
from launch_ros.actions import Node
from launch.actions import IncludeLaunchDescription
from launch.launch_description_sources import PythonLaunchDescriptionSource
from ament_index_python.packages import get_package_share_directory

def resolve_package_uris(urdf_string):
    """Replace all package://... with file:///absolute/path."""
    def repl(match):
        pkg = match.group(1)
        rest = match.group(2)
        try:
            pkg_path = get_package_share_directory(pkg)
            abs_path = os.path.join(pkg_path, rest)
            return 'file://' + abs_path
        except Exception:
            return match.group(0)
    return re.sub(r'package://(\w+)/(.+)', repl, urdf_string)

def launch_setup(context, *args, **kwargs):
    use_sim = LaunchConfiguration('use_sim').perform(context)
    arm_type = LaunchConfiguration('arm_type').perform(context)
    port = LaunchConfiguration('port').perform(context)
    max_speed = LaunchConfiguration('max_speed').perform(context)
    max_accel = LaunchConfiguration('max_accel').perform(context)
    pkg_soarm101_bringup = get_package_share_directory('soarm101_bringup')
    pkg_soarm101_ros2_control = get_package_share_directory('soarm101_ros2_control')
    pkg_ros_gz_sim = get_package_share_directory('ros_gz_sim')

    # 1. Parse xacro file with mappings
    xacro_file = os.path.join(pkg_soarm101_bringup, 'urdf', 'full.xacro')
    doc = xacro.parse(open(xacro_file))
    xacro.process_doc(
        doc, mappings={
            'use_sim': use_sim,
            'arm_type': arm_type,
            'port': port,
            'max_speed': max_speed,
            'max_accel': max_accel,
        }
    )
    urdf_str = doc.toprettyxml(indent='  ')

    # 2. Replace package:// with absolute file paths
    urdf_resolved = resolve_package_uris(urdf_str)

    robot_params = {'robot_description': urdf_resolved}

    # ---- Robot state publisher is always needed ----
    robot_state_publisher = Node(
        package='robot_state_publisher',
        executable='robot_state_publisher',
        name='robot_state_publisher',
        output='screen',
        parameters=[robot_params]
    )

    # ---- Helper function: create a spawner node ----
    def spawner_node(name, controller_manager='/controller_manager', param_file=None):
        args = [name, '-c', controller_manager]
        if param_file:
            args.extend(['--param-file', param_file])
        return Node(
            package='controller_manager',
            executable='spawner',
            arguments=args,
            output='screen'
        )

    if use_sim == 'true':
        # ----- SIMULATION MODE -----
        gz_sim = IncludeLaunchDescription(
            PythonLaunchDescriptionSource(
                os.path.join(pkg_ros_gz_sim, 'launch', 'gz_sim.launch.py')
            ),
            launch_arguments={'gz_args': '-r empty.sdf'}.items()
        )
        spawn_robot = Node(
            package='ros_gz_sim',
            executable='create',
            arguments=['-name', 'soarm101', '-topic', 'robot_description'],
            output='screen'
        )
        controllers_yaml = os.path.join(pkg_soarm101_ros2_control, 'config', 'sim_controllers.yaml')

        # --- joint_state_broadcaster is always needed ---
        jsb = spawner_node('joint_state_broadcaster', param_file=controllers_yaml)

        nodes = [robot_state_publisher, gz_sim, spawn_robot, jsb]

        # --- Trajectory and gripper controllers are only needed for follower ---
        if arm_type == 'follower':
            jtc = spawner_node('joint_trajectory_controller', param_file=controllers_yaml)
            gripper = spawner_node('gripper_controller', param_file=controllers_yaml)
            nodes.extend([jtc, gripper])

        return nodes

    else:
        # ----- REAL ROBOT MODE -----
        controllers_yaml = os.path.join(pkg_soarm101_ros2_control, 'config', 'real_controllers.yaml')
        control_node = Node(
            package='controller_manager',
            executable='ros2_control_node',
            parameters=[robot_params, controllers_yaml],
            output='screen'
        )

        # --- joint_state_broadcaster and telemetry are always needed ---
        jsb = spawner_node('joint_state_broadcaster', param_file=controllers_yaml)
        telemetry = spawner_node('soarm101_telemetry_controller', param_file=controllers_yaml)

        nodes = [robot_state_publisher, control_node, jsb, telemetry]

        # --- Trajectory and gripper controllers are only needed for follower ---
        if arm_type == 'follower':
            jtc = spawner_node('joint_trajectory_controller', param_file=controllers_yaml)
            gripper = spawner_node('gripper_controller', param_file=controllers_yaml)
            nodes.extend([jtc, gripper])

        return nodes

def generate_launch_description():
    return LaunchDescription([
        DeclareLaunchArgument(
            name='use_sim',
            description='True -> run in Gazebo, false -> run on real robot',
            default_value='false',
            choices=["false", "true"]
        ),
        DeclareLaunchArgument(
            name='arm_type',
            description='Type of arm: follower (controlled) or leader (read-only)',
            default_value='follower',
            choices=['follower', 'leader']
        ),
        DeclareLaunchArgument(
            name='port',
            description='Serial port for the robot',
            default_value='/dev/ttyACM0',
        ),
        DeclareLaunchArgument(
            name='max_speed',
            description='Max speed (0–3400) for each joint',
            default_value='2400',
        ),
        DeclareLaunchArgument(
            name='max_accel',
            description='Max acceleration (0–254) for each joint',
            default_value='50',
        ),
        OpaqueFunction(function=launch_setup)
    ])
```

## Файл: `package.xml`

```xml
<?xml version="1.0"?>
<?xml-model href="http://download.ros.org/schema/package_format3.xsd" schematypens="http://www.w3.org/2001/XMLSchema"?>
<package format="3">
  <name>soarm101_bringup</name>
  <version>1.0.0</version>
  <description>Launch files and configurations for SO-ARM101</description>
  <maintainer email="sashagrachev2005@gmail.com">banana-killer</maintainer>
  <license>MIT</license>

  <depend>launch</depend>
  <depend>launch_ros</depend>
  <depend>robot_state_publisher</depend>
  <depend>controller_manager</depend>
  <depend>soarm101_description</depend>
  <depend>soarm101_hardware</depend>
  <depend>soarm101_ros2_control</depend>
  <depend>soarm101_telemetry_controller</depend>

  <test_depend>ament_copyright</test_depend>
  <test_depend>ament_flake8</test_depend>
  <test_depend>ament_pep257</test_depend>
  <test_depend>python3-pytest</test_depend>

  <export>
    <build_type>ament_python</build_type>
  </export>
</package>
```

## Файл: `resource/soarm101_bringup`

```

```

## Файл: `setup.cfg`

```
[develop]
script_dir=$base/lib/soarm101_bringup
[install]
install_scripts=$base/lib/soarm101_bringup
```

## Файл: `setup.py`

```python
import os
from glob import glob
from setuptools import find_packages, setup

package_name = 'soarm101_bringup'

setup(
    name=package_name,
    version='1.0.0',
    packages=find_packages(exclude=['test']),
    data_files=[
        ('share/ament_index/resource_index/packages',
            ['resource/' + package_name]),
        ('share/' + package_name, ['package.xml']),
        (
            os.path.join("share", package_name, "launch"),
            glob("launch/*.launch.py"),
        ),
        (
            os.path.join("share", package_name, "urdf"),
            glob("urdf/*.xacro"),
        ),
    ],
    install_requires=['setuptools'],
    zip_safe=True,
    maintainer='banana-killer',
    maintainer_email='sashagrachev2005@gmail.com',
    description='Launch files and configurations for SO-ARM101',
    license='MIT',
    extras_require={
        'test': [
            'pytest',
        ],
    },
    entry_points={
        'console_scripts': [
        ],
    },
)
```

## Файл: `soarm101_bringup/__init__.py`

```python

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

## Файл: `urdf/full.xacro`

```xml
<?xml version="1.0"?>
<robot xmlns:xacro="http://www.ros.org/wiki/xacro" name="soarm101">

  <!-- Аргумент выбора режима (sim / real) -->
  <xacro:arg name="use_sim" default="false"/>
  <xacro:arg name="arm_type" default="follower"/>
  <xacro:arg name="port" default="/dev/ttyACM0"/>
  <xacro:arg name="max_speed" default="2400"/>
  <xacro:arg name="max_accel" default="50"/>

  <xacro:property name="arm_type" value="$(arg arm_type)" />

  <!-- URDF_description of SOARM101 -->
  <xacro:include filename="$(find soarm101_description)/urdf/soarm101.xacro"/>
  <xacro:include filename="$(find soarm101_ros2_control)/urdf/ros2_control_real.xacro"/>


  <!--=================== ROS2_CONTROL & PLUGINS ===================-->
  <!-- Выбор варианта в зависимости от аргумента use_sim -->
  <xacro:if value="$(arg use_sim)">
    <!-- СИМУЛЯЦИЯ -->
    <xacro:include filename="$(find soarm101_ros2_control)/urdf/ros2_control_sim.xacro"/>

  </xacro:if>

  <xacro:unless value="$(arg use_sim)">
    <!-- РЕАЛЬНЫЙ РОБОТ -->

    <xacro:if value="${arm_type == 'leader'}">
      <xacro:soarm101_hardware_leader port="$(arg port)" max_speed="$(arg max_speed)" max_accel="$(arg max_accel)"/>
    </xacro:if>
    
    <xacro:if value="${arm_type == 'follower'}">
      <xacro:soarm101_hardware_follower port="$(arg port)" max_speed="$(arg max_speed)" max_accel="$(arg max_accel)"/>
    </xacro:if>
    
  </xacro:unless>

</robot>
```

