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


#ifndef SOARM101_HARDWARE__SOARM101_SYSTEM_HPP_
#define SOARM101_HARDWARE__SOARM101_SYSTEM_HPP_

#include <array>
#include <memory>
#include <string>
#include <vector>
#include <map>

#include "hardware_interface/handle.hpp"
#include "hardware_interface/hardware_info.hpp"
#include "hardware_interface/system_interface.hpp"
#include "hardware_interface/types/hardware_interface_return_values.hpp"
#include "rclcpp/macros.hpp"
#include "rclcpp_lifecycle/node_interfaces/lifecycle_node_interface.hpp"
#include "rclcpp_lifecycle/state.hpp"

#include "SCServo.h"

//! Macro to enable servo (Enable Torque)
#define ENABLE_TORQUE  1
//! Macro to disable servo (Disable Torque)
#define DISABLE_TORQUE 0
//! Fixed number of motors in the robot
#define NUM_MOTORS 6

namespace soarm101_hardware_leader
{

/**
 * @brief Main hardware interface class for the SO-ARM101 robot.
 * 
 * Implements hardware_interface::SystemInterface for controlling 6 Feetech STS
 * servos via a serial port. Provides state interfaces (position, velocity,
 * effort, temperature, voltage, current, moving flag) and command interfaces
 * (position).
 * 
 * Key features:
 * - Loads calibration from YAML file.
 * - Park position on deactivation.
 * - Configurable default speed and acceleration.
 * - Optimised raw ↔ radian conversions.
 */
class SOARM101SystemHardwareLeader : public hardware_interface::SystemInterface
{
public:
  RCLCPP_SHARED_PTR_DEFINITIONS(SOARM101SystemHardwareLeader)

  /**
   * @brief Constructor. Initialises flags.
   */
  SOARM101SystemHardwareLeader();

  /**
   * @brief Destructor. Disconnects the driver if initialised.
   */
  ~SOARM101SystemHardwareLeader();

  // ==================== Overridden methods from SystemInterface ====================

  /**
   * @brief Initialisation of the component.
   * @param info HardwareInfo structure with parameters from ROS 2.
   * @return SUCCESS on success, otherwise ERROR.
   * 
   * Called once when the plugin is loaded. Reads port, baud rate, calibration file,
   * as well as user parameters default_speed, default_accel, park_positions.
   * Creates motor structures.
   */
  hardware_interface::CallbackReturn on_init(
    const hardware_interface::HardwareInfo & info) override;

  /**
   * @brief Configuration of the component.
   * @param previous_state State before configuration (not used).
   * @return SUCCESS on success, otherwise ERROR.
   * 
   * Loads calibration, connects to the serial port, initialises commands with
   * current positions.
   */
  hardware_interface::CallbackReturn on_configure(
    const rclcpp_lifecycle::State & previous_state) override;

  /**
   * @brief Export state interfaces (reading data from motors).
   * @return Vector of StateInterface pointers to sensor fields of each motor.
   * 
   * Exports: position, velocity, effort, temperature, voltage, current, moving_flag.
   */
  std::vector<hardware_interface::StateInterface> export_state_interfaces() override;

  /**
   * @brief Export command interfaces (writing data to motors).
   * @return Vector of CommandInterface pointers to command_position of each motor.
   * 
   * Exports only position (position control mode).
   */
  std::vector<hardware_interface::CommandInterface> export_command_interfaces() override;

  /**
   * @brief Activation of the component.
   * @param previous_state State before activation (not used).
   * @return SUCCESS on success, otherwise ERROR.
   * 
   * Enables torque on all motors, reads current positions and sets commands
   * equal to current positions to prevent jerks.
   */
  hardware_interface::CallbackReturn on_activate(
    const rclcpp_lifecycle::State & previous_state) override;

  /**
   * @brief Deactivation of the component.
   * @param previous_state State before deactivation (not used).
   * @return SUCCESS on success, otherwise ERROR.
   * 
   * If a park position is set, moves the robot to it (with reduced speed),
   * then disables torque on all motors.
   */
  hardware_interface::CallbackReturn on_deactivate(
    const rclcpp_lifecycle::State & previous_state) override;

  /**
   * @brief Read data from motors.
   * @param time Current time (not used).
   * @param period Period (not used).
   * @return return_type::OK always.
   * 
   * Called cyclically (usually 100 Hz). Updates sensors.position etc.
   */
  hardware_interface::return_type read(
    const rclcpp::Time & time, const rclcpp::Duration & period) override;

  /**
   * @brief Write commands to motors.
   * @param time Current time (not used).
   * @param period Period (not used).
   * @return return_type::OK always.
   * 
   * Called cyclically. Sends position commands synchronously using SyncWritePosEx.
   * Speed and acceleration are taken from default_speed_ and default_accel_.
   */
  hardware_interface::return_type write(
    const rclcpp::Time & time, const rclcpp::Duration & period) override;

private:
  // ==================== Nested structures ====================

  /**
   * @brief Sensor readings of a single motor.
   */
  struct MotorSensor
  {
    double position = 0.0;      ///< Current position, radians
    double velocity = 0.0;      ///< Current velocity, rad/s
    double effort   = 0.0;      ///< Current effort (load), N*m
    double temperature = 0.0;   ///< Motor temperature, °C
    double voltage  = 0.0;      ///< Supply voltage, V
    double current  = 0.0;      ///< Current, A
    double moving_flag = 0.0;   ///< Motion flag (0 – stopped, 1 – moving)
    double enable_torque = 0.0;

  };

  /**
   * @brief Motor calibration parameters.
   * 
   * Contains raw limits (range_min, range_max) and precomputed coefficients
   * for fast raw ↔ radian conversion.
   */
  struct MotorCalibration
  {
    int drive_mode;                 ///< Motor drive mode (not used)
    int range_min;                  ///< Minimum raw encoder value
    int range_max;                  ///< Maximum raw encoder value
    double raw_to_rad_scale;        ///< Scale for raw->radians conversion
    double raw_to_rad_offset;       ///< Offset for raw->radians conversion
    double rad_to_raw_scale;        ///< Scale for radians->raw conversion
    double rad_to_raw_offset;       ///< Offset for radians->raw conversion
    double urdf_lower;              ///< Lower limit from URDF, radians
    double urdf_upper;              ///< Upper limit from URDF, radians
  };

  /**
   * @brief Aggregated structure for one motor.
   * 
   * Contains all information: ID, name, target command, max torque, flag 'enable torque',
   * sensor readings, and calibration.
   */
  struct Motor
  {
    int id;                         ///< Motor ID (1..6)
    std::string joint_name;         ///< Joint name from URDF
    double max_torque = 0.0;        ///< Max torque, N*m
    double command_position = 0.0;  ///< Target position, radians
    MotorSensor sensors;            ///< Sensor readings
    MotorCalibration calibration;   ///< Calibration parameters
  };

  // ==================== Class members ====================

  //! Parameters from configuration
  std::string port_;                ///< Serial port (e.g., /dev/ttyACM0)
  int         baudrate_;            ///< Baud rate
  std::string calibration_file_;    ///< Path to YAML calibration file

  //! Parameters loaded from info_.hardware_parameters
  u16 default_speed_ = 2400;        ///< Default speed for writing
  u8  default_accel_ = 50;          ///< Default acceleration for writing
  std::vector<double> park_positions_; ///< Park position (radians) in info_.joints order
  std::vector<double> max_torques_; ///< Max torques (N * m)

  //! Motor data
  std::vector<Motor> motors_;       ///< Vector of Motor structures, size = info_.joints.size()
  std::map<std::string, int> motor_ids_; ///< Mapping joint name -> motor ID

  //! SCServo SDK driver
  SMS_STS servo_driver_;
  bool driver_initialized_;         ///< Flag indicating successful driver connection

  // ==================== Private methods ====================

  /**
   * @brief Load calibration from YAML file.
   * @return true on success, false on error.
   * 
   * Parses the file; joint names must match keys in motor_ids_.
   * For each motor calls updateCalibrationCoefficients().
   */
  bool loadCalibration();

  /**
   * @brief Precompute coefficients for fast conversion.
   * @param motor Reference to Motor structure whose calibration will be updated.
   * 
   * Determines URDF limits based on motor.id, then computes raw_to_rad_scale etc.
   */
  void updateCalibrationCoefficients(Motor & motor);

  /**
   * @brief Read data from a single motor.
   * @param index Index in info_.joints and motors_ vectors.
   * 
   * Calls FeedBack, then ReadPos, ReadSpeed, etc., fills sensors.
   */
  void readMotorData(size_t index);

  /**
   * @brief Convert raw encoder value to radians.
   * @param raw_position Raw value (0-4095).
   * @param motor Reference to Motor containing calibration.
   * @return Angle in radians.
   * 
   * Uses precomputed coefficients from motor.calibration.
   */
  double rawToRadians(int raw_position, const Motor & motor);

  /**
   * @brief Convert radians to raw encoder value.
   * @param radians Angle in radians.
   * @param motor Reference to Motor containing calibration.
   * @return Raw value (0-4095), clipped to URDF limits.
   */
  int radiansToRaw(double radians, const Motor & motor);

  /**
   * @brief Move the robot to the park position.
   * 
   * Sets command_position from park_positions_, sends commands with half
   * speed/acceleration, then waits for movement completion (timeout 10 s).
   */
  void moveToParkPosition();

  /**
   * @brief Parse a string containing an array of numbers for park_positions.
   * @param str String like "[0.004, -1.712, ...]" or "0.004, -1.712, ..."
   * @return Vector of double numbers.
   */
  std::vector<double> parseParkPositions(const std::string & str);

  /**
   * @brief Parse a string containing an array of numbers for max_torques.
   * @param str String like "[0.004, 1.712, ...]" or "0.004, 1.712, ..."
   * @return Vector of double numbers.
   */
  std::vector<double> parseMaxTorques(const std::string & str);
};

}  // namespace soarm101_hardware_leader

#endif  // SOARM101_HARDWARE_LEADER_SOARM101_SYSTEM_HPP_