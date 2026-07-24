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