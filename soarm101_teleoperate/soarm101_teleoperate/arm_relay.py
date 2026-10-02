#!/usr/bin/env python3

# Copyright (c) 2026 Alice Zenina and Alexander Grachev RTU MIREA (Russia)
# SPDX-License-Identifier: MIT
# Details in the LICENSE file in the root of the package.

import rclpy
from rclpy.node import Node
from rclpy.qos import QoSProfile, ReliabilityPolicy, DurabilityPolicy
from trajectory_msgs.msg import JointTrajectory, JointTrajectoryPoint
from soarm101_interfaces.msg import MotorStates
from rclpy.duration import Duration
from std_msgs.msg import Header
from std_srvs.srv import SetBool


class LeaderFollowerRelay(Node):
    def __init__(self):
        super().__init__('leader_follower_relay')
        self.get_logger().info('LeaderFollowerRelay node started')

        # ---- Parameters ----
        self.declare_parameter('command_timeout', 0.03)  # time to reach target, ~1-3 telemetry periods
        self.declare_parameter('publish_rate', 0.0)  # 0 = relay every leader message
        self.declare_parameter('ignored_joints', ['gripper_jaw_joint'])
        self.declare_parameter('enabled', True)
        self.declare_parameter('resume_time', 2.0)  # s, smooth catch-up after resume

        self.command_timeout = self.get_parameter('command_timeout').value
        self.publish_rate = self.get_parameter('publish_rate').value
        self.ignored_joints = self.get_parameter('ignored_joints').value
        self.enabled = bool(self.get_parameter('enabled').value)
        self.resume_time = float(self.get_parameter('resume_time').value)
        self._resume_pending = False  # next command must be a slow catch-up move
        self._hold_until = None       # ignore leader until the catch-up is done
        self.get_logger().info(f'Ignored joints: {self.ignored_joints}')

        # ---- QoS ----
        qos = QoSProfile(
            depth=10,
            reliability=ReliabilityPolicy.BEST_EFFORT,
            durability=DurabilityPolicy.VOLATILE
        )

        self.sub = self.create_subscription(
            MotorStates,
            '/leader/soarm101_telemetry_controller/motor_states',
            self.leader_callback,
            qos
        )
        self.get_logger().info(
            'Subscribed to /leader/soarm101_telemetry_controller/motor_states'
        )

        # ---- Publishers ----
        self.cmd_pub = self.create_publisher(
            JointTrajectory,
            '/follower/joint_trajectory_controller/joint_trajectory',
            10
        )
        self.get_logger().info(
            'Publishing to /follower/joint_trajectory_controller/joint_trajectory'
        )

        # ---- Pause / resume (used by playback tools) ----
        self.create_service(SetBool, '~/set_enabled', self.set_enabled_callback)

        # ---- Frequency limitation ----
        self.last_publish_time = self.get_clock().now()
        self.min_interval = 1.0 / self.publish_rate if self.publish_rate > 0 else 0.0

    def set_enabled_callback(self, request, response):
        if request.data and not self.enabled:
            # The follower may be far from the leader after a pause:
            # first move there slowly instead of jumping.
            self._resume_pending = True
        self.enabled = request.data
        response.success = True
        response.message = 'enabled' if self.enabled else 'paused'
        self.get_logger().info(f'Relay {response.message}')
        return response

    def leader_callback(self, msg: MotorStates):
        if not self.enabled:
            return
        now = self.get_clock().now()
        if self._hold_until is not None:
            if now < self._hold_until:
                return
            self._hold_until = None

        # Frequency limitation
        if (now - self.last_publish_time).nanoseconds * 1e-9 < self.min_interval:
            return

        if not msg.motors:
            return

        # ---- Filtering joints ----
        joint_names = []
        positions = []

        for motor in msg.motors:
            if motor.joint_name in self.ignored_joints:
                continue
            joint_names.append(motor.joint_name)
            positions.append(motor.position)

        if not joint_names:
            return

        trajectory = JointTrajectory()
        trajectory.header = Header()
        # Zero stamp = "start executing on receipt". With a real stamp the
        # transport delay is eaten from time_from_start, which matters at ~30 ms.
        trajectory.header.frame_id = 'world_frame'

        trajectory.joint_names = joint_names

        point = JointTrajectoryPoint()
        point.positions = positions
        if self._resume_pending:
            point.time_from_start = Duration(seconds=self.resume_time).to_msg()
            self._hold_until = now + Duration(seconds=self.resume_time)
            self._resume_pending = False
        else:
            point.time_from_start = Duration(seconds=self.command_timeout).to_msg()

        trajectory.points = [point]

        # ---- Publish ----
        self.cmd_pub.publish(trajectory)
        self.last_publish_time = now


def main(args=None):
    rclpy.init(args=args)
    node = LeaderFollowerRelay()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        node.get_logger().info('LeaderFollowerRelay shutting down gracefully...')
    finally:
        node.destroy_node()
        if rclpy.ok():
            rclpy.shutdown()


if __name__ == '__main__':
    main()