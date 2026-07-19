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

#include "soarm101_hardware/soarm101_hardware.hpp"

#include <cmath>
#include <fstream>
#include <limits>
#include <memory>
#include <sstream>
#include <vector>

#include "hardware_interface/types/hardware_interface_type_values.hpp"
#include "rclcpp/rclcpp.hpp"

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

  // --- Порт ---
  port_ = info_.hardware_parameters["port"];
  if (port_.empty()) {
    port_ = "/dev/ttyACM0";
  }

  // --- Скорость ---
  std::string baudrate_str = info_.hardware_parameters["baudrate"];
  if (baudrate_str.empty()) {
    baudrate_ = 1000000;
  } else {
    baudrate_ = std::stoi(baudrate_str);
  }

  // --- Файл калибровки ---
  calibration_file_ = info_.hardware_parameters["calibration_file"];

  // --- Маппинг имён суставов на ID моторов ---
  motor_ids_["shoulder_pan_joint"]   = 1;
  motor_ids_["shoulder_lift_joint"]  = 2;
  motor_ids_["elbow_flex_joint"]     = 3;
  motor_ids_["wrist_flex_joint"]     = 4;
  motor_ids_["wrist_roll_joint"]     = 5;
  motor_ids_["gripper_jaw_joint"]    = 6;

  // --- Выделяем память под моторы ---
  motors_.resize(info_.joints.size());

  // --- Заполняем структуры начальными данными ---
  for (size_t i = 0; i < info_.joints.size(); ++i) {
    const auto & joint = info_.joints[i];
    auto & motor = motors_[i];

    motor.id = motor_ids_[joint.name];          // устанавливаем ID
    motor.joint_name = joint.name;

    // Если в URDF задана initial_position – используем её
    if (joint.parameters.find("initial_position") != joint.parameters.end()) {
      motor.sensors.position = std::stod(joint.parameters.at("initial_position"));
      motor.command_position = motor.sensors.position;
      RCLCPP_INFO(
        rclcpp::get_logger("SOARM101SystemHardware"),
        "Joint '%s' initial position set to: %.3f rad",
        joint.name.c_str(), motor.sensors.position);
    } else {
      // Иначе оставляем 0 (позже прочитаем с сервоприводов)
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

  // --- Загружаем калибровку ---
  if (!calibration_file_.empty() && !loadCalibration()) {
    RCLCPP_ERROR(
      rclcpp::get_logger("SOARM101SystemHardware"),
      "Failed to load calibration from: %s", calibration_file_.c_str());
    return hardware_interface::CallbackReturn::ERROR;
  }

  // --- Подключаемся к шине ---
  if (!servo_driver_.begin(baudrate_, port_.c_str())) {
    RCLCPP_ERROR(
      rclcpp::get_logger("SOARM101SystemHardware"),
      "Failed to connect to motor bus on port %s. Check connection and permissions.",
      port_.c_str());
    RCLCPP_ERROR(rclcpp::get_logger("SOARM101SystemHardware"), "TROUBLESHOOTING:");
    RCLCPP_ERROR(
      rclcpp::get_logger("SOARM101SystemHardware"),
      "1. Check if robot is connected: ls /dev/ttyACM* /dev/ttyUSB*");
    RCLCPP_ERROR(
      rclcpp::get_logger("SOARM101SystemHardware"),
      "2. If robot is on different port, launch with: port:=/dev/ttyACMX");
    RCLCPP_ERROR(
      rclcpp::get_logger("SOARM101SystemHardware"),
      "3. Check permissions: sudo usermod -aG dialout $USER (then logout/login)");
    return hardware_interface::CallbackReturn::ERROR;
  }
  driver_initialized_ = true;

  // --- Инициализируем команды текущими позициями (если не заданы) ---
  for (size_t i = 0; i < motors_.size(); ++i) {
    auto & motor = motors_[i];
    if (std::isnan(motor.sensors.position)) {
      motor.sensors.position = 0.0;
    }
    motor.command_position = motor.sensors.position;
  }

  RCLCPP_INFO(rclcpp::get_logger("SOARM101SystemHardware"), "Configuration completed");

  // Установка парковочной позиции (в порядке info_.joints)
  // Можно загружать из параметров, но пока захардкодим.
  park_positions_.resize(info_.joints.size());
  if (info_.joints.size() == 6) {
      park_positions_[0] = 0.004;   // shoulder_pan
      park_positions_[1] = -1.712;  // shoulder_lift
      park_positions_[2] = 1.560;   // elbow_flex
      park_positions_[3] = 0.748;   // wrist_flex
      park_positions_[4] = -0.029;  // wrist_roll
      park_positions_[5] = 0.461;   // gripper_jaw
      RCLCPP_INFO(rclcpp::get_logger("SOARM101SystemHardware"), "Park position set.");
  } else {
      RCLCPP_WARN(rclcpp::get_logger("SOARM101SystemHardware"), 
                  "Unexpected number of joints (%zu), park positions not set.", info_.joints.size());
  }
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

    state_interfaces.emplace_back(joint.name, hardware_interface::HW_IF_POSITION, &motor.sensors.position);
    state_interfaces.emplace_back(joint.name, hardware_interface::HW_IF_VELOCITY, &motor.sensors.velocity);
    state_interfaces.emplace_back(joint.name, hardware_interface::HW_IF_EFFORT,   &motor.sensors.effort);
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
      info_.joints[i].name, hardware_interface::HW_IF_POSITION, &motors_[i].command_position);
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

  // --- Включаем момент на всех моторах ---
  for (const auto & pair : motor_ids_) {
    int motor_id = pair.second;
    if (servo_driver_.EnableTorque(motor_id, ENABLE_SERVO) == FAIL_CODE) {
      RCLCPP_ERROR(
        rclcpp::get_logger("SOARM101SystemHardware"),
        "Failed to enable torque for motor %d", motor_id);
      return hardware_interface::CallbackReturn::ERROR;
    }
    rclcpp::sleep_for(std::chrono::milliseconds(50));
  }

  // --- Читаем текущие позиции и другие данные ---
  RCLCPP_INFO(rclcpp::get_logger("SOARM101SystemHardware"), "Current motor positions:");
  for (size_t i = 0; i < info_.joints.size(); ++i) {
    const std::string & joint_name = info_.joints[i].name;
    int motor_id = motor_ids_[joint_name];

    if (servo_driver_.FeedBack(motor_id) == SUCSESS_CODE) {
      int raw_pos         = servo_driver_.ReadPos(motor_id);
      int raw_velocity    = servo_driver_.ReadSpeed(motor_id);
      int raw_effort      = servo_driver_.ReadLoad(motor_id);
      int raw_temperature = servo_driver_.ReadTemper(motor_id);
      int raw_voltage     = servo_driver_.ReadVoltage(motor_id);
      int raw_current     = servo_driver_.ReadCurrent(motor_id);
      int raw_moving_flag = servo_driver_.ReadMove(motor_id);

      auto & motor = motors_[i];
      motor.sensors.position = rawToRadians(raw_pos, motor);
      motor.sensors.velocity = static_cast<double>(raw_velocity) / 4096.0 * 2.0 * M_PI;
      motor.sensors.effort   = static_cast<double>(raw_effort);
      motor.sensors.temperature = static_cast<double>(raw_temperature);
      motor.sensors.voltage  = static_cast<double>(raw_voltage) / 10.0;
      motor.sensors.current  = static_cast<double>(raw_current) / 1000.0;
      motor.sensors.moving_flag = static_cast<double>(raw_moving_flag);

      motor.command_position = motor.sensors.position;

      RCLCPP_INFO(
        rclcpp::get_logger("SOARM101SystemHardware"),
        "  %s: %.3f rad (raw: %d)", joint_name.c_str(), motor.sensors.position, raw_pos);
    }
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

    // 1. Переместиться в парковочную позицию
    moveToParkPosition();

    // 2. Отключить момент
    for (const auto & pair : motor_ids_) {
        int motor_id = pair.second;
        if (servo_driver_.EnableTorque(motor_id, DISABLE_SERVO) == 0) {
            RCLCPP_ERROR(
                rclcpp::get_logger("SOARM101SystemHardware"),
                "Failed to disable torque for motor %d", motor_id);
            return hardware_interface::CallbackReturn::ERROR;
        }
        rclcpp::sleep_for(std::chrono::milliseconds(50));
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
  // Читаем данные со всех моторов
  for (size_t i = 0; i < info_.joints.size(); ++i) {
    const std::string & joint_name = info_.joints[i].name;
    int motor_id = motor_ids_[joint_name];

    if (servo_driver_.FeedBack(motor_id) == SUCSESS_CODE) {
      int raw_pos         = servo_driver_.ReadPos(motor_id);
      int raw_velocity    = servo_driver_.ReadSpeed(motor_id);
      int raw_effort      = servo_driver_.ReadLoad(motor_id);
      int raw_temperature = servo_driver_.ReadTemper(motor_id);
      int raw_voltage     = servo_driver_.ReadVoltage(motor_id);
      int raw_current     = servo_driver_.ReadCurrent(motor_id);
      int raw_moving_flag = servo_driver_.ReadMove(motor_id);

      auto & motor = motors_[i];
      motor.sensors.position = rawToRadians(raw_pos, motor);
      motor.sensors.velocity = static_cast<double>(raw_velocity) / 4096.0 * 2.0 * M_PI;
      motor.sensors.effort   = static_cast<double>(raw_effort);
      motor.sensors.temperature = static_cast<double>(raw_temperature);
      motor.sensors.voltage  = static_cast<double>(raw_voltage) / 10.0;
      motor.sensors.current  = static_cast<double>(raw_current) / 1000.0;
      motor.sensors.moving_flag = static_cast<double>(raw_moving_flag);
    }
  }

  return hardware_interface::return_type::OK;
}

// ----------------------------------------------------------------------------
// write
// ----------------------------------------------------------------------------
hardware_interface::return_type
SOARM101SystemHardware::write(const rclcpp::Time & /*time*/, const rclcpp::Duration & /*period*/)
{
  // Используем фиксированную скорость (без начального движения)
  const u16 speed = 2400;

  std::vector<u8> motor_ids;
  std::vector<s16> positions;
  std::vector<u16> speeds;
  std::vector<u8> accelerations;

  for (size_t i = 0; i < info_.joints.size(); ++i) {
    double cmd = motors_[i].command_position;
    if (!std::isnan(cmd)) {
      const auto & motor = motors_[i];
      int raw = radiansToRaw(cmd, motor);

      motor_ids.push_back(static_cast<u8>(motor.id));
      positions.push_back(static_cast<s16>(raw));
      speeds.push_back(speed);
      accelerations.push_back(static_cast<u8>(50));
    }
  }

  if (!motor_ids.empty()) {
    servo_driver_.SyncWritePosEx(
      motor_ids.data(), motor_ids.size(),
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
  MotorCalibration current_calib = {0, 0, 0};
  bool in_block = false;

  while (std::getline(file, line)) {
    // Удаляем пробелы в начале и конце строки
    size_t start = line.find_first_not_of(" \t");
    if (start == std::string::npos) continue; // пустая строка
    std::string trimmed = line.substr(start);
    size_t end = trimmed.find_last_not_of(" \t");
    if (end != std::string::npos) trimmed = trimmed.substr(0, end + 1);

    // Пропускаем комментарии
    if (trimmed.empty() || trimmed[0] == '#') continue;

    // Проверяем, является ли строка именем мотора (заканчивается на ':')
    if (trimmed.back() == ':') {
      // Сохраняем предыдущий мотор, если он был
      if (!current_motor_name.empty()) {
        auto it = motor_ids_.find(current_motor_name);
        if (it != motor_ids_.end()) {
          int motor_id = it->second;
          for (auto & motor : motors_) {
            if (motor.id == motor_id) {
              motor.calibration = current_calib;
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

      // Начинаем новый мотор
      current_motor_name = trimmed.substr(0, trimmed.length() - 1);
      // Удаляем пробелы вокруг имени (если они были)
      size_t name_start = current_motor_name.find_first_not_of(" \t");
      size_t name_end = current_motor_name.find_last_not_of(" \t");
      if (name_start != std::string::npos && name_end != std::string::npos) {
        current_motor_name = current_motor_name.substr(name_start, name_end - name_start + 1);
      }
      current_calib = {0, 0, 0};
      in_block = true;
      continue;
    }

    // Если мы внутри блока, парсим параметры
    if (in_block && !current_motor_name.empty()) {
      size_t colon_pos = trimmed.find(':');
      if (colon_pos != std::string::npos) {
        std::string key = trimmed.substr(0, colon_pos);
        std::string value = trimmed.substr(colon_pos + 1);
        // Убираем пробелы
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

  // Сохраняем последний мотор
  if (!current_motor_name.empty()) {
    auto it = motor_ids_.find(current_motor_name);
    if (it != motor_ids_.end()) {
      int motor_id = it->second;
      for (auto & motor : motors_) {
        if (motor.id == motor_id) {
          motor.calibration = current_calib;
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
// rawToRadians
// ----------------------------------------------------------------------------
double SOARM101SystemHardware::rawToRadians(
  int raw_position, const Motor & motor)
{
  const auto & calib = motor.calibration;
  // Ограничиваем сырым диапазоном
  int clamped = std::max(calib.range_min, std::min(calib.range_max, raw_position));

  // Нормализуем в [0,1]
  double progress = static_cast<double>(clamped - calib.range_min) /
                    static_cast<double>(calib.range_max - calib.range_min);

  // URDF-ограничения (жёстко зашиты, в будущем можно загружать из параметров)
  double urdf_lower, urdf_upper;
  switch (motor.id) {
    case 1:  // shoulder_pan
      urdf_lower = -1.91986;
      urdf_upper =  1.91986;
      break;
    case 2:  // shoulder_lift
      urdf_lower = -1.74533;
      urdf_upper =  1.74533;
      break;
    case 3:  // elbow_flex
      urdf_lower = -1.74533;
      urdf_upper =  1.5708;
      break;
    case 4:  // wrist_flex
      urdf_lower = -1.65806;
      urdf_upper =  1.65806;
      break;
    case 5:  // wrist_roll
      urdf_lower = -2.79253;
      urdf_upper =  2.79253;
      break;
    case 6:  // gripper
      urdf_lower = -0.1745;
      urdf_upper =  1.4483;
      break;
    default:
      urdf_lower = -M_PI;
      urdf_upper =  M_PI;
      break;
  }

  return progress * (urdf_upper - urdf_lower) + urdf_lower;
}

// ----------------------------------------------------------------------------
// radiansToRaw
// ----------------------------------------------------------------------------
int SOARM101SystemHardware::radiansToRaw(
  double radians, const Motor & motor)
{
  const auto & calib = motor.calibration;

  // URDF-ограничения
  double urdf_lower, urdf_upper;
  switch (motor.id) {
    case 1:
      urdf_lower = -1.91986;
      urdf_upper =  1.91986;
      break;
    case 2:
      urdf_lower = -1.74533;
      urdf_upper =  1.74533;
      break;
    case 3:
      urdf_lower = -1.74533;
      urdf_upper =  1.5708;
      break;
    case 4:
      urdf_lower = -1.65806;
      urdf_upper =  1.65806;
      break;
    case 5:
      urdf_lower = -2.79253;
      urdf_upper =  2.79253;
      break;
    case 6:
      urdf_lower = -0.1745;
      urdf_upper =  1.4483;
      break;
    default:
      urdf_lower = -M_PI;
      urdf_upper =  M_PI;
      break;
  }

  // Клэмпим в URDF-диапазон
  double clamped = std::min(urdf_upper, std::max(urdf_lower, radians));

  // Нормализуем в [0,1]
  double progress = (clamped - urdf_lower) / (urdf_upper - urdf_lower);

  // Масштабируем на сырой диапазон
  int raw = static_cast<int>(progress * (calib.range_max - calib.range_min) + calib.range_min);

  return raw;
}


void SOARM101SystemHardware::moveToParkPosition()
{
    if (park_positions_.size() != info_.joints.size()) {
        RCLCPP_WARN(rclcpp::get_logger("SOARM101SystemHardware"), 
                    "Park positions not set or size mismatch, skipping.");
        return;
    }

    RCLCPP_INFO(rclcpp::get_logger("SOARM101SystemHardware"), 
                "Moving to park position...");

    // Устанавливаем команды в парковочную позицию
    for (size_t i = 0; i < info_.joints.size(); ++i) {
        motors_[i].command_position = park_positions_[i];
    }

    // Отправляем команды (как в write, но с пониженной скоростью)
    const u16 speed = 1200;  // медленнее, чем обычно (2400)
    const u8 accel = 30;     // меньше ускорение

    std::vector<u8> motor_ids;
    std::vector<s16> positions;
    std::vector<u16> speeds;
    std::vector<u8> accelerations;

    for (size_t i = 0; i < info_.joints.size(); ++i) {
        double cmd = motors_[i].command_position;
        if (!std::isnan(cmd)) {
            const auto & motor = motors_[i];
            int raw = radiansToRaw(cmd, motor);
            motor_ids.push_back(static_cast<u8>(motor.id));
            positions.push_back(static_cast<s16>(raw));
            speeds.push_back(speed);
            accelerations.push_back(accel);
        }
    }

    if (!motor_ids.empty()) {
        servo_driver_.SyncWritePosEx(
            motor_ids.data(), motor_ids.size(),
            positions.data(), speeds.data(), accelerations.data());
    }

    // Ждём завершения движения (с таймаутом)
    const int timeout_seconds = 10;
    const int sleep_ms = 50;
    int elapsed = 0;
    bool all_stopped = false;

    while (elapsed < timeout_seconds * 1000) {
        bool moving = false;
        for (size_t i = 0; i < info_.joints.size(); ++i) {
            int motor_id = motors_[i].id;
            if (servo_driver_.FeedBack(motor_id) != 0) {
                int moving_flag = servo_driver_.ReadMove(motor_id);
                motors_[i].sensors.moving_flag = static_cast<double>(moving_flag);
                if (moving_flag != 0) {
                    moving = true;
                }
            }
        }

        if (!moving) {
            all_stopped = true;
            break;
        }

        rclcpp::sleep_for(std::chrono::milliseconds(sleep_ms));
        elapsed += sleep_ms;
    }

    if (all_stopped) {
        RCLCPP_INFO(rclcpp::get_logger("SOARM101SystemHardware"), 
                    "Park position reached.");
    } else {
        RCLCPP_WARN(rclcpp::get_logger("SOARM101SystemHardware"), 
                    "Park position timeout after %d seconds, continuing.", timeout_seconds);
    }
}

}  // namespace soarm101_hardware

#include "pluginlib/class_list_macros.hpp"
PLUGINLIB_EXPORT_CLASS(soarm101_hardware::SOARM101SystemHardware, hardware_interface::SystemInterface)