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