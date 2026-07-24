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

#define ENABLE_SERVO  1
#define DISABLE_SERVO 0
#define NUM_MOTORS 6
//! Макрос для включения сервопривода (Enable Torque)
#define ENABLE_SERVO  1
//! Макрос для отключения сервопривода (Disable Torque)
#define DISABLE_SERVO 0
//! Количество моторов в роботе (фиксированное)
#define NUM_MOTORS 6

namespace soarm101_hardware
{

/**
 * @brief Главный класс аппаратного интерфейса для робота SO-ARM101.
 * 
 * Реализует hardware_interface::SystemInterface для управления 6 сервоприводами
 * Feetech STS через последовательный порт. Предоставляет интерфейсы состояния
 * (позиция, скорость, усилие, температура, напряжение, ток, флаг движения)
 * и команды (позиция).
 * 
 * Основные возможности:
 * - Загрузка калибровки из YAML-файла.
 * - Парковочная позиция при деактивации.
 * - Настраиваемые скорость и ускорение.
 * - Оптимизированные преобразования raw ↔ радианы.
 */
class SOARM101SystemHardware : public hardware_interface::SystemInterface
{
public:
  RCLCPP_SHARED_PTR_DEFINITIONS(SOARM101SystemHardware)

  /**
   * @brief Конструктор. Инициализирует флаги.
   */
  SOARM101SystemHardware();

  /**
   * @brief Деструктор. Отключает драйвер, если он был инициализирован.
   */
  ~SOARM101SystemHardware();

  // ==================== Методы, переопределяемые из SystemInterface ====================

  /**
   * @brief Инициализация компонента.
   * @param info Структура HardwareInfo с параметрами из ROS 2.
   * @return SUCCESS при успехе, иначе ERROR.
   * 
   * Вызывается один раз при загрузке плагина. Читает параметры порта,
   * скорости, файла калибровки, а также пользовательские параметры
   * default_speed, default_accel, park_positions. Создаёт структуры для моторов.
   */
  hardware_interface::CallbackReturn on_init(
    const hardware_interface::HardwareInfo & info) override;

  /**
   * @brief Конфигурация компонента.
   * @param previous_state Состояние до конфигурации (не используется).
   * @return SUCCESS при успехе, иначе ERROR.
   * 
   * Загружает калибровку, подключается к последовательному порту,
   * инициализирует команды текущими позициями.
   */
  hardware_interface::CallbackReturn on_configure(
    const rclcpp_lifecycle::State & previous_state) override;

  /**
   * @brief Экспорт интерфейсов состояния (чтение данных с моторов).
   * @return Вектор StateInterface, содержащий указатели на поля sensors каждого мотора.
   * 
   * Экспортирует: position, velocity, effort, temperature, voltage, current, moving_flag.
   */
  std::vector<hardware_interface::StateInterface> export_state_interfaces() override;

  /**
   * @brief Экспорт интерфейсов команд (запись данных на моторы).
   * @return Вектор CommandInterface, содержащий указатели на command_position каждого мотора.
   * 
   * Экспортирует только position (позиционный режим).
   */
  std::vector<hardware_interface::CommandInterface> export_command_interfaces() override;

  /**
   * @brief Активация компонента.
   * @param previous_state Состояние до активации (не используется).
   * @return SUCCESS при успехе, иначе ERROR.
   * 
   * Включает момент на всех моторах, читает текущие позиции и устанавливает
   * команды равными текущим позициям для предотвращения рывков.
   */
  hardware_interface::CallbackReturn on_activate(
    const rclcpp_lifecycle::State & previous_state) override;

  /**
   * @brief Деактивация компонента.
   * @param previous_state Состояние до деактивации (не используется).
   * @return SUCCESS при успехе, иначе ERROR.
   * 
   * Если задана парковочная позиция, перемещает робота в неё (с пониженной скоростью),
   * затем отключает момент на всех моторах.
   */
  hardware_interface::CallbackReturn on_deactivate(
    const rclcpp_lifecycle::State & previous_state) override;

  /**
   * @brief Чтение данных с моторов.
   * @param time Текущее время (не используется).
   * @param period Период (не используется).
   * @return return_type::OK всегда.
   * 
   * Вызывается циклически (обычно 100 Гц). Обновляет поля sensors.position и др.
   */
  hardware_interface::return_type read(
    const rclcpp::Time & time, const rclcpp::Duration & period) override;

  /**
   * @brief Запись команд на моторы.
   * @param time Текущее время (не используется).
   * @param period Период (не используется).
   * @return return_type::OK всегда.
   * 
   * Вызывается циклически. Отправляет команды позиции синхронно с использованием
   * SyncWritePosEx. Скорость и ускорение задаются параметрами default_speed_ и default_accel_.
   */
  hardware_interface::return_type write(
    const rclcpp::Time & time, const rclcpp::Duration & period) override;

private:
  // ==================== Вложенные структуры ====================

  /**
   * @brief Показания датчиков одного мотора.
   */
  struct MotorSensor
  {
    double position = 0.0;      ///< Текущая позиция, радианы
    double velocity = 0.0;      ///< Текущая скорость, рад/с
    double effort   = 0.0;      ///< Текущее усилие (нагрузка), условные единицы
    double temperature = 0.0;   ///< Температура мотора, °C
    double voltage  = 0.0;      ///< Напряжение питания, В
    double current  = 0.0;      ///< Ток, А
    double moving_flag = 0.0;   ///< Флаг движения (0 – стоп, 1 – движется)
  };

  /**
   * @brief Параметры калибровки мотора.
   * 
   * Содержит как сырые пределы (range_min, range_max), так и предвычисленные
   * коэффициенты для быстрого преобразования raw ↔ радианы.
   */
  struct MotorCalibration
  {
    int drive_mode;                 ///< Режим работы мотора (не используется)
    int range_min;                  ///< Минимальное сырое значение энкодера
    int range_max;                  ///< Максимальное сырое значение энкодера
    double raw_to_rad_scale;        ///< Коэффициент для преобразования raw->радианы
    double raw_to_rad_offset;       ///< Смещение для преобразования raw->радианы
    double rad_to_raw_scale;        ///< Коэффициент для преобразования радианы->raw
    double rad_to_raw_offset;       ///< Смещение для преобразования радианы->raw
    double urdf_lower;              ///< Нижний предел из URDF, радианы
    double urdf_upper;              ///< Верхний предел из URDF, радианы
  };

  /**
   * @brief Агрегированная структура для одного мотора.
   * 
   * Содержит всю информацию: идентификатор, имя, целевую команду,
   * показания датчиков и калибровку.
   */
  struct Motor
  {
    int id;                         ///< Идентификатор мотора (1..6)
    std::string joint_name;         ///< Имя сустава из URDF
    double command_position = 0.0;  ///< Целевая позиция, радианы
    MotorSensor sensors;            ///< Показания датчиков
    MotorCalibration calibration;   ///< Калибровочные параметры
  };

  // ==================== Члены класса ====================

  //! Параметры из конфигурации
  std::string port_;                ///< Последовательный порт (например, /dev/ttyACM0)
  int         baudrate_;            ///< Скорость передачи, бод
  std::string calibration_file_;    ///< Путь к файлу калибровки YAML

  //! Параметры, загружаемые из info_.hardware_parameters
  u16 default_speed_ = 2400;        ///< Скорость по умолчанию для записи
  u8  default_accel_ = 50;          ///< Ускорение по умолчанию для записи
  std::vector<double> park_positions_; ///< Парковочная позиция (радианы) в порядке info_.joints

  //! Данные по моторам
  std::vector<Motor> motors_;       ///< Вектор структур Motor, размер = info_.joints.size()
  std::map<std::string, int> motor_ids_; ///< Маппинг имени сустава -> ID мотора

  //! Драйвер SCServo SDK
  SMS_STS servo_driver_;
  bool driver_initialized_;         ///< Флаг успешного подключения к драйверу

  // ==================== Приватные методы ====================

  /**
   * @brief Загрузка калибровки из YAML-файла.
   * @return true при успехе, false при ошибке.
   * 
   * Парсит файл, имена суставов должны совпадать с ключами в motor_ids_.
   * Для каждого мотора вызывает updateCalibrationCoefficients().
   */
  bool loadCalibration();

  /**
   * @brief Вычисление предварительных коэффициентов для быстрого преобразования.
   * @param motor Ссылка на структуру Motor, у которой будет обновлена calibration.
   * 
   * Определяет URDF-лимиты на основе motor.id, затем вычисляет raw_to_rad_scale и др.
   */
  void updateCalibrationCoefficients(Motor & motor);

  /**
   * @brief Чтение данных с одного мотора.
   * @param index Индекс в векторе info_.joints и motors_.
   * 
   * Вызывает FeedBack, затем ReadPos, ReadSpeed и т.д., заполняет sensors.
   */
  void readMotorData(size_t index);

  /**
   * @brief Преобразование сырого значения энкодера в радианы.
   * @param raw_position Сырое значение (0-4095).
   * @param motor Ссылка на Motor, содержащий калибровку.
   * @return Угол в радианах.
   * 
   * Использует предвычисленные коэффициенты из motor.calibration.
   */
  double rawToRadians(int raw_position, const Motor & motor);

  /**
   * @brief Преобразование радиан в сырое значение энкодера.
   * @param radians Угол в радианах.
   * @param motor Ссылка на Motor, содержащий калибровку.
   * @return Сырое значение (0-4095), клиппированное по URDF-лимитам.
   */
  int radiansToRaw(double radians, const Motor & motor);

  /**
   * @brief Перемещение робота в парковочную позицию.
   * 
   * Устанавливает command_position из park_positions_, отправляет команды
   * с половинной скоростью/ускорением, затем ждёт завершения движения (таймаут 10 с).
   */
  void moveToParkPosition();

  /**
   * @brief Парсинг строки с массивом чисел для park_positions.
   * @param str Строка вида "[0.004, -1.712, ...]" или "0.004, -1.712, ..."
   * @return Вектор чисел double.
   */
  std::vector<double> parseParkPositions(const std::string & str);
};

}  // namespace soarm101_hardware

#endif  // SOARM101_HARDWARE__SOARM101_SYSTEM_HPP_