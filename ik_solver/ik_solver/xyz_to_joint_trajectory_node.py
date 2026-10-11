#!/usr/bin/env python3
"""
ROS 2 узел ik_solver: принимает целевую точку схвата (x, y, z в системе base_link,
метры) и публикует trajectory_msgs/JointTrajectory для follower/joint_trajectory_controller.

Топики:
  подписка:   ~/target_position  (geometry_msgs/Point)   -> /ik_solver/target_position
              /follower/joint_states (sensor_msgs/JointState), если track_state = true
  публикация: /follower/joint_trajectory_controller/joint_trajectory

Параметры: см. config/ik_solver.yaml.
  pitch_deg        угол подхода: 0 = схват строго вниз, +-90 = горизонтально
  roll_deg         поворот схвата вокруг своей оси
  duration_sec     минимальное время движения
  max_speed_rad_s  ограничение скорости: время увеличивается, если надо
  dry_run          true = только считать и писать в лог, ничего не публиковать
  joint_scale, joint_zero_offset_deg   поправка калибровки (по умолчанию нет)
"""
import math

from builtin_interfaces.msg import Duration
from geometry_msgs.msg import Point
import rclpy
from rclpy.node import Node
from sensor_msgs.msg import JointState
from trajectory_msgs.msg import JointTrajectory, JointTrajectoryPoint

from ik_solver.calibration_correction import state_to_true, true_to_command
from ik_solver.so101_ik import solve_ik, UnreachableError

JOINT_ORDER = [
    'shoulder_pan_joint',
    'shoulder_lift_joint',
    'elbow_flex_joint',
    'wrist_flex_joint',
    'wrist_roll_joint',
]
SEED_KEYS = ['shoulder_pan', 'shoulder_lift', 'elbow_flex', 'wrist_flex', 'wrist_roll']
DEG = math.pi / 180.0


class IKSolverNode(Node):

    def __init__(self):
        super().__init__('ik_solver')

        self.declare_parameter('pitch_deg', 0.0)
        self.declare_parameter('roll_deg', 0.0)
        self.declare_parameter('duration_sec', 3.0)
        self.declare_parameter('max_speed_rad_s', 0.8)
        self.declare_parameter('track_state', True)
        self.declare_parameter('dry_run', False)
        self.declare_parameter(
            'output_topic', '/follower/joint_trajectory_controller/joint_trajectory')
        self.declare_parameter('joint_states_topic', '/follower/joint_states')
        self.declare_parameter('joint_scale', [1.0, 1.0, 1.0, 1.0, 1.0])
        self.declare_parameter('joint_zero_offset_deg', [0.0, 0.0, 0.0, 0.0, 0.0])

        self._last_state = None
        self._warned_correction = False

        self._pub = self.create_publisher(
            JointTrajectory, self._get('output_topic'), 10)
        self.create_subscription(Point, '~/target_position', self._on_target, 10)
        if self._get('track_state'):
            self.create_subscription(
                JointState, self._get('joint_states_topic'), self._on_joint_state, 10)

        self.get_logger().info(
            'ik_solver готов: жду geometry_msgs/Point на ~/target_position '
            '(base_link, метры). dry_run=%s' % self._get('dry_run'))

    def _get(self, name):
        return self.get_parameter(name).value

    def _correction(self):
        scale = list(self._get('joint_scale'))
        offset_deg = list(self._get('joint_zero_offset_deg'))
        if len(scale) != 5 or len(offset_deg) != 5 or any(abs(s) < 1e-6 for s in scale):
            if not self._warned_correction:
                self.get_logger().warn(
                    'joint_scale / joint_zero_offset_deg заданы неверно '
                    '(нужно по 5 чисел, scale != 0): поправка отключена.')
                self._warned_correction = True
            return [1.0] * 5, [0.0] * 5
        return scale, [d * DEG for d in offset_deg]

    def _on_joint_state(self, msg):
        self._last_state = dict(zip(msg.name, msg.position))

    def _on_target(self, msg):
        theta3 = float(self._get('pitch_deg')) * DEG
        roll = float(self._get('roll_deg')) * DEG
        scale, offset = self._correction()

        seed = None
        q_now = None
        if self._last_state is not None:
            c = [self._last_state.get(name) for name in JOINT_ORDER]
            if all(v is not None for v in c):
                q_now = state_to_true(c, scale, offset)
                seed = dict(zip(SEED_KEYS, q_now))

        try:
            sol = solve_ik(msg.x, msg.y, msg.z, theta3=theta3, roll=roll, seed=seed)
        except UnreachableError as exc:
            self.get_logger().warn(
                'Точка (%.3f, %.3f, %.3f) недостижима: %s' % (msg.x, msg.y, msg.z, exc))
            return

        q_true = sol.as_list()
        if not sol.within_limits():
            self.get_logger().warn(
                'IK нашла решение, но оно вне лимитов суставов: %s. Не отправляю.' % q_true)
            return

        cmd = true_to_command(q_true, scale, offset)

        duration = float(self._get('duration_sec'))
        max_speed = float(self._get('max_speed_rad_s'))
        if q_now is not None and max_speed > 0.0:
            max_delta = max(abs(a - b) for a, b in zip(q_true, q_now))
            duration = max(duration, max_delta / max_speed)

        self.get_logger().info(
            'IK: pan=%.3f lift=%.3f elbow=%.3f wrist_flex=%.3f roll=%.3f рад, время %.1f с'
            % (q_true[0], q_true[1], q_true[2], q_true[3], q_true[4], duration))

        if self._get('dry_run'):
            self.get_logger().info('dry_run: команда НЕ отправлена.')
            return

        point = JointTrajectoryPoint()
        point.positions = [float(v) for v in cmd]
        sec = int(duration)
        point.time_from_start = Duration(sec=sec, nanosec=int((duration - sec) * 1e9))

        traj = JointTrajectory()
        traj.joint_names = JOINT_ORDER
        traj.points = [point]
        self._pub.publish(traj)


def main(args=None):
    rclpy.init(args=args)
    node = IKSolverNode()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        if rclpy.ok():
            rclpy.shutdown()


if __name__ == '__main__':
    main()
