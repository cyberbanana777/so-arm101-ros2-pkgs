#!/usr/bin/env python3

# Copyright (c) 2026 Alice Zenina and Alexander Grachev RTU MIREA (Russia)
# SPDX-License-Identifier: MIT
# Details in the LICENSE file in the root of the package.

import rclpy
from rclpy.node import Node
from rclpy.qos import QoSProfile, ReliabilityPolicy, DurabilityPolicy
from rclpy.action import ActionClient
from control_msgs.action import GripperCommand
from soarm101_interfaces.msg import MotorStates
from std_srvs.srv import SetBool


class GripperRelay(Node):
    def __init__(self):
        super().__init__('gripper_relay')
        self.get_logger().info('GripperRelay node started')

        # ---- Parameters ----
        self.declare_parameter('max_effort', 10.0)       # max torque (N)
        self.declare_parameter('joint_name', 'gripper_jaw_joint')
        self.declare_parameter('enabled', True)
        self.enabled = bool(self.get_parameter('enabled').value)
        self.max_effort = self.get_parameter('max_effort').value
        self.joint_name = self.get_parameter('joint_name').value
        self.get_logger().info(
            f'Relaying joint: {self.joint_name} with max_effort={self.max_effort}'
        )

        # ---- QoS ----
        qos = QoSProfile(
            depth=10,
            reliability=ReliabilityPolicy.BEST_EFFORT,
            durability=DurabilityPolicy.VOLATILE
        )

        # ---- Subscription to leader's telemetry ----
        self.sub = self.create_subscription(
            MotorStates,
            '/leader/soarm101_telemetry_controller/motor_states',
            self.leader_callback,
            qos
        )
        self.get_logger().info(
            'Subscribed to /leader/soarm101_telemetry_controller/motor_states '
            '(BEST_EFFORT)'
        )

        # ---- Action client for follower's gripper ----
        self.action_client = ActionClient(
            self,
            GripperCommand,
            '/follower/gripper_controller/gripper_cmd'
        )
        self.get_logger().info(
            'Action client for /follower/gripper_controller/gripper_cmd created'
        )

        # ---- Pause / resume (used by playback tools) ----
        self.create_service(SetBool, '~/set_enabled', self.set_enabled_callback)

        # ---- Store last sent position to avoid spamming ----
        self.last_position = None
        self.last_goal_handle = None

    def set_enabled_callback(self, request, response):
        self.enabled = request.data
        self.last_position = None  # resend the leader position on resume
        response.success = True
        response.message = 'enabled' if self.enabled else 'paused'
        self.get_logger().info(f'Relay {response.message}')
        return response

    def leader_callback(self, msg: MotorStates):
        if not self.enabled:
            return
        target_position = None
        for motor in msg.motors:
            if motor.joint_name == self.joint_name:
                target_position = motor.position
                break

        if target_position is None:
            self.get_logger().warn(
                f'Joint {self.joint_name} not found in motor states'
            )
            return

        if (self.last_position is not None and
                abs(target_position - self.last_position) < 1e-6):
            return

        self.last_position = target_position

        # ---- Create goal ----
        goal = GripperCommand.Goal()
        goal.command.position = target_position
        goal.command.max_effort = self.max_effort

        self.get_logger().debug(
            f'Sending gripper goal: position={target_position:.4f}, '
            f'max_effort={self.max_effort}'
        )

        # ---- Send goal ----
        if not self.action_client.wait_for_server(timeout_sec=0.1):
            self.get_logger().warn('Gripper action server not available, skipping')
            return

        send_goal_future = self.action_client.send_goal_async(goal)
        send_goal_future.add_done_callback(self.goal_response_callback)

    def goal_response_callback(self, future):
        goal_handle = future.result()
        if not goal_handle.accepted:
            self.get_logger().warn('Gripper goal rejected')
            return

        self.get_logger().debug('Gripper goal accepted')


def main(args=None):
    rclpy.init(args=args)
    node = GripperRelay()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        node.get_logger().info('GripperRelay shutting down gracefully...')
    finally:
        node.destroy_node()
        if rclpy.ok():
            rclpy.shutdown()


if __name__ == '__main__':
    main()