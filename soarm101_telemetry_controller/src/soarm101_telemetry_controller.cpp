// Copyright (c) 2026 Alice Zenina and Alexander Grachev RTU MIREA (Russia)
// SPDX-License-Identifier: MIT
// Details in the LICENSE file in the root of the package.

#include "soarm101_telemetry_controller/soarm101_telemetry_controller.hpp"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "controller_interface/helpers.hpp"
#include "hardware_interface/types/hardware_interface_type_values.hpp"

namespace soarm101_telemetry_controller
{

// ---------- on_init ----------
controller_interface::CallbackReturn ServoTelemetryController::on_init()
{
  return controller_interface::CallbackReturn::SUCCESS;
}

// ---------- command_interface_configuration ----------
controller_interface::InterfaceConfiguration ServoTelemetryController::command_interface_configuration() const
{
  controller_interface::InterfaceConfiguration config;
  config.type = controller_interface::interface_configuration_type::NONE;
  // config.names остаётся пустым
  return config;
}

// ---------- state_interface_configuration ----------
controller_interface::InterfaceConfiguration ServoTelemetryController::state_interface_configuration() const
{
  controller_interface::InterfaceConfiguration config;
  config.type = controller_interface::interface_configuration_type::INDIVIDUAL;
  for (const auto & joint : joint_names_)
  {
    for (const auto & iface : interface_names_)
    {
      config.names.push_back(joint + "/" + iface);
    }
  }
  return config;
}

// ---------- on_configure ----------
controller_interface::CallbackReturn ServoTelemetryController::on_configure(const rclcpp_lifecycle::State & /*previous_state*/)
{
  if (!parse_parameters())
  {
    RCLCPP_ERROR(get_node()->get_logger(), "Failed to parse parameters");
    return controller_interface::CallbackReturn::FAILURE;
  }

  auto qos = rclcpp::QoS(rclcpp::SystemDefaultsQoS());
  qos.best_effort();
  publisher_ = get_node()->create_publisher<soarm101_interfaces::msg::MotorStates>(
    "~/motor_states",
    qos
  );

  RCLCPP_INFO(get_node()->get_logger(), "ServoTelemetryController configured");
  return controller_interface::CallbackReturn::SUCCESS;
}

// ---------- on_activate ----------
controller_interface::CallbackReturn ServoTelemetryController::on_activate(const rclcpp_lifecycle::State & /*previous_state*/)
{
  joint_state_indices_.clear();
  joint_state_indices_.resize(joint_names_.size());

  for (size_t i = 0; i < joint_names_.size(); ++i)
  {
    const auto & joint = joint_names_[i];
    for (const auto & iface : interface_names_)
    {
      const std::string full_name = joint + "/" + iface;
      bool found = false;
      for (size_t idx = 0; idx < state_interfaces_.size(); ++idx)
      {
        if (state_interfaces_[idx].get_name() == full_name)
        {
          joint_state_indices_[i][iface] = idx;
          found = true;
          break;
        }
      }
      if (!found)
      {
        RCLCPP_ERROR(
          get_node()->get_logger(),
          "State interface '%s' not found", full_name.c_str()
        );
        return controller_interface::CallbackReturn::FAILURE;
      }
    }
  }

  publisher_->on_activate();
  RCLCPP_INFO(get_node()->get_logger(), "ServoTelemetryController activated");
  return controller_interface::CallbackReturn::SUCCESS;
}

// ---------- on_deactivate ----------
controller_interface::CallbackReturn ServoTelemetryController::on_deactivate(const rclcpp_lifecycle::State & /*previous_state*/)
{
  joint_state_indices_.clear();
  publisher_->on_deactivate();
  return controller_interface::CallbackReturn::SUCCESS;
}

// ---------- on_cleanup ----------
controller_interface::CallbackReturn ServoTelemetryController::on_cleanup(const rclcpp_lifecycle::State & /*previous_state*/)
{
  joint_state_indices_.clear();
  publisher_.reset();
  return controller_interface::CallbackReturn::SUCCESS;
}

// ---------- on_error ----------
controller_interface::CallbackReturn ServoTelemetryController::on_error(const rclcpp_lifecycle::State & /*previous_state*/)
{
  joint_state_indices_.clear();
  return controller_interface::CallbackReturn::SUCCESS;
}

// ---------- on_shutdown ----------
controller_interface::CallbackReturn ServoTelemetryController::on_shutdown(const rclcpp_lifecycle::State & /*previous_state*/)
{
  joint_state_indices_.clear();
  publisher_.reset();
  return controller_interface::CallbackReturn::SUCCESS;
}

// ---------- update ----------
controller_interface::return_type ServoTelemetryController::update(
  const rclcpp::Time & /*time*/,
  const rclcpp::Duration & /*period*/)
{
  if (joint_names_.empty() || !publisher_ || !publisher_->is_activated())
    return controller_interface::return_type::OK;

  auto msg = soarm101_interfaces::msg::MotorStates();
  msg.motors.resize(joint_names_.size());

  for (size_t i = 0; i < joint_names_.size(); ++i)
  {
    const auto & index_map = joint_state_indices_[i];
    auto & motor = msg.motors[i];

    motor.motor_id = static_cast<int32_t>(motor_ids_[i]);
    motor.joint_name = joint_names_[i];

    auto it = index_map.find("position");
    motor.position = (it != index_map.end()) ? state_interfaces_[it->second].get_value() : 0.0;

    it = index_map.find("velocity");
    motor.velocity = (it != index_map.end()) ? state_interfaces_[it->second].get_value() : 0.0;

    it = index_map.find("effort");
    motor.effort = (it != index_map.end()) ? state_interfaces_[it->second].get_value() : 0.0;

    it = index_map.find("temperature");
    motor.temperature = (it != index_map.end()) ? state_interfaces_[it->second].get_value() : 0.0;

    it = index_map.find("voltage");
    motor.voltage = (it != index_map.end()) ? state_interfaces_[it->second].get_value() : 0.0;

    it = index_map.find("current");
    motor.current = (it != index_map.end()) ? state_interfaces_[it->second].get_value() : 0.0;

    it = index_map.find("moving_flag");
    if (it != index_map.end())
      motor.moving_flag = (state_interfaces_[it->second].get_value() > 0.5);
    else
      motor.moving_flag = false;

    it = index_map.find("max_torque");
    motor.max_torque = (it != index_map.end()) ? state_interfaces_[it->second].get_value() : 0.0;

    it = index_map.find("enable_torque");
    motor.enable_torque = (it != index_map.end()) ? state_interfaces_[it->second].get_value() : 0.0;
  }

  publisher_->publish(msg);
  return controller_interface::return_type::OK;
}

// ---------- parse_parameters ----------
bool ServoTelemetryController::parse_parameters()
{
  auto node = get_node();

  if (!node->get_parameter("joints", joint_names_) || joint_names_.empty())
  {
    RCLCPP_ERROR(node->get_logger(), "Parameter 'joints' not set or empty");
    return false;
  }

  if (!node->get_parameter("motor_ids", motor_ids_))
  {
    motor_ids_.resize(joint_names_.size());
    for (size_t i = 0; i < joint_names_.size(); ++i)
      motor_ids_[i] = i + 1;
    RCLCPP_WARN(node->get_logger(), "motor_ids not provided, using indices starting from 1");
  }
  else if (motor_ids_.size() != joint_names_.size())
  {
    RCLCPP_ERROR(node->get_logger(), "motor_ids size (%zu) != joints size (%zu)",
                 motor_ids_.size(), joint_names_.size());
    return false;
  }

  if (!node->get_parameter("interface_names", interface_names_))
  {
    interface_names_ = {"position", "velocity", "effort", "temperature",
                        "voltage", "current", "moving_flag", "max_torque", "enable_torque" };
    RCLCPP_INFO(node->get_logger(), "Using default interface_names");
  }
  else if (interface_names_.empty())
  {
    RCLCPP_ERROR(node->get_logger(), "interface_names is empty");
    return false;
  }

  return true;
}

}  // namespace soarm101_telemetry_controller

// ---------- Plugin registration ----------
#include "pluginlib/class_list_macros.hpp"
PLUGINLIB_EXPORT_CLASS(
  soarm101_telemetry_controller::ServoTelemetryController,
  controller_interface::ControllerInterface
)