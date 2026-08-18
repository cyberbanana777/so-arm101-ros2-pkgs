# converter_calibration_data

The package contains a utility for converting the SOARM101 robot's calibration data from the **lerobot** library format (JSON) into a custom YAML format, as well as ready-made example configuration files for the leader and follower.

---

## Purpose

After calibrating the robot with `lerobot-calibrate`, you get a JSON file with parameters for each joint. This package converts that file into a structured YAML config, which is then used by:
- the `soarm101_hardware` component for servo initialization.

---

## Installing dependencies

```bash
pip install -r pip_requirements.txt
```

Installs the following libraries:
- `pyyaml`.

---

## Usage

### 1. Calibrate the robot via lerobot

Example command ([from the lerobot documentation](https://huggingface.co/docs/lerobot/so101?calibrate_leader=Command)):

```bash
lerobot-calibrate \
    --teleop.type=so101_leader \
    --teleop.port=/dev/ttyACM0 \
    --teleop.id=my_awesome_leader_arm
```

As output, you get a JSON file, for example `my_awesome_follower_arm.json`.

### 2. Run the converter

```bash
cd converter_calibration_data/converter_calibration_data
python3 lerobot_to_custom_format.py ./my_awesome_follower_arm.json ../config/motor_calibration.yaml leader
```

**Arguments:**
- `./my_awesome_follower_arm.json` – path to the JSON file from lerobot.
- `../config/motor_calibration.yaml` – path where the resulting YAML config will be saved (for the whole stack to work correctly, it must be placed in `../config/` relative to the script directory).
- `leader` – prefix that will be added to the output file name (for example, `leader_motor_calibration.yaml`). This allows distinguishing between leader and follower configurations. It must be either `leader` or `follower`.

After successful execution, you will get a file like this (in the example — `leader_motor_calibration.yaml`):

```yaml
shoulder_pan_joint:
  drive_mode: 0
  range_min: 732
  range_max: 3459
shoulder_lift_joint:
  drive_mode: 0
  range_min: 824
  range_max: 3209
...
```

For the follower, simply specify a different prefix, for example `follower`, and the output file will be named `follower_motor_calibration.yaml`.

### 3. Build the workspace

```bash
cd ../../..
colcon build --packages-select converter_calibration_data
source install/local_setup.bash
```

Now the calibration data is available for use by other software components.

---

## Output YAML format

The file contains a dictionary where the keys are joint names (with the `_joint` suffix), and the values are parameters:

| Field | Type | Description |
|------|-----|----------|
| `drive_mode` | `int` | Drive mode (0 — standard, other values depend on the protocol) |
| `range_min` | `int` | Minimum position value (in raw encoder counts) |
| `range_max` | `int` | Maximum position value |

These ranges are used to convert raw values to radians and back.

---

## License

The package is distributed under the **MIT** license (see the [LICENSE](LICENSE) file in the package root).

---

## Version

**2.0.0** – changed the command-line interface: added a mandatory `prefix` argument for generating the output file name; documentation updated.

---

## Support

Questions and suggestions should be submitted via [Issues](https://github.com/cyberbanana777/so-arm101-ros2-pkgs/issues).