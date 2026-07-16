#include <rclcpp/rclcpp.hpp>
#include <rclcpp_action/rclcpp_action.hpp>
#include <control_msgs/action/gripper_command.hpp>
#include <chrono>

using namespace std::chrono_literals;
using GripperCommand = control_msgs::action::GripperCommand;

class GripperGoalSender : public rclcpp::Node
{
public:
  GripperGoalSender()
  : Node("gripper_goal_sender")
  {
    // Параметр: целевая ширина (0.0 – закрыт, ~0.03 – открыт)
    this->declare_parameter("position", 0.0);
    this->declare_parameter("max_effort", 10.0);

    action_client_ = rclcpp_action::create_client<GripperCommand>(
      this, "/gripper_controller/gripper_cmd");

    if (!action_client_->wait_for_action_server(5s)) {
      RCLCPP_ERROR(this->get_logger(), "Gripper action server not available");
      rclcpp::shutdown();
      return;
    }

    send_command();
  }

private:
  void send_command()
  {
    auto goal_msg = GripperCommand::Goal();
    goal_msg.command.position = this->get_parameter("position").as_double();
    goal_msg.command.max_effort = this->get_parameter("max_effort").as_double();

    RCLCPP_INFO(this->get_logger(), "Sending gripper command (position=%.3f)...",
                goal_msg.command.position);
    auto send_goal_options = rclcpp_action::Client<GripperCommand>::SendGoalOptions();
    send_goal_options.result_callback =
      [this](const auto & result) {
        RCLCPP_INFO(this->get_logger(), "Gripper goal result: %s",
          result.code == rclcpp_action::ResultCode::SUCCEEDED ? "SUCCEEDED" : "FAILED");
        rclcpp::shutdown();
      };
    action_client_->async_send_goal(goal_msg, send_goal_options);
  }

  rclcpp_action::Client<GripperCommand>::SharedPtr action_client_;
};

int main(int argc, char * argv[])
{
  rclcpp::init(argc, argv);
  auto node = std::make_shared<GripperGoalSender>();
  rclcpp::spin(node);
  return 0;
}