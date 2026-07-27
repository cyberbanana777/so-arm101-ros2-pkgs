// Copyright (c) 2026 Alice Zenina and Alexander Grachev RTU MIREA (Russia)
// SPDX-License-Identifier: MIT
// Details in the LICENSE file in the root of the package.

#pragma once

#include <memory>
#include <string>
#include <vector>
#include <unordered_map>

#include "controller_interface/controller_interface.hpp"
#include "rclcpp_lifecycle/node_interfaces/lifecycle_node_interface.hpp"
#include "rclcpp_lifecycle/state.hpp"
#include "rclcpp_lifecycle/lifecycle_publisher.hpp"

#include "soarm101_interfaces/msg/motor_states.hpp"

namespace soarm101_telemetry_controller
{

class ServoTelemetryController : public controller_interface::ControllerInterface
{
public:
  controller_interface::CallbackReturn on_init() override;
  controller_interface::InterfaceConfiguration command_interface_configuration() const override;
  controller_interface::InterfaceConfiguration state_interface_configuration() const override;
  controller_interface::CallbackReturn on_configure(const rclcpp_lifecycle::State & previous_state) override;
  controller_interface::CallbackReturn on_activate(const rclcpp_lifecycle::State & previous_state) override;
  controller_interface::CallbackReturn on_deactivate(const rclcpp_lifecycle::State & previous_state) override;
  controller_interface::CallbackReturn on_cleanup(const rclcpp_lifecycle::State & previous_state) override;
  controller_interface::CallbackReturn on_error(const rclcpp_lifecycle::State & previous_state) override;
  controller_interface::CallbackReturn on_shutdown(const rclcpp_lifecycle::State & previous_state) override;
  controller_interface::return_type update(const rclcpp::Time & time, const rclcpp::Duration & period) override;

private:
  std::vector<std::string> joint_names_;
  std::vector<int64_t> motor_ids_;
  std::vector<std::string> interface_names_;

  // Индексы в state_interfaces_ для каждого сустава и имени интерфейса
  std::vector<std::unordered_map<std::string, size_t>> joint_state_indices_;

  rclcpp_lifecycle::LifecyclePublisher<soarm101_interfaces::msg::MotorStates>::SharedPtr publisher_;

  bool parse_parameters();
};

}  // namespace soarm101_telemetry_controller