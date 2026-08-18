#!/usr/bin/env python3

# Copyright (c) 2026 Alice Zenina and Alexander Grachev RTU MIREA (Russia)
# SPDX-License-Identifier: MIT
# Details in the LICENSE file in the root of the package.

import os

import yaml
import rclpy
from rclpy.node import Node
from geometry_msgs.msg import TransformStamped
from tf2_ros import StaticTransformBroadcaster
from tf_transformations import quaternion_from_euler
from ament_index_python.packages import get_package_share_directory


class StaticTFPublisher(Node):
    def __init__(self):
        super().__init__('static_tf_publisher')
        self.get_logger().info('StaticTFPublisher node started')

        config_file = os.path.join(
            get_package_share_directory('soarm101_teleoperate'),
            'config',
            'dual_arm_setup.yaml'
        )
        with open(config_file, 'r') as f:
            config = yaml.safe_load(f)

        world_config = config['world']
        self.br = StaticTransformBroadcaster(self)

        transforms = []
        for name, params in world_config.items():
            trans = params['translation']
            rot = params['rotation']

            t = TransformStamped()
            t.header.stamp = self.get_clock().now().to_msg()
            t.header.frame_id = 'world_frame'
            t.child_frame_id = f'{name}/zero_point_link'
            t.transform.translation.x = trans['x']
            t.transform.translation.y = trans['y']
            t.transform.translation.z = trans['z']

            q = quaternion_from_euler(rot['roll'], rot['pitch'], rot['yaw'])
            t.transform.rotation.x = q[0]
            t.transform.rotation.y = q[1]
            t.transform.rotation.z = q[2]
            t.transform.rotation.w = q[3]

            transforms.append(t)
            self.get_logger().info(
                f"Added static TF: world -> {name}/zero_point_link "
                f"at ({trans['x']}, {trans['y']}, {trans['z']})"
            )

        self.br.sendTransform(transforms)
        self.get_logger().info('All static transforms published')


def main(args=None):
    rclpy.init(args=args)
    node = StaticTFPublisher()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        node.get_logger().info('StaticTFPublisher shutting down gracefully...')
    finally:
        node.destroy_node()
        if rclpy.ok():
            rclpy.shutdown()


if __name__ == '__main__':
    main()