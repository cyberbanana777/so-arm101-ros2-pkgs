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

#ifndef SOARM101_HARDWARE__SOARM101_SYSTEM_HPP_
#define SOARM101_HARDWARE__SOARM101_SYSTEM_HPP_

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

#define ENABLE_SERVO  1
#define DISABLE_SERVO 0

#define FAIL_CODE 0
#define SUCSESS_CODE 1

namespace soarm101_hardware
{

class SOARM101SystemHardware : public hardware_interface::SystemInterface
{
public:
  RCLCPP_SHARED_PTR_DEFINITIONS(SOARM101SystemHardware)

  SOARM101SystemHardware();
  ~SOARM101SystemHardware();

  hardware_interface::CallbackReturn on_init(
    const hardware_interface::HardwareInfo & info) override;

  hardware_interface::CallbackReturn on_configure(
    const rclcpp_lifecycle::State & previous_state) override;

  std::vector<hardware_interface::StateInterface> export_state_interfaces() override;

  std::vector<hardware_interface::CommandInterface> export_command_interfaces() override;

  hardware_interface::CallbackReturn on_activate(
    const rclcpp_lifecycle::State & previous_state) override;

  hardware_interface::CallbackReturn on_deactivate(
    const rclcpp_lifecycle::State & previous_state) override;

  hardware_interface::return_type read(
    const rclcpp::Time & time, const rclcpp::Duration & period) override;

  hardware_interface::return_type write(
    const rclcpp::Time & time, const rclcpp::Duration & period) override;

private:
  // ------------------- Структуры данных -------------------
  struct MotorSensor
  {
    double position = 0.0;
    double velocity = 0.0;
    double effort   = 0.0;
    double temperature = 0.0;
    double voltage  = 0.0;
    double current  = 0.0;
    double moving_flag = 0.0;   // интерфейс требует double
  };

  // Калибровка мотора (без id – берём из Motor)
  struct MotorCalibration
  {
    int drive_mode;
    int range_min;    // минимальный сырой отсчёт энкодера
    int range_max;    // максимальный сырой отсчёт
  };

  // Агрегатор всех данных по одному мотору
  struct Motor
  {
    int id;                     // ID мотора (1..6)
    std::string joint_name;     // имя сустава из URDF
    double command_position;    // целевая позиция в радианах
    MotorSensor sensors;        // все показания с датчиков
    MotorCalibration calibration; // калибровочные параметры
  };

  // ------------------- Параметры из конфига -------------------
  std::string port_;
  int         baudrate_;
  std::string calibration_file_;
  // Парковочная позиция (радианы) в порядке info_.joints
  std::vector<double> park_positions_;

  // ------------------- Данные по моторам -------------------
  std::vector<Motor> motors_;          // индекс соответствует порядку в info_.joints
  std::map<std::string, int> motor_ids_;  // имя сустава -> ID мотора (только для загрузки калибровки)

  // ------------------- Драйвер -------------------
  SMS_STS servo_driver_;
  bool driver_initialized_;

  // ------------------- Вспомогательные методы -------------------
  bool loadCalibration();
  double rawToRadians(int raw_position, const Motor & motor);
  int radiansToRaw(double radians, const Motor & motor);
  void moveToParkPosition();
};

}  // namespace soarm101_hardware

#endif  // SOARM101_HARDWARE__SOARM101_SYSTEM_HPP_