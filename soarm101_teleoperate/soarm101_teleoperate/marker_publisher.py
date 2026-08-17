#!/usr/bin/env python3

import yaml
import rclpy
from rclpy.node import Node
from visualization_msgs.msg import Marker, MarkerArray
from tf_transformations import quaternion_from_euler
import os
from ament_index_python.packages import get_package_share_directory

class MarkerPublisher(Node):
    def __init__(self):
        super().__init__('marker_publisher')
        self.get_logger().info('MarkerPublisher node started')

        config_file = os.path.join(
            get_package_share_directory('soarm101_teleoperate'),
            'config',
            'dual_arm_setup.yaml'
        )
        with open(config_file, 'r') as f:
            config = yaml.safe_load(f)
        
        self.marker_config = config['markers']
        self.pub = self.create_publisher(MarkerArray, '/markers', 10)
        self.timer = self.create_timer(1.0, self.publish_markers)
        self.publish_markers()

    def create_diamond_markers(self, frame_id, color, text, offset_z, size):
        markers = []
        
        # Текст
        text_marker = Marker()
        text_marker.header.frame_id = frame_id
        text_marker.header.stamp = self.get_clock().now().to_msg()
        text_marker.ns = 'dual_arm'
        text_marker.id = 0
        text_marker.type = Marker.TEXT_VIEW_FACING
        text_marker.action = Marker.ADD
        text_marker.text = text
        text_marker.pose.position.z = offset_z + size + 0.02
        text_marker.pose.orientation.w = 1.0
        text_marker.scale.x = 0.1
        text_marker.scale.y = 0.1
        text_marker.scale.z = 0.1
        text_marker.color.r = color[0]
        text_marker.color.g = color[1]
        text_marker.color.b = color[2]
        text_marker.color.a = 1.0
        text_marker.lifetime = rclpy.duration.Duration(seconds=2.0).to_msg()
        markers.append(text_marker)

        # Куб 1: повёрнут на 45° вокруг оси X
        cube1 = Marker()
        cube1.header.frame_id = frame_id
        cube1.header.stamp = self.get_clock().now().to_msg()
        cube1.ns = 'dual_arm'
        cube1.id = 1
        cube1.type = Marker.CUBE
        cube1.action = Marker.ADD
        cube1.pose.position.z = offset_z
        q1 = quaternion_from_euler(0.0, 0.0, 0.785)  # 45°
        cube1.pose.orientation.x = q1[0]
        cube1.pose.orientation.y = q1[1]
        cube1.pose.orientation.z = q1[2]
        cube1.pose.orientation.w = q1[3]
        cube1.scale.x = size
        cube1.scale.y = size
        cube1.scale.z = size
        cube1.color.r = color[0]
        cube1.color.g = color[1]
        cube1.color.b = color[2]
        cube1.color.a = 0.8
        cube1.lifetime = rclpy.duration.Duration(seconds=2.0).to_msg()
        markers.append(cube1)

        # # Куб 2: повёрнут на -45° вокруг оси X
        # cube2 = Marker()
        # cube2.header.frame_id = frame_id
        # cube2.header.stamp = self.get_clock().now().to_msg()
        # cube2.ns = 'dual_arm'
        # cube2.id = 2
        # cube2.type = Marker.CUBE
        # cube2.action = Marker.ADD
        # cube2.pose.position.z = offset_z
        # q2 = quaternion_from_euler(-0.785, 0.0, 0.0)  # -45°
        # cube2.pose.orientation.x = q2[0]
        # cube2.pose.orientation.y = q2[1]
        # cube2.pose.orientation.z = q2[2]
        # cube2.pose.orientation.w = q2[3]
        # cube2.scale.x = size
        # cube2.scale.y = size * 0.5
        # cube2.scale.z = size * 0.5
        # cube2.color.r = color[0]
        # cube2.color.g = color[1]
        # cube2.color.b = color[2]
        # cube2.color.a = 0.8
        # cube2.lifetime = rclpy.duration.Duration(seconds=2.0).to_msg()
        # markers.append(cube2)

        return markers

    def publish_markers(self):
        marker_array = MarkerArray()
        marker_id_counter = 0

        for name, params in self.marker_config.items():
            frame_id = f'{name}/zero_point_link'
            color = params['color']
            text = params['text']
            offset_z = params['offset_z']
            shape = params.get('shape', 'diamond')

            if shape == 'diamond':
                size = params.get('diamond_size', 0.1)
                markers = self.create_diamond_markers(frame_id, color, text, offset_z, size)
            else:
                # fallback: сфера с текстом
                text_marker = Marker()
                text_marker.header.frame_id = frame_id
                text_marker.header.stamp = self.get_clock().now().to_msg()
                text_marker.ns = 'dual_arm'
                text_marker.id = marker_id_counter
                text_marker.type = Marker.TEXT_VIEW_FACING
                text_marker.action = Marker.ADD
                text_marker.text = text
                text_marker.pose.position.z = offset_z
                text_marker.pose.orientation.w = 1.0
                text_marker.scale.x = 0.1
                text_marker.scale.y = 0.1
                text_marker.scale.z = 0.1
                text_marker.color.r = color[0]
                text_marker.color.g = color[1]
                text_marker.color.b = color[2]
                text_marker.color.a = 1.0
                text_marker.lifetime = rclpy.duration.Duration(seconds=2.0).to_msg()
                markers = [text_marker]

            for m in markers:
                m.id = marker_id_counter
                marker_array.markers.append(m)
                marker_id_counter += 1

        self.pub.publish(marker_array)

def main(args=None):
    rclpy.init(args=args)
    node = MarkerPublisher()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        node.get_logger().info('MarkerPublisher shutting down gracefully...')
    finally:
        node.destroy_node()
        if rclpy.ok():
            rclpy.shutdown()
if __name__ == '__main__':
    main()