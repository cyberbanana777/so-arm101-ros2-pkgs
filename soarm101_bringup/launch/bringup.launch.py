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
    """Заменяет все package://... на file:///absolute/path."""
    def repl(match):
        pkg = match.group(1)
        rest = match.group(2)
        try:
            pkg_path = get_package_share_directory(pkg)
            abs_path = os.path.join(pkg_path, rest)
            # Добавляем префикс file:// (с тремя слэшами для абсолютного пути)
            return 'file://' + abs_path
        except Exception:
            return match.group(0)  # если пакет не найден, оставляем как есть
    return re.sub(r'package://(\w+)/(.+)', repl, urdf_string)

def launch_setup(context, *args, **kwargs):
    use_sim = LaunchConfiguration('use_sim').perform(context)
    pkg_soarm101 = get_package_share_directory('soarm101_description')
    pkg_soarm101_ros2_control = get_package_share_directory('soarm101_ros2_control')
    pkg_ros_gz_sim = get_package_share_directory('ros_gz_sim')

    # 1. Разбираем xacro в строку
    xacro_file = os.path.join(pkg_soarm101, 'urdf', 'soarm101.xacro')
    doc = xacro.parse(open(xacro_file))
    xacro.process_doc(doc, mappings={'use_sim': use_sim})
    urdf_str = doc.toprettyxml(indent='  ')

    # 2. Заменяем package:// -> абсолютные пути
    urdf_resolved = resolve_package_uris(urdf_str)
    
    robot_params = {'robot_description': urdf_resolved}

    robot_state_publisher = Node(
        package='robot_state_publisher',
        executable='robot_state_publisher',
        name='robot_state_publisher',
        output='screen',
        parameters=[robot_params]
    )

    if use_sim == 'true':
        # Gazebo
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
        # Spawners
        jsb = Node(
            package='controller_manager',
            executable='spawner',
            arguments=['joint_state_broadcaster', '-c', '/controller_manager', '--param-file', controllers_yaml],
            output='screen'
        )
        jtc = Node(
            package='controller_manager',
            executable='spawner',
            arguments=['joint_trajectory_controller', '-c', '/controller_manager', '--param-file', controllers_yaml],
            output='screen'
        )
        gripper = Node(
            package='controller_manager',
            executable='spawner',
            arguments=['gripper_controller', '-c', '/controller_manager', '--param-file', controllers_yaml],
            output='screen'
        )
        return [
            robot_state_publisher,
            gz_sim,
            spawn_robot,
            jsb,
            jtc,
            gripper
        ]
    else:
        controllers_yaml = os.path.join(pkg_soarm101_ros2_control, 'config', 'real_controllers.yaml')
        control_node = Node(
            package='controller_manager',
            executable='ros2_control_node',
            parameters=[robot_params, controllers_yaml],
            output='screen'
        )
        jsb = Node(
            package='controller_manager',
            executable='spawner',
            arguments=['joint_state_broadcaster', '-c', '/controller_manager', '--param-file', controllers_yaml],
            output='screen'
        )
        jtc = Node(
            package='controller_manager',
            executable='spawner',
            arguments=['joint_trajectory_controller', '-c', '/controller_manager', '--param-file', controllers_yaml],
            output='screen'
        )
        gripper = Node(
            package='controller_manager',
            executable='spawner',
            arguments=['gripper_controller', '-c', '/controller_manager', '--param-file', controllers_yaml],
            output='screen'
        )
        return [
            robot_state_publisher,
            control_node,
            jsb,
            jtc,
            gripper
        ]

def generate_launch_description():
    return LaunchDescription([
        DeclareLaunchArgument('use_sim', default_value='true'),
        OpaqueFunction(function=launch_setup)
    ])