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


#include "soarm101_hardware/soarm101_hardware.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <limits>
#include <memory>
#include <sstream>
#include <thread>
#include <vector>

#include "hardware_interface/types/hardware_interface_type_values.hpp"
#include "rclcpp/rclcpp.hpp"

// For convenience
using hardware_interface::HW_IF_POSITION;
using hardware_interface::HW_IF_VELOCITY;
using hardware_interface::HW_IF_EFFORT;

namespace soarm101_hardware {

SOARM101SystemHardware::SOARM101SystemHardware()
: driver_initialized_(false)
{
}

SOARM101SystemHardware::~SOARM101SystemHardware()
{
  if (driver_initialized_) {
    servo_driver_.end();
  }
}

// ----------------------------------------------------------------------------
// on_init
// ----------------------------------------------------------------------------
hardware_interface::CallbackReturn
SOARM101SystemHardware::on_init(const hardware_interface::HardwareInfo & info)
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

  // --- Baud rate ---
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
    RCLCPP_INFO(rclcpp::get_logger("SOARM101SystemHardware"), "default_speed = %d", default_speed_);
  }

  // --- Default acceleration ---
  std::string accel_str = info_.hardware_parameters["default_accel"];
  if (!accel_str.empty()) {
    default_accel_ = static_cast<u8>(std::stoi(accel_str));
    RCLCPP_INFO(rclcpp::get_logger("SOARM101SystemHardware"), "default_accel = %d", default_accel_);
  }

  // --- Park position ---
  std::string park_str = info_.hardware_parameters["park_positions"];
  if (!park_str.empty()) {
    park_positions_ = parseParkPositions(park_str);
    if (park_positions_.size() == info_.joints.size()) {
      RCLCPP_INFO(rclcpp::get_logger("SOARM101SystemHardware"), "Park positions loaded.");
    } else {
      RCLCPP_WARN(rclcpp::get_logger("SOARM101SystemHardware"),
                  "park_positions size (%zu) != joints size (%zu), ignoring",
                  park_positions_.size(), info_.joints.size());
      park_positions_.clear();
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
    motor.id = motor_ids_[joint.name];
    motor.joint_name = joint.name;

    if (joint.parameters.find("initial_position") != joint.parameters.end()) {
      motor.sensors.position = std::stod(joint.parameters.at("initial_position"));
      motor.command_position = motor.sensors.position;
      RCLCPP_INFO(
        rclcpp::get_logger("SOARM101SystemHardware"),
        "Joint '%s' initial position set to: %.3f rad",
        joint.name.c_str(), motor.sensors.position);
    } else {
      motor.sensors.position = 0.0;
      motor.command_position = 0.0;
    }
  }

  RCLCPP_INFO(rclcpp::get_logger("SOARM101SystemHardware"), "on_init() finished successfully");
  return hardware_interface::CallbackReturn::SUCCESS;
}

// ----------------------------------------------------------------------------
// on_configure
// ----------------------------------------------------------------------------
hardware_interface::CallbackReturn
SOARM101SystemHardware::on_configure(const rclcpp_lifecycle::State & /*previous_state*/)
{
  RCLCPP_INFO(rclcpp::get_logger("SOARM101SystemHardware"), "Configuring...");

  // --- Load calibration ---
  if (!calibration_file_.empty() && !loadCalibration()) {
    RCLCPP_ERROR(
      rclcpp::get_logger("SOARM101SystemHardware"),
      "Failed to load calibration from: %s", calibration_file_.c_str());
    return hardware_interface::CallbackReturn::ERROR;
  }

  // --- Connect to bus ---
  if (!servo_driver_.begin(baudrate_, port_.c_str())) {
    RCLCPP_ERROR(
      rclcpp::get_logger("SOARM101SystemHardware"),
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

  RCLCPP_INFO(rclcpp::get_logger("SOARM101SystemHardware"), "Configuration completed");
  return hardware_interface::CallbackReturn::SUCCESS;
}

// ----------------------------------------------------------------------------
// export_state_interfaces
// ----------------------------------------------------------------------------
std::vector<hardware_interface::StateInterface>
SOARM101SystemHardware::export_state_interfaces()
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
  }
  return state_interfaces;
}

// ----------------------------------------------------------------------------
// export_command_interfaces
// ----------------------------------------------------------------------------
std::vector<hardware_interface::CommandInterface>
SOARM101SystemHardware::export_command_interfaces()
{
  std::vector<hardware_interface::CommandInterface> command_interfaces;
  for (size_t i = 0; i < info_.joints.size(); ++i) {
    command_interfaces.emplace_back(
      info_.joints[i].name, HW_IF_POSITION, &motors_[i].command_position);
  }
  return command_interfaces;
}

// ----------------------------------------------------------------------------
// on_activate
// ----------------------------------------------------------------------------
hardware_interface::CallbackReturn
SOARM101SystemHardware::on_activate(const rclcpp_lifecycle::State & /*previous_state*/)
{
  RCLCPP_INFO(rclcpp::get_logger("SOARM101SystemHardware"), "Activating...");

  for (const auto & pair : motor_ids_) {
    int motor_id = pair.second;
    if (servo_driver_.EnableTorque(motor_id, ENABLE_SERVO) != 1) {
      RCLCPP_ERROR(
        rclcpp::get_logger("SOARM101SystemHardware"),
        "Failed to enable torque for motor %d", motor_id);
      return hardware_interface::CallbackReturn::ERROR;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }

  RCLCPP_INFO(rclcpp::get_logger("SOARM101SystemHardware"), "Current motor positions:");
  for (size_t i = 0; i < info_.joints.size(); ++i) {
    readMotorData(i);
    motors_[i].command_position = motors_[i].sensors.position;
    RCLCPP_INFO(
      rclcpp::get_logger("SOARM101SystemHardware"),
      "  %s: %.3f rad",
      info_.joints[i].name.c_str(),
      motors_[i].sensors.position);
  }

  RCLCPP_INFO(rclcpp::get_logger("SOARM101SystemHardware"), "Activation completed");
  return hardware_interface::CallbackReturn::SUCCESS;
}

// ----------------------------------------------------------------------------
// on_deactivate
// ----------------------------------------------------------------------------
hardware_interface::CallbackReturn
SOARM101SystemHardware::on_deactivate(const rclcpp_lifecycle::State & /*previous_state*/)
{
  RCLCPP_INFO(rclcpp::get_logger("SOARM101SystemHardware"), "Deactivating...");

  // --- Park ---
  if (!park_positions_.empty()) {
    moveToParkPosition();
  } else {
    RCLCPP_INFO(rclcpp::get_logger("SOARM101SystemHardware"), "No park position set, skipping movement.");
  }

  // --- Disable torque ---
  for (const auto & pair : motor_ids_) {
    int motor_id = pair.second;
    if (servo_driver_.EnableTorque(motor_id, DISABLE_SERVO) == 0) {
      RCLCPP_ERROR(
        rclcpp::get_logger("SOARM101SystemHardware"),
        "Failed to disable torque for motor %d", motor_id);
      return hardware_interface::CallbackReturn::ERROR;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }

  RCLCPP_INFO(rclcpp::get_logger("SOARM101SystemHardware"), "Deactivation completed");
  return hardware_interface::CallbackReturn::SUCCESS;
}

// ----------------------------------------------------------------------------
// read
// ----------------------------------------------------------------------------
hardware_interface::return_type
SOARM101SystemHardware::read(const rclcpp::Time & /*time*/, const rclcpp::Duration & /*period*/)
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
SOARM101SystemHardware::write(const rclcpp::Time & /*time*/, const rclcpp::Duration & /*period*/)
{
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
      speeds[count] = default_speed_;
      accelerations[count] = default_accel_;
      ++count;
    }
  }

  if (count > 0) {
    servo_driver_.SyncWritePosEx(
        motor_ids.data(), count,
        positions.data(), speeds.data(), accelerations.data());
  }

  return hardware_interface::return_type::OK;
}

// ----------------------------------------------------------------------------
// loadCalibration
// ----------------------------------------------------------------------------
bool SOARM101SystemHardware::loadCalibration()
{
  if (calibration_file_.empty()) {
    RCLCPP_INFO(rclcpp::get_logger("SOARM101SystemHardware"), "No calibration file specified, using default values.");
    return true;
  }

  RCLCPP_INFO(rclcpp::get_logger("SOARM101SystemHardware"), "Loading calibration from: %s", calibration_file_.c_str());

  std::ifstream file(calibration_file_);
  if (!file.is_open()) {
    RCLCPP_ERROR(rclcpp::get_logger("SOARM101SystemHardware"), "Failed to open calibration file: %s", calibration_file_.c_str());
    return false;
  }

  std::string line;
  std::string current_motor_name;
  MotorCalibration current_calib = {0, 0, 0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
  bool in_block = false;

  while (std::getline(file, line)) {
    size_t start = line.find_first_not_of(" \t");
    if (start == std::string::npos) continue;
    std::string trimmed = line.substr(start);
    size_t end = trimmed.find_last_not_of(" \t");
    if (end != std::string::npos) trimmed = trimmed.substr(0, end + 1);

    if (trimmed.empty() || trimmed[0] == '#') continue;

    if (trimmed.back() == ':') {
      if (!current_motor_name.empty()) {
        auto it = motor_ids_.find(current_motor_name);
        if (it != motor_ids_.end()) {
          int motor_id = it->second;
          for (auto & motor : motors_) {
            if (motor.id == motor_id) {
              motor.calibration = current_calib;
              updateCalibrationCoefficients(motor);
              RCLCPP_INFO(rclcpp::get_logger("SOARM101SystemHardware"),
                  "✅ Applied calibration for motor '%s' (ID %d): range_min=%d, range_max=%d",
                  current_motor_name.c_str(), motor_id, current_calib.range_min, current_calib.range_max);
              break;
            }
          }
        } else {
          RCLCPP_WARN(rclcpp::get_logger("SOARM101SystemHardware"),
              "⚠️ Unknown motor name '%s' in calibration file", current_motor_name.c_str());
        }
      }

      current_motor_name = trimmed.substr(0, trimmed.length() - 1);
      size_t ns = current_motor_name.find_first_not_of(" \t");
      size_t ne = current_motor_name.find_last_not_of(" \t");
      if (ns != std::string::npos && ne != std::string::npos) {
        current_motor_name = current_motor_name.substr(ns, ne - ns + 1);
      }
      current_calib = {0, 0, 0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
      in_block = true;
      continue;
    }

    if (in_block && !current_motor_name.empty()) {
      size_t colon_pos = trimmed.find(':');
      if (colon_pos != std::string::npos) {
        std::string key = trimmed.substr(0, colon_pos);
        std::string value = trimmed.substr(colon_pos + 1);
        key.erase(0, key.find_first_not_of(" \t"));
        key.erase(key.find_last_not_of(" \t") + 1);
        value.erase(0, value.find_first_not_of(" \t"));
        value.erase(value.find_last_not_of(" \t") + 1);

        if (key == "drive_mode") {
          current_calib.drive_mode = std::stoi(value);
        } else if (key == "range_min") {
          current_calib.range_min = std::stoi(value);
        } else if (key == "range_max") {
          current_calib.range_max = std::stoi(value);
        }
      }
    }
  }

  // Save the last motor
  if (!current_motor_name.empty()) {
    auto it = motor_ids_.find(current_motor_name);
    if (it != motor_ids_.end()) {
      int motor_id = it->second;
      for (auto & motor : motors_) {
        if (motor.id == motor_id) {
          motor.calibration = current_calib;
          updateCalibrationCoefficients(motor);
          RCLCPP_INFO(rclcpp::get_logger("SOARM101SystemHardware"),
              "✅ Applied calibration for motor '%s' (ID %d): range_min=%d, range_max=%d",
              current_motor_name.c_str(), motor_id, current_calib.range_min, current_calib.range_max);
          break;
        }
      }
    } else {
      RCLCPP_WARN(rclcpp::get_logger("SOARM101SystemHardware"),
          "⚠️ Unknown motor name '%s' in calibration file", current_motor_name.c_str());
    }
  }

  file.close();
  RCLCPP_INFO(rclcpp::get_logger("SOARM101SystemHardware"), "Calibration loading finished.");
  return true;
}

// ----------------------------------------------------------------------------
// updateCalibrationCoefficients
// ----------------------------------------------------------------------------
void SOARM101SystemHardware::updateCalibrationCoefficients(Motor & motor)
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
    RCLCPP_WARN(rclcpp::get_logger("SOARM101SystemHardware"),
                "Zero raw range for motor %d, using scale=1", motor.id);
  }
  calib.raw_to_rad_offset = calib.range_min;
  calib.rad_to_raw_offset = urdf_lower;
}

// ----------------------------------------------------------------------------
// readMotorData
// ----------------------------------------------------------------------------
void SOARM101SystemHardware::readMotorData(size_t index)
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
    motor.sensors.effort   = static_cast<double>(raw_effort);
    motor.sensors.temperature = static_cast<double>(raw_temperature);
    motor.sensors.voltage  = static_cast<double>(raw_voltage) / 10.0;
    motor.sensors.current  = static_cast<double>(raw_current) / 1000.0;
    motor.sensors.moving_flag = static_cast<double>(raw_moving_flag);
  }
}

// ----------------------------------------------------------------------------
// rawToRadians
// ----------------------------------------------------------------------------
double SOARM101SystemHardware::rawToRadians(int raw_position, const Motor & motor)
{
  const auto & calib = motor.calibration;
  int clamped = std::max(calib.range_min, std::min(calib.range_max, raw_position));
  return (static_cast<double>(clamped) - calib.raw_to_rad_offset) * calib.raw_to_rad_scale + calib.urdf_lower;
}

// ----------------------------------------------------------------------------
// radiansToRaw
// ----------------------------------------------------------------------------
int SOARM101SystemHardware::radiansToRaw(double radians, const Motor & motor)
{
  const auto & calib = motor.calibration;
  double clamped = std::min(calib.urdf_upper, std::max(calib.urdf_lower, radians));
  return static_cast<int>((clamped - calib.rad_to_raw_offset) * calib.rad_to_raw_scale + calib.range_min);
}

// ----------------------------------------------------------------------------
// moveToParkPosition
// ----------------------------------------------------------------------------
void SOARM101SystemHardware::moveToParkPosition()
{
  if (park_positions_.size() != info_.joints.size()) {
    RCLCPP_WARN(rclcpp::get_logger("SOARM101SystemHardware"), 
                "Park positions size mismatch, skipping.");
    return;
  }

  RCLCPP_INFO(rclcpp::get_logger("SOARM101SystemHardware"), 
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
    RCLCPP_INFO(rclcpp::get_logger("SOARM101SystemHardware"), "Park position reached.");
  } else {
    RCLCPP_WARN(rclcpp::get_logger("SOARM101SystemHardware"), 
                "Park position timeout after %d seconds.", timeout_ms/1000);
  }
}

// ----------------------------------------------------------------------------
// parseParkPositions
// ----------------------------------------------------------------------------
std::vector<double> SOARM101SystemHardware::parseParkPositions(const std::string & str)
{
  std::vector<double> result;
  std::string s = str;
  // Remove square brackets if present
  if (s.front() == '[' && s.back() == ']') {
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
        RCLCPP_WARN(rclcpp::get_logger("SOARM101SystemHardware"),
                    "Failed to parse park position token: '%s'", token.c_str());
      }
    }
  }
  return result;
}

}  // namespace soarm101_hardware

#include "pluginlib/class_list_macros.hpp"
PLUGINLIB_EXPORT_CLASS(soarm101_hardware::SOARM101SystemHardware, hardware_interface::SystemInterface)