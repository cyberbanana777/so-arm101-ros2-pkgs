// Copyright 2024 The HuggingFace Inc. team. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
//
// Copyright 2026 Alice Zenina and Alexander Grachev RTU MIREA (Russia). All rights reserved.
// Modifications to the original work are licensed under the same Apache 2.0 license.
//
// =============================================================================
// MODIFICATIONS (by Alice Zenina and Alexander Grachev, 2026-07-24)
// =============================================================================
// - Aggregated all motor data into single `Motor` struct.
// - Added park position support (`park_positions` parameter).
// - Added configurable default speed/accel (`default_speed`, `default_accel`).
// - Precomputed calibration coefficients for faster conversion.
// - Replaced dynamic vectors with std::array in write().
// - Unified read logic into `readMotorData()`.
// - Fixed motor_id vs index bug in sensor reading.
// - Changed moving_flag type to double for StateInterface compatibility.
// - Added `moveToParkPosition()` and `parseParkPositions()`.
// =============================================================================  


#include "soarm101_hardware/soarm101_hardware_leader.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <limits>
#include <memory>
#include <sstream>
#include <thread>
#include <vector>
#include <yaml-cpp/yaml.h>


#include "hardware_interface/types/hardware_interface_type_values.hpp"
#include "rclcpp/rclcpp.hpp"

// For convenience
using hardware_interface::HW_IF_POSITION;
using hardware_interface::HW_IF_VELOCITY;
using hardware_interface::HW_IF_EFFORT;

namespace soarm101_hardware_leader {

SOARM101SystemHardwareLeader::SOARM101SystemHardwareLeader()
: driver_initialized_(false)
{
}

SOARM101SystemHardwareLeader::~SOARM101SystemHardwareLeader()
{
  if (driver_initialized_) {
    servo_driver_.end();
  }
}

// ----------------------------------------------------------------------------
// on_init
// ----------------------------------------------------------------------------
hardware_interface::CallbackReturn
SOARM101SystemHardwareLeader::on_init(const hardware_interface::HardwareInfo & info)
{
  if (hardware_interface::SystemInterface::on_init(info) !=
      hardware_interface::CallbackReturn::SUCCESS) {
    return hardware_interface::CallbackReturn::ERROR;
  }

  // --- Port ---
  port_ = info_.hardware_parameters["port"];
  if (port_.empty()) {
    port_ = "/dev/ttyACM0";
  }

  // --- Baudrate ---
  std::string baudrate_str = info_.hardware_parameters["baudrate"];
  if (baudrate_str.empty()) {
    baudrate_ = 1000000;
  } else {
    baudrate_ = std::stoi(baudrate_str);
  }

  // --- Calibration file ---
  calibration_file_ = info_.hardware_parameters["calibration_file"];

  // --- Default speed ---
  std::string speed_str = info_.hardware_parameters["default_speed"];
  if (!speed_str.empty()) {
    default_speed_ = static_cast<u16>(std::stoi(speed_str));
    RCLCPP_INFO(rclcpp::get_logger("SOARM101SystemHardwareLeader"), "default_speed = %d", default_speed_);
  }

  // --- Default acceleration ---
  std::string accel_str = info_.hardware_parameters["default_accel"];
  if (!accel_str.empty()) {
    default_accel_ = static_cast<u8>(std::stoi(accel_str));
    RCLCPP_INFO(rclcpp::get_logger("SOARM101SystemHardwareLeader"), "default_accel = %d", default_accel_);
  }

  // --- Park position ---
  std::string park_str = info_.hardware_parameters["park_positions"];
  if (!park_str.empty()) {
    park_positions_ = parseParkPositions(park_str);
    if (park_positions_.size() == info_.joints.size()) {
      RCLCPP_INFO(rclcpp::get_logger("SOARM101SystemHardwareLeader"), "Park positions loaded.");
    } else {
      RCLCPP_WARN(rclcpp::get_logger("SOARM101SystemHardwareLeader"),
                  "park_positions size (%zu) != joints size (%zu), ignoring",
                  park_positions_.size(), info_.joints.size());
      park_positions_.clear();
    }
  }

  // --- Max torques ---
  std::string torqs_str = info_.hardware_parameters["max_torques"];
  if (!torqs_str.empty()) {
    max_torques_ = parseMaxTorques(torqs_str);
    if (max_torques_.size() == info_.joints.size()) {
      RCLCPP_INFO(rclcpp::get_logger("SOARM101SystemHardwareLeader"), "Max torques loaded. Text: %s", torqs_str.c_str() );
    } else {
      RCLCPP_WARN(rclcpp::get_logger("SOARM101SystemHardwareLeader"),
                  "max_torques size (%zu) != joints size (%zu), ignoring",
                  max_torques_.size(), info_.joints.size());
      max_torques_.clear();
    }
  }

  // --- Name mapping ---
  motor_ids_["shoulder_pan_joint"]   = 1;
  motor_ids_["shoulder_lift_joint"]  = 2;
  motor_ids_["elbow_flex_joint"]     = 3;
  motor_ids_["wrist_flex_joint"]     = 4;
  motor_ids_["wrist_roll_joint"]     = 5;
  motor_ids_["gripper_jaw_joint"]    = 6;

  motors_.resize(info_.joints.size());

  for (size_t i = 0; i < info_.joints.size(); ++i) {
    const auto & joint = info_.joints[i];
    auto & motor = motors_[i];
    auto it = motor_ids_.find(joint.name);
    if (it == motor_ids_.end()) {
      RCLCPP_ERROR(rclcpp::get_logger("SOARM101SystemHardwareLeader"),
                   "Unknown joint name: %s", joint.name.c_str());
      return hardware_interface::CallbackReturn::ERROR;
    }
    motor.id = it->second;
    motor.joint_name = joint.name;
  }

  RCLCPP_INFO(rclcpp::get_logger("SOARM101SystemHardwareLeader"), "on_init() finished successfully");
  return hardware_interface::CallbackReturn::SUCCESS;
}

// ----------------------------------------------------------------------------
// on_configure
// ----------------------------------------------------------------------------
hardware_interface::CallbackReturn
SOARM101SystemHardwareLeader::on_configure(const rclcpp_lifecycle::State & /*previous_state*/)
{
  RCLCPP_INFO(rclcpp::get_logger("SOARM101SystemHardwareLeader"), "Configuring...");

  // --- Load calibration ---
  if (!calibration_file_.empty() && !loadCalibration()) {
    RCLCPP_ERROR(
      rclcpp::get_logger("SOARM101SystemHardwareLeader"),
      "Failed to load calibration from: %s", calibration_file_.c_str());
    return hardware_interface::CallbackReturn::ERROR;
  }

  // --- Connect to bus ---
  if (!servo_driver_.begin(baudrate_, port_.c_str())) {
    RCLCPP_ERROR(
      rclcpp::get_logger("SOARM101SystemHardwareLeader"),
      "Failed to connect to motor bus on port %s.", port_.c_str());
    return hardware_interface::CallbackReturn::ERROR;
  }
  driver_initialized_ = true;

  // --- Initialise commands ---
  for (size_t i = 0; i < motors_.size(); ++i) {
    auto & motor = motors_[i];
    if (std::isnan(motor.sensors.position)) {
      motor.sensors.position = 0.0;
    }
    motor.command_position = motor.sensors.position;
  }

  if (max_torques_.size() == motors_.size()) {
      for (size_t i = 0; i < motors_.size(); ++i) {
          motors_[i].max_torque = max_torques_[i];
      }
  } else {
      RCLCPP_WARN(rclcpp::get_logger("SOARM101SystemHardwareLeader"),
                  "max_torques not set or size mismatch, using default 0.0");
      for (auto & motor : motors_) {
          motor.max_torque = 0.0;
      }
  }


  RCLCPP_INFO(rclcpp::get_logger("SOARM101SystemHardwareLeader"), "Configuration completed");
  return hardware_interface::CallbackReturn::SUCCESS;
}

// ----------------------------------------------------------------------------
// export_state_interfaces
// ----------------------------------------------------------------------------
std::vector<hardware_interface::StateInterface>
SOARM101SystemHardwareLeader::export_state_interfaces()
{
  std::vector<hardware_interface::StateInterface> state_interfaces;
  for (size_t i = 0; i < info_.joints.size(); ++i) {
    const auto & joint = info_.joints[i];
    auto & motor = motors_[i];

    state_interfaces.emplace_back(joint.name, HW_IF_POSITION, &motor.sensors.position);
    state_interfaces.emplace_back(joint.name, HW_IF_VELOCITY, &motor.sensors.velocity);
    state_interfaces.emplace_back(joint.name, HW_IF_EFFORT,   &motor.sensors.effort);
    state_interfaces.emplace_back(joint.name, "temperature",  &motor.sensors.temperature);
    state_interfaces.emplace_back(joint.name, "voltage",      &motor.sensors.voltage);
    state_interfaces.emplace_back(joint.name, "current",      &motor.sensors.current);
    state_interfaces.emplace_back(joint.name, "moving_flag",  &motor.sensors.moving_flag);
    state_interfaces.emplace_back(joint.name, "max_torque",  &motor.max_torque);
    state_interfaces.emplace_back(joint.name, "enable_torque",  &motor.sensors.enable_torque);
  }
  return state_interfaces;
}

// ----------------------------------------------------------------------------
// export_command_interfaces
// ----------------------------------------------------------------------------
std::vector<hardware_interface::CommandInterface>
SOARM101SystemHardwareLeader::export_command_interfaces()
{
  return {};
}

// ----------------------------------------------------------------------------
// on_activate
// ----------------------------------------------------------------------------
hardware_interface::CallbackReturn
SOARM101SystemHardwareLeader::on_activate(const rclcpp_lifecycle::State & /*previous_state*/)
{
  RCLCPP_INFO(rclcpp::get_logger("SOARM101SystemHardwareLeader"), "Activating...");

  RCLCPP_INFO(rclcpp::get_logger("SOARM101SystemHardwareLeader"), "Current motor positions:");
  for (size_t i = 0; i < info_.joints.size(); ++i) {
    readMotorData(i);
    motors_[i].command_position = motors_[i].sensors.position;
    RCLCPP_INFO(
      rclcpp::get_logger("SOARM101SystemHardwareLeader"),
      "  %s: %.3f rad",
      info_.joints[i].name.c_str(),
      motors_[i].sensors.position);
  }

  RCLCPP_INFO(rclcpp::get_logger("SOARM101SystemHardwareLeader"), "Activation completed");
  return hardware_interface::CallbackReturn::SUCCESS;
}

// ----------------------------------------------------------------------------
// on_deactivate
// ----------------------------------------------------------------------------
hardware_interface::CallbackReturn
SOARM101SystemHardwareLeader::on_deactivate(const rclcpp_lifecycle::State & /*previous_state*/)
{
  RCLCPP_INFO(rclcpp::get_logger("SOARM101SystemHardwareLeader"), "Deactivating...");

  // --- Park ---
  if (!park_positions_.empty()) {
    moveToParkPosition();
  } else {
    RCLCPP_INFO(rclcpp::get_logger("SOARM101SystemHardwareLeader"), "No park position set, skipping movement.");
  }

  // --- Disable torque ---
  for (const auto & pair : motor_ids_) {
    int motor_id = pair.second;
    if (servo_driver_.EnableTorque(motor_id, DISABLE_TORQUE) == 0) {
      RCLCPP_ERROR(
        rclcpp::get_logger("SOARM101SystemHardwareLeader"),
        "Failed to disable torque for motor %d", motor_id);
      return hardware_interface::CallbackReturn::ERROR;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }

  RCLCPP_INFO(rclcpp::get_logger("SOARM101SystemHardwareLeader"), "Deactivation completed");
  return hardware_interface::CallbackReturn::SUCCESS;
}

// ----------------------------------------------------------------------------
// read
// ----------------------------------------------------------------------------
hardware_interface::return_type
SOARM101SystemHardwareLeader::read(const rclcpp::Time & /*time*/, const rclcpp::Duration & /*period*/)
{
  for (size_t i = 0; i < info_.joints.size(); ++i) {
    readMotorData(i);
  }
  return hardware_interface::return_type::OK;
}

// ----------------------------------------------------------------------------
// write
// ----------------------------------------------------------------------------
hardware_interface::return_type
SOARM101SystemHardwareLeader::write(const rclcpp::Time & /*time*/, const rclcpp::Duration & /*period*/)
{
  return hardware_interface::return_type::OK;
}

// ----------------------------------------------------------------------------
// loadCalibration
// ----------------------------------------------------------------------------
bool SOARM101SystemHardwareLeader::loadCalibration()
{
  if (calibration_file_.empty()) {
    RCLCPP_INFO(rclcpp::get_logger("SOARM101SystemHardwareLeader"),
                "No calibration file specified, using default values.");
    return true;
  }

  RCLCPP_INFO(rclcpp::get_logger("SOARM101SystemHardwareLeader"),
              "Loading calibration from: %s", calibration_file_.c_str());

  try {
    // Загружаем YAML-файл
    YAML::Node config = YAML::LoadFile(calibration_file_);

    // Проверяем, что корневой узел — словарь
    if (!config.IsMap()) {
      RCLCPP_ERROR(rclcpp::get_logger("SOARM101SystemHardwareLeader"),
                   "Calibration file root is not a map.");
      return false;
    }

    // Вывод содержимого для отладки (опционально)
    YAML::Emitter emitter;
    emitter << config;
    RCLCPP_INFO(rclcpp::get_logger("SOARM101SystemHardwareLeader"),
                "=== Calibration file content ===\n%s", emitter.c_str());

    // Проходим по всем ключам (joint_name)
    for (auto it = config.begin(); it != config.end(); ++it) {
      std::string joint_name = it->first.as<std::string>();
      YAML::Node joint_data = it->second;

      // Проверяем, что узел — словарь с нужными полями
      if (!joint_data.IsMap()) {
        RCLCPP_WARN(rclcpp::get_logger("SOARM101SystemHardwareLeader"),
                    "Skipping '%s' – not a map", joint_name.c_str());
        continue;
      }

      // Ищем соответствующий мотор по имени
      auto motor_it = motor_ids_.find(joint_name);
      if (motor_it == motor_ids_.end()) {
        RCLCPP_WARN(rclcpp::get_logger("SOARM101SystemHardwareLeader"),
                    "Unknown joint name '%s' in calibration file, skipping.",
                    joint_name.c_str());
        continue;
      }

      int motor_id = motor_it->second;

      // Извлекаем значения с проверкой наличия
      int drive_mode = 0;
      int range_min = 0;
      int range_max = 0;

      if (joint_data["drive_mode"]) {
        drive_mode = joint_data["drive_mode"].as<int>();
      }
      if (joint_data["range_min"]) {
        range_min = joint_data["range_min"].as<int>();
      } else {
        RCLCPP_WARN(rclcpp::get_logger("SOARM101SystemHardwareLeader"),
                    "range_min missing for '%s', using 0", joint_name.c_str());
      }
      if (joint_data["range_max"]) {
        range_max = joint_data["range_max"].as<int>();
      } else {
        RCLCPP_WARN(rclcpp::get_logger("SOARM101SystemHardwareLeader"),
                    "range_max missing for '%s', using 0", joint_name.c_str());
      }

      // Находим мотор в векторе motors_ и обновляем калибровку
      for (auto & motor : motors_) {
        if (motor.id == motor_id) {
          motor.calibration.drive_mode = drive_mode;
          motor.calibration.range_min = range_min;
          motor.calibration.range_max = range_max;
          updateCalibrationCoefficients(motor);
          RCLCPP_INFO(rclcpp::get_logger("SOARM101SystemHardwareLeader"),
                      "✅ Calibration for '%s' (ID %d): range_min=%d, range_max=%d",
                      joint_name.c_str(), motor_id, range_min, range_max);
          break;
        }
      }
    }

    RCLCPP_INFO(rclcpp::get_logger("SOARM101SystemHardwareLeader"),
                "Calibration loading finished.");
    return true;

  } catch (const YAML::Exception & e) {
    RCLCPP_ERROR(rclcpp::get_logger("SOARM101SystemHardwareLeader"),
                 "YAML parsing error: %s", e.what());
    return false;
  } catch (const std::exception & e) {
    RCLCPP_ERROR(rclcpp::get_logger("SOARM101SystemHardwareLeader"),
                 "Error loading calibration: %s", e.what());
    return false;
  }
}

// ----------------------------------------------------------------------------
// updateCalibrationCoefficients
// ----------------------------------------------------------------------------
void SOARM101SystemHardwareLeader::updateCalibrationCoefficients(Motor & motor)
{
  auto & calib = motor.calibration;
  double urdf_lower, urdf_upper;
  switch (motor.id) {
    case 1:  urdf_lower = -1.91986; urdf_upper =  1.91986; break;
    case 2:  urdf_lower = -1.74533; urdf_upper =  1.74533; break;
    case 3:  urdf_lower = -1.74533; urdf_upper =  1.5708;  break;
    case 4:  urdf_lower = -1.65806; urdf_upper =  1.65806; break;
    case 5:  urdf_lower = -2.79253; urdf_upper =  2.79253; break;
    case 6:  urdf_lower = -0.1745;  urdf_upper =  1.4483;  break;
    default: urdf_lower = -M_PI;    urdf_upper =  M_PI;    break;
  }
  calib.urdf_lower = urdf_lower;
  calib.urdf_upper = urdf_upper;

  double raw_range = calib.range_max - calib.range_min;
  double urdf_range = urdf_upper - urdf_lower;
  if (raw_range != 0.0) {
    calib.raw_to_rad_scale = urdf_range / raw_range;
    calib.rad_to_raw_scale = raw_range / urdf_range;
  } else {
    calib.raw_to_rad_scale = 1.0;
    calib.rad_to_raw_scale = 1.0;
    RCLCPP_WARN(rclcpp::get_logger("SOARM101SystemHardwareLeader"),
                "Zero raw range for motor %d, using scale=1", motor.id);
  }
  calib.raw_to_rad_offset = calib.range_min;
  calib.rad_to_raw_offset = urdf_lower;
}

// ----------------------------------------------------------------------------
// readMotorData
// ----------------------------------------------------------------------------
void SOARM101SystemHardwareLeader::readMotorData(size_t index)
{
  const std::string & joint_name = info_.joints[index].name;
  int motor_id = motor_ids_[joint_name];
  auto & motor = motors_[index];

  if (servo_driver_.FeedBack(motor_id) != 0) {
    int raw_pos         = servo_driver_.ReadPos(motor_id);
    int raw_velocity    = servo_driver_.ReadSpeed(motor_id);
    int raw_effort      = servo_driver_.ReadLoad(motor_id);
    int raw_temperature = servo_driver_.ReadTemper(motor_id);
    int raw_voltage     = servo_driver_.ReadVoltage(motor_id);
    int raw_current     = servo_driver_.ReadCurrent(motor_id);
    int raw_moving_flag = servo_driver_.ReadMove(motor_id);

    motor.sensors.position = rawToRadians(raw_pos, motor);
    motor.sensors.velocity = static_cast<double>(raw_velocity) / 4096.0 * 2.0 * M_PI;
    motor.sensors.effort   = static_cast<double>(raw_effort) / 1000.0 * motor.max_torque;
    motor.sensors.temperature = static_cast<double>(raw_temperature);
    motor.sensors.voltage  = static_cast<double>(raw_voltage) / 10.0;
    motor.sensors.current  = static_cast<double>(raw_current) / 1000.0;
    motor.sensors.moving_flag = static_cast<double>(raw_moving_flag);
  }
}

// ----------------------------------------------------------------------------
// rawToRadians
// ----------------------------------------------------------------------------
double SOARM101SystemHardwareLeader::rawToRadians(int raw_position, const Motor & motor)
{
  const auto & calib = motor.calibration;
  int clamped = std::max(calib.range_min, std::min(calib.range_max, raw_position));
  return (static_cast<double>(clamped) - calib.raw_to_rad_offset) * calib.raw_to_rad_scale + calib.urdf_lower;
}

// ----------------------------------------------------------------------------
// radiansToRaw
// ----------------------------------------------------------------------------
int SOARM101SystemHardwareLeader::radiansToRaw(double radians, const Motor & motor)
{
  const auto & calib = motor.calibration;
  double clamped = std::min(calib.urdf_upper, std::max(calib.urdf_lower, radians));
  return static_cast<int>((clamped - calib.rad_to_raw_offset) * calib.rad_to_raw_scale + calib.range_min);
}

// ----------------------------------------------------------------------------
// moveToParkPosition
// ----------------------------------------------------------------------------
void SOARM101SystemHardwareLeader::moveToParkPosition()
{
  if (park_positions_.size() != info_.joints.size()) {
    RCLCPP_WARN(rclcpp::get_logger("SOARM101SystemHardwareLeader"), 
                "Park positions size mismatch, skipping.");
    return;
  }

  RCLCPP_INFO(rclcpp::get_logger("SOARM101SystemHardwareLeader"), 
              "Moving to park position...");

  // Set commands
  for (size_t i = 0; i < info_.joints.size(); ++i) {
    motors_[i].command_position = park_positions_[i];
  }

  // Send commands with reduced speed
  const u16 speed = default_speed_ / 2;
  const u8 accel = default_accel_ / 2;

  std::array<u8, NUM_MOTORS> motor_ids;
  std::array<s16, NUM_MOTORS> positions;
  std::array<u16, NUM_MOTORS> speeds;
  std::array<u8, NUM_MOTORS> accelerations;
  size_t count = 0;

  for (size_t i = 0; i < info_.joints.size(); ++i) {
    double cmd = motors_[i].command_position;
    if (!std::isnan(cmd)) {
      const auto & motor = motors_[i];
      int raw = radiansToRaw(cmd, motor);
      motor_ids[count] = static_cast<u8>(motor.id);
      positions[count] = static_cast<s16>(raw);
      speeds[count] = speed;
      accelerations[count] = accel;
      ++count;
    }
  }

  if (count > 0) {
    servo_driver_.SyncWritePosEx(
        motor_ids.data(), count,
        positions.data(), speeds.data(), accelerations.data());
  }

  // Wait for movement completion (timeout 10 seconds)
  const int timeout_ms = 10000;
  const int sleep_ms = 50;
  int elapsed_ms = 0;
  bool all_stopped = false;

  while (elapsed_ms < timeout_ms) {
    bool moving = false;
    for (size_t i = 0; i < info_.joints.size(); ++i) {
      int motor_id = motors_[i].id;
      if (servo_driver_.FeedBack(motor_id) != 0) {
        int flag = servo_driver_.ReadMove(motor_id);
        if (flag != 0) {
          moving = true;
          break;
        }
      }
    }
    if (!moving) {
      all_stopped = true;
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(sleep_ms));
    elapsed_ms += sleep_ms;
  }

  if (all_stopped) {
    RCLCPP_INFO(rclcpp::get_logger("SOARM101SystemHardwareLeader"), "Park position reached.");
  } else {
    RCLCPP_WARN(rclcpp::get_logger("SOARM101SystemHardwareLeader"), 
                "Park position timeout after %d seconds.", timeout_ms/1000);
  }
}

// ----------------------------------------------------------------------------
// parseParkPositions
// ----------------------------------------------------------------------------
std::vector<double> SOARM101SystemHardwareLeader::parseParkPositions(const std::string & str)
{
  std::vector<double> result;
  std::string s = str;
  // Remove square brackets if present
  if (!s.empty() && s.front() == '[' && s.back() == ']') {
    s = s.substr(1, s.length() - 2);
  }
  std::stringstream ss(s);
  std::string token;
  while (std::getline(ss, token, ',')) {
    // Remove spaces
    token.erase(0, token.find_first_not_of(" \t"));
    token.erase(token.find_last_not_of(" \t") + 1);
    if (!token.empty()) {
      try {
        result.push_back(std::stod(token));
      } catch (...) {
        RCLCPP_WARN(rclcpp::get_logger("SOARM101SystemHardwareLeader"),
                    "Failed to parse park position token: '%s'", token.c_str());
      }
    }
  }
  return result;
}

// ----------------------------------------------------------------------------
// parseMaxTorques
// ----------------------------------------------------------------------------
std::vector<double> SOARM101SystemHardwareLeader::parseMaxTorques(const std::string & str)
{
  std::vector<double> result;
  std::string s = str;
  // Remove square brackets if present
  if (!s.empty() && s.front() == '[' && s.back() == ']') {
    s = s.substr(1, s.length() - 2);
  }
  std::stringstream ss(s);
  std::string token;
  while (std::getline(ss, token, ',')) {
    // Remove spaces
    token.erase(0, token.find_first_not_of(" \t"));
    token.erase(token.find_last_not_of(" \t") + 1);
    if (!token.empty()) {
      try {
        result.push_back(std::stod(token));
      } catch (...) {
        RCLCPP_WARN(rclcpp::get_logger("SOARM101SystemHardwareLeader"),
                    "Failed to parse max torque token: '%s'", token.c_str());
      }
    }
  }
  return result;
}

}  // namespace soarm101_hardware_leader

#include "pluginlib/class_list_macros.hpp"
PLUGINLIB_EXPORT_CLASS(soarm101_hardware_leader::SOARM101SystemHardwareLeader, hardware_interface::SystemInterface)