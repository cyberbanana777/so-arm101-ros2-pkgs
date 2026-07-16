#include <rclcpp/rclcpp.hpp>
#include <rclcpp_action/rclcpp_action.hpp>
#include <trajectory_msgs/msg/joint_trajectory.hpp>
#include <control_msgs/action/follow_joint_trajectory.hpp>
#include <chrono>

using namespace std::chrono_literals;
using FollowJointTrajectory = control_msgs::action::FollowJointTrajectory;

class JointGoalSender : public rclcpp::Node
{
public:
  JointGoalSender()
  : Node("joint_goal_sender")
  {
    // Объявляем параметры с целевыми углами (в радианах)
    this->declare_parameter("shoulder_pan", 0.0);
    this->declare_parameter("shoulder_lift", 0.0);
    this->declare_parameter("elbow_flex", 0.0);
    this->declare_parameter("wrist_flex", 0.0);
    this->declare_parameter("wrist_roll", 0.0);
    this->declare_parameter("goal_time", 2.0);  // время движения

    action_client_ = rclcpp_action::create_client<FollowJointTrajectory>(
      this, "/joint_trajectory_controller/follow_joint_trajectory");

    // Ждём доступности action-сервера
    if (!action_client_->wait_for_action_server(5s)) {
      RCLCPP_ERROR(this->get_logger(), "Action server not available");
      rclcpp::shutdown();
      return;
    }

    send_goal();
  }

private:
  void send_goal()
  {
    auto goal_msg = FollowJointTrajectory::Goal();
    goal_msg.trajectory.joint_names = {
      "shoulder_pan_joint",
      "shoulder_lift_joint",
      "elbow_flex_joint",
      "wrist_flex_joint",
      "wrist_roll_joint"
    };

    trajectory_msgs::msg::JointTrajectoryPoint point;
    point.positions = {
      this->get_parameter("shoulder_pan").as_double(),
      this->get_parameter("shoulder_lift").as_double(),
      this->get_parameter("elbow_flex").as_double(),
      this->get_parameter("wrist_flex").as_double(),
      this->get_parameter("wrist_roll").as_double()
    };
    point.time_from_start = rclcpp::Duration::from_seconds(
      this->get_parameter("goal_time").as_double());

    goal_msg.trajectory.points.push_back(point);

    RCLCPP_INFO(this->get_logger(), "Sending goal...");
    auto send_goal_options = rclcpp_action::Client<FollowJointTrajectory>::SendGoalOptions();
    send_goal_options.result_callback =
      [this](const auto & result) {
        RCLCPP_INFO(this->get_logger(), "Goal result: %s",
          result.code == rclcpp_action::ResultCode::SUCCEEDED ? "SUCCEEDED" : "FAILED");
        rclcpp::shutdown();
      };
    action_client_->async_send_goal(goal_msg, send_goal_options);
  }

  rclcpp_action::Client<FollowJointTrajectory>::SharedPtr action_client_;
};

int main(int argc, char * argv[])
{
  rclcpp::init(argc, argv);
  auto node = std::make_shared<JointGoalSender>();
  rclcpp::spin(node);
  return 0;
}