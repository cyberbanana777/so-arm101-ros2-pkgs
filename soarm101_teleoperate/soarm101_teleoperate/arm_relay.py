#!/usr/bin/env python3

import rclpy
from rclpy.node import Node
from rclpy.qos import QoSProfile, ReliabilityPolicy, DurabilityPolicy
from trajectory_msgs.msg import JointTrajectory, JointTrajectoryPoint
from soarm101_interfaces.msg import MotorStates
from rclpy.duration import Duration
from std_msgs.msg import Header

class LeaderFollowerRelay(Node):
    def __init__(self):
        super().__init__('leader_follower_relay')
        self.get_logger().info('LeaderFollowerRelay node started')

        # ---- Параметры ----
        self.declare_parameter('command_timeout', 0.0015)  # время на движение к цели
        self.declare_parameter('publish_rate', 50.0)
        self.declare_parameter('ignored_joints', ['gripper_jaw_joint'])

        self.command_timeout = self.get_parameter('command_timeout').value
        self.publish_rate = self.get_parameter('publish_rate').value
        self.ignored_joints = self.get_parameter('ignored_joints').value
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
        self.get_logger().info('Subscribed to /leader/soarm101_telemetry_controller/motor_states')

        # ---- Публикация ----
        self.cmd_pub = self.create_publisher(
            JointTrajectory,
            '/follower/joint_trajectory_controller/joint_trajectory',
            10
        )
        self.get_logger().info('Publishing to /follower/joint_trajectory_controller/joint_trajectory')

        # ---- Ограничение частоты ----
        self.last_publish_time = self.get_clock().now()
        self.min_interval = 1.0 / self.publish_rate if self.publish_rate > 0 else 0.0

    def leader_callback(self, msg: MotorStates):
        now = self.get_clock().now()

        # Ограничение частоты
        if (now - self.last_publish_time).nanoseconds * 1e-9 < self.min_interval:
            return

        if not msg.motors:
            return

        # ---- Фильтрация суставов ----
        joint_names = []
        positions = []

        for motor in msg.motors:
            if motor.joint_name in self.ignored_joints:
                continue
            joint_names.append(motor.joint_name)
            positions.append(motor.position)

        if not joint_names:
            return

        # ---- Формируем траекторию с "нулевым" временем относительно сейчас ----
        # Этот способ заставляет контроллер мгновенно начать движение к новой цели.
        # Используем текущее время как старт, а цель достигается через command_timeout.
        trajectory = JointTrajectory()
        trajectory.header = Header()
        trajectory.header.stamp = now.to_msg()  # Важно: текущее время
        trajectory.header.frame_id = 'world_frame'

        trajectory.joint_names = joint_names

        point = JointTrajectoryPoint()
        point.positions = positions
        # Устанавливаем время для достижения цели (относительно времени в header)
        point.time_from_start = Duration(seconds=self.command_timeout).to_msg()

        trajectory.points = [point]

        # ---- Публикация ----
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