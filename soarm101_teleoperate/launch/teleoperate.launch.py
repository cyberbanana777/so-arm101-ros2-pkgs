# Copyright (c) 2026 Alice Zenina and Alexander Grachev RTU MIREA (Russia)
# SPDX-License-Identifier: MIT
# Details in the LICENSE file in the root of the package.

import os
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, IncludeLaunchDescription
from launch.substitutions import LaunchConfiguration
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch_ros.actions import Node
from ament_index_python.packages import get_package_share_directory
from launch.actions import RegisterEventHandler, TimerAction
from launch.event_handlers import OnShutdown

def generate_launch_description():
    # ----- Arguments -----
    leader_port = LaunchConfiguration('leader_port')
    follower_port = LaunchConfiguration('follower_port')
    leader_namespace = 'leader'
    follower_namespace = 'follower'

    # ----- Paths -----
    pkg_bringup = get_package_share_directory('soarm101_bringup')
    pkg_teleoperate = get_package_share_directory('soarm101_teleoperate')
    bringup_launch = os.path.join(pkg_bringup, 'launch', 'bringup.launch.py')

    # ----- Start leader ----- 
    leader_launch = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(bringup_launch),
        launch_arguments={
            'arm_type': 'leader',
            'port': leader_port,
            'use_sim': 'false',
            'max_speed': '2400',
            'max_accel': '50',
        }.items()
    )

    # ----- Start follower -----
    follower_launch = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(bringup_launch),
        launch_arguments={
            'arm_type': 'follower',
            'port': follower_port,
            'use_sim': 'false',
            'max_speed': '3400',
            'max_accel': '254',
        }.items()
    )

    # node for static transform
    static_tf_node = Node(
        package='soarm101_teleoperate',
        executable='static_transform_publisher_world_to_arms',
        name='static_transform_publisher_world_to_arms',
        output='screen'
    )

    # marker publisher node
    marker_node = Node(
        package='soarm101_teleoperate',
        executable='marker_publisher',
        name='marker_publisher',
        output='screen'
    )
    # ----- Start RViz with config -----
    rviz_default_config = os.path.join(pkg_teleoperate, 'rviz', 'dual_arm.rviz')
    rviz_node = Node(
        package='rviz2',
        executable='rviz2',
        name='rviz2',
        arguments=['-d', rviz_default_config],
        output='screen'
    )
    
    relay_node = Node(
    package='soarm101_teleoperate',
    executable='arm_relay',
    name='arm_relay',
    output='screen',
    parameters=[{
        'command_timeout': 0.002,
        'publish_rate': 50.0,
        'ignored_joints': ['gripper_jaw_joint'],
        }]
    )
    
    gripper_relay_node = Node(
    package='soarm101_teleoperate',
    executable='gripper_relay',
    name='gripper_relay',
    output='screen',
    parameters=[{
        'max_effort': 10.0,
        'joint_name': 'gripper_jaw_joint',
        }]
    )
    
    shutdown_handler = RegisterEventHandler(
        OnShutdown(
            on_shutdown=[
                TimerAction(
                    period=10.0,   # waiting 10 sec before shutdown
                    actions=[]     # no actions, just waiting
                )
            ]
        )
    )

    return LaunchDescription([
        DeclareLaunchArgument('leader_port', default_value='/dev/soarm101_leader', description='Serial port for leader arm'),
        DeclareLaunchArgument('follower_port', default_value='/dev/soarm101_follower', description='Serial port for follower arm'),
        leader_launch,
        follower_launch,
        static_tf_node,
        marker_node,
        rviz_node,
        relay_node,
        gripper_relay_node,
        shutdown_handler,
    ])