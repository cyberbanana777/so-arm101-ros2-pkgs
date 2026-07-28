# Пакет: scservo_sdk

Это пакет **scservo_sdk**.

---

## Файл: `CMakeLists.txt`

```text
cmake_minimum_required(VERSION 3.8)
project(scservo_sdk)

# Настройки C++17
if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 17)
endif()
if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Поиск зависимостей
find_package(ament_cmake REQUIRED)

# Список всех исходников SDK
set(SCSERVO_SOURCES
  scservo_sdk/HLSCL.cpp
  scservo_sdk/SCS.cpp
  scservo_sdk/SCS0009.cpp
  scservo_sdk/SCSCL.cpp
  scservo_sdk/SCSerial.cpp
  scservo_sdk/SMS_STS.cpp
)

# Список заголовков (чтобы IDE их видела, и для установки)
set(SCSERVO_HEADERS
  scservo_sdk/HLSCL.h
  scservo_sdk/INST.h
  scservo_sdk/SCS.h
  scservo_sdk/SCS0009.h
  scservo_sdk/SCSCL.h
  scservo_sdk/SCSerial.h
  scservo_sdk/SCServo.h
  scservo_sdk/ServoErrors.h
  scservo_sdk/ServoUtils.h
  scservo_sdk/SMS_STS.h
  scservo_sdk/SyncWriteBuffer.h
)

# Создаём статическую библиотеку (можно SHARED, но статическая проще для встраивания)
add_library(scservo_sdk STATIC ${SCSERVO_SOURCES} ${SCSERVO_HEADERS})

set_target_properties(scservo_sdk PROPERTIES
  POSITION_INDEPENDENT_CODE ON
)

# Прилинковываем зависимости
target_include_directories(scservo_sdk
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/scservo_sdk>
    $<INSTALL_INTERFACE:include/${PROJECT_NAME}>
)
# Если нужен последовательный порт и прочее, добавь соответствующие find_package и линковку (например, Boost::system, -lpthread)

# Экспортируем библиотеку для других ROS2-пакетов
ament_export_targets(export_scservo_sdk HAS_LIBRARY_TARGET)

# Установка библиотеки и заголовков
install(
  TARGETS scservo_sdk
  EXPORT export_scservo_sdk
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
  RUNTIME DESTINATION bin
  INCLUDES DESTINATION include/${PROJECT_NAME}
)

# Копируем все заголовки в папку include/soarm101_sdk
install(
  DIRECTORY scservo_sdk/
  DESTINATION include/${PROJECT_NAME}
  FILES_MATCHING PATTERN "*.h"
)

# Стандартный вызов
ament_package()
```

## Файл: `LICENSE`

```
Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
```

## Файл: `ORIGINAL_LINK_REPO`

```
https://github.com/adityakamath/SCServo_Linux
```

## Файл: `package.xml`

```xml
<?xml version="1.0"?>
<?xml-model href="http://download.ros.org/schema/package_format3.xsd" schematypens="http://www.w3.org/2001/XMLSchema"?>
<package format="3">
  <name>scservo_sdk</name>
  <version>1.0.0</version>
  <description>Low-level SDK for SCServo motors (SCSCL, SMS_STS)</description>
  <maintainer email="sashagrachev2005@gmail.com">banana-killer</maintainer>
  <license>MIT</license>

  <buildtool_depend>ament_cmake</buildtool_depend>

  <test_depend>ament_lint_auto</test_depend>
  <test_depend>ament_lint_common</test_depend>

  <export>
    <build_type>ament_cmake</build_type>
  </export>
</package>
```

## Файл: `scservo_sdk/HLSCL.cpp`

```cpp
/**
 * @file HLSCL.cpp
 * @brief Feetech HTS/HLS Series Serial Servo Application Layer Implementation
 *
 * @details This file implements high-level control functions for Feetech HLS
 * series servo motors. Supports three operating modes with complete read/write
 * functionality and LSP-compliant initialization.
 *
 * **Implemented Features:**
 * - Mode 0: Position control with speed, acceleration, and torque
 * - Mode 1: Velocity control (constant velocity wheel mode)
 * - Mode 2: Force/Torque control (constant torque output mode)
 * - Synchronized writes for multi-motor coordination
 * - Buffered writes with RegWriteAction
 * - Comprehensive feedback reading (position, speed, load, voltage, temp, current)
 *
 * **Refactoring Improvements:**
 * - Uses ServoUtils for direction bit encoding/decoding (DRY principle)
 * - Uses SyncWriteBuffer for automatic memory management (RAII)
 * - Standardized error handling with ServoErrors
 *
 * @see HLSCL.h for class interface and usage examples
 */

#include "HLSCL.h"
#include "SyncWriteBuffer.h"

HLSCL::HLSCL()
{
	End = 0;
}

HLSCL::HLSCL(u8 End):SCSerial(End)
{
}

HLSCL::HLSCL(u8 End, u8 Level):SCSerial(End, Level)
{
}

/**
 * @brief Write position, speed, acceleration, and torque to servo
 * 
 * Sends single-servo position command with full parameter control.
 * Negative positions are encoded with direction bit using ServoUtils.
 * 
 * @param ID Servo ID
 * @param Position Target position (±4095 steps)
 * @param Speed Moving speed (0-3400 steps/s)
 * @param ACC Acceleration value (0-254)
 * @param Torque Torque limit (0-1000)
 * @return 1 on success, 0 on failure
 */
int HLSCL::WritePosEx(u8 ID, s16 Position, u16 Speed, u8 ACC, u16 Torque)
{
	u16 encodedPosition = ServoUtils::encodeSignedValue(Position, HLSCL_DIRECTION_BIT_POS);

	u8 bBuf[7];
	bBuf[0] = ACC;
	Host2SCS(bBuf+1, bBuf+2, encodedPosition);
	Host2SCS(bBuf+3, bBuf+4, Torque);
	Host2SCS(bBuf+5, bBuf+6, Speed);

	return genWrite(ID, HLSCL_ACC, bBuf, 7);
}

/**
 * @brief Register write position command (executes on RegWriteAction)
 * 
 * Queues position/speed/acceleration/torque command for later execution.
 * Use with RegWriteAction for synchronized multi-servo motion.
 * 
 * @param ID Servo ID
 * @param Position Target position (±4095 steps)
 * @param Speed Moving speed (0-3400 steps/s)
 * @param ACC Acceleration value (0-254)
 * @param Torque Torque limit (0-1000)
 * @return 1 on success, 0 on failure
 */
int HLSCL::RegWritePosEx(u8 ID, s16 Position, u16 Speed, u8 ACC, u16 Torque)
{
	u16 encodedPosition = ServoUtils::encodeSignedValue(Position, HLSCL_DIRECTION_BIT_POS);

	u8 bBuf[7];
	bBuf[0] = ACC;
	Host2SCS(bBuf+1, bBuf+2, encodedPosition);
	Host2SCS(bBuf+3, bBuf+4, Torque);
	Host2SCS(bBuf+5, bBuf+6, Speed);

	return regWrite(ID, HLSCL_ACC, bBuf, 7);
}

/**
 * @brief Synchronized position write for multiple servos
 * 
 * Uses RAII-based SyncWriteBuffer for automatic memory management.
 * Encodes signed positions using ServoUtils helper.
 * 
 * @param ID Array of servo IDs
 * @param IDN Number of servos
 * @param Position Array of target positions (±4095 steps)
 * @param Speed Array of speeds (0-3400 steps/s)
 * @param ACC Array of accelerations (0-254, can be nullptr for 0)
 * @param Torque Array of torque limits (0-1000)
 */
void HLSCL::SyncWritePosEx(u8 ID[], u8 IDN, s16 Position[], u16 Speed[], u8 ACC[], u16 Torque[])
{
	SyncWriteBuffer buffer(IDN, 7);
	if (!buffer.isValid()) {
		return;  // Allocation failed
	}

	u8* offbuf = buffer.getBuffer();
	for(u8 i = 0; i < IDN; i++) {
		u16 encodedPosition = ServoUtils::encodeSignedValue(Position[i], HLSCL_DIRECTION_BIT_POS);
		
		if(ACC) {
			offbuf[i*7] = ACC[i];
		} else {
			offbuf[i*7] = 0;
		}
		Host2SCS(offbuf+i*7+1, offbuf+i*7+2, encodedPosition);
		Host2SCS(offbuf+i*7+3, offbuf+i*7+4, Torque[i]);
		Host2SCS(offbuf+i*7+5, offbuf+i*7+6, Speed[i]);
	}
	syncWrite(ID, IDN, HLSCL_ACC, offbuf, 7);
}

/**
 * @brief Synchronized speed write for multiple servos
 * 
 * Uses RAII-based SyncWriteBuffer for automatic memory management.
 * Encodes signed speeds using ServoUtils helper.
 * 
 * @param ID Array of servo IDs
 * @param IDN Number of servos
 * @param Speed Array of speeds (±3400 steps/s)
 * @param ACC Array of accelerations (0-254, can be nullptr for 0)
 * @param Torque Array of torque limits (0-1000)
 */
void HLSCL::SyncWriteSpe(u8 ID[], u8 IDN, s16 Speed[], u8 ACC[], u16 Torque[])
{
	SyncWriteBuffer buffer(IDN, 7);
	if (!buffer.isValid()) {
		return;  // Allocation failed
	}

	u8* offbuf = buffer.getBuffer();
	for(u8 i = 0; i < IDN; i++) {
		u16 encodedSpeed = ServoUtils::encodeSignedValue(Speed[i], HLSCL_DIRECTION_BIT_SPEED);
		
		if(ACC) {
			offbuf[i*7] = ACC[i];
		} else {
			offbuf[i*7] = 0;
		}
		Host2SCS(offbuf+i*7+1, offbuf+i*7+2, 0);
		Host2SCS(offbuf+i*7+3, offbuf+i*7+4, Torque[i]);
		Host2SCS(offbuf+i*7+5, offbuf+i*7+6, encodedSpeed);
	}
	syncWrite(ID, IDN, HLSCL_ACC, offbuf, 7);
}

int HLSCL::WheelMode(u8 ID)
{
	return writeByte(ID, HLSCL_MODE, 1);
}

int HLSCL::EleMode(u8 ID)
{
	return writeByte(ID, HLSCL_MODE, 2);
}

/**
 * @brief Write speed command for constant velocity mode
 * 
 * Sends single-servo speed command with acceleration and torque control.
 * Negative speeds are encoded with direction bit using ServoUtils.
 * 
 * @param ID Servo ID
 * @param Speed Target velocity (±3400 steps/s)
 * @param ACC Acceleration value (0-254)
 * @param Torque Torque limit (0-1000)
 * @return 1 on success, 0 on failure
 */
int HLSCL::WriteSpe(u8 ID, s16 Speed, u8 ACC, u16 Torque)
{
	u16 encodedSpeed = ServoUtils::encodeSignedValue(Speed, HLSCL_DIRECTION_BIT_SPEED);

	u8 bBuf[7];
	bBuf[0] = ACC;
	Host2SCS(bBuf+1, bBuf+2, 0);
	Host2SCS(bBuf+3, bBuf+4, Torque);
	Host2SCS(bBuf+5, bBuf+6, encodedSpeed);

	return genWrite(ID, HLSCL_ACC, bBuf, 7);
}

/**
 * @brief Write torque command for electric/force mode
 * 
 * Sends single-servo torque command for constant torque output.
 * Negative torques are encoded with direction bit using ServoUtils.
 * 
 * @param ID Servo ID
 * @param Torque Target torque (±1000)
 * @return 1 on success, 0 on failure
 */
int HLSCL::WriteEle(u8 ID, s16 Torque)
{
	u16 encodedTorque = ServoUtils::encodeSignedValue(Torque, HLSCL_DIRECTION_BIT_TORQUE);
	return writeWord(ID, HLSCL_GOAL_TORQUE_L, encodedTorque);
}

int HLSCL::EnableTorque(u8 ID, u8 Enable)
{
	return writeByte(ID, HLSCL_TORQUE_ENABLE, Enable);
}

int HLSCL::unLockEprom(u8 ID)
{
	// Disable torque before unlocking EEPROM
	int ret = EnableTorque(ID, 0);
	if(ret != 1){
		return ret;  // Propagate error if torque disable failed
	}
	
	// Unlock EEPROM
	return writeByte(ID, HLSCL_LOCK, 0);
}

int HLSCL::LockEprom(u8 ID)
{
	return writeByte(ID, HLSCL_LOCK, 1);
}

int HLSCL::CalibrationOfs(u8 ID)
{
	int ret;
	
	// Disable torque before calibration
	ret = EnableTorque(ID, 0);
	if(ret != 1){
		return ret;  // Propagate error
	}
	
	// Unlock EEPROM to allow calibration write
	ret = unLockEprom(ID);
	if(ret != 1){
		return ret;  // Propagate error
	}
	
	// Send calibration command
	return writeByte(ID, HLSCL_TORQUE_ENABLE, 128);  // 128 = Calibration command
}

int HLSCL::FeedBack(int ID)
{
	int nLen = Read(ID, HLSCL_PRESENT_POSITION_L, Mem, sizeof(Mem));
	if(nLen!=sizeof(Mem)){
		return -1;
	}
	return nLen;
}

/**
 * @brief Read current position from servo or cached data
 * 
 * Uses ServoUtils for direction bit decoding.
 * 
 * @param ID Servo ID or -1 for cached data
 * @return Position (±4095 steps) or -1 on error
 */
int HLSCL::ReadPos(int ID)
{
	if(ServoUtils::isCachedRead(ID)) {
		u16 encodedPos = ServoUtils::readWordFromBuffer(Mem, 
			HLSCL_PRESENT_POSITION_L - HLSCL_PRESENT_POSITION_L,
			HLSCL_PRESENT_POSITION_H - HLSCL_PRESENT_POSITION_L);
		return ServoUtils::decodeSignedValue(encodedPos, HLSCL_DIRECTION_BIT_POS);
	} else {
		int Pos = readWord(ID, HLSCL_PRESENT_POSITION_L);
		if(Pos == -1) {
			return -1;
		}
		return ServoUtils::decodeSignedValue(static_cast<u16>(Pos), HLSCL_DIRECTION_BIT_POS);
	}
}

/**
 * @brief Read current speed from servo or cached data
 * 
 * Uses ServoUtils for direction bit decoding.
 * 
 * @param ID Servo ID or -1 for cached data
 * @return Speed (±3400 steps/s) or -1 on error
 */
int HLSCL::ReadSpeed(int ID)
{
	if(ServoUtils::isCachedRead(ID)) {
		u16 encodedSpeed = ServoUtils::readWordFromBuffer(Mem,
			HLSCL_PRESENT_SPEED_L - HLSCL_PRESENT_POSITION_L,
			HLSCL_PRESENT_SPEED_H - HLSCL_PRESENT_POSITION_L);
		return ServoUtils::decodeSignedValue(encodedSpeed, HLSCL_DIRECTION_BIT_SPEED);
	} else {
		int Speed = readWord(ID, HLSCL_PRESENT_SPEED_L);
		if(Speed == -1) {
			return -1;
		}
		return ServoUtils::decodeSignedValue(static_cast<u16>(Speed), HLSCL_DIRECTION_BIT_SPEED);
	}
}

/**
 * @brief Read current load from servo or cached data
 * 
 * Uses ServoUtils for direction bit decoding (bit 10 for load).
 * 
 * @param ID Servo ID or -1 for cached data
 * @return Load (±1000) or -1 on error
 */
int HLSCL::ReadLoad(int ID)
{
	if(ServoUtils::isCachedRead(ID)) {
		u16 encodedLoad = ServoUtils::readWordFromBuffer(Mem,
			HLSCL_PRESENT_LOAD_L - HLSCL_PRESENT_POSITION_L,
			HLSCL_PRESENT_LOAD_H - HLSCL_PRESENT_POSITION_L);
		return ServoUtils::decodeSignedValue(encodedLoad, HLSCL_DIRECTION_BIT_LOAD);
	} else {
		int Load = readWord(ID, HLSCL_PRESENT_LOAD_L);
		if(Load == -1) {
			return -1;
		}
		return ServoUtils::decodeSignedValue(static_cast<u16>(Load), HLSCL_DIRECTION_BIT_LOAD);
	}
}

/**
 * @brief Read voltage from servo or cached data
 * 
 * @param ID Servo ID or -1 for cached data
 * @return Voltage in 0.1V units (e.g., 120=12.0V) or -1 on error
 */
int HLSCL::ReadVoltage(int ID)
{
	if(ServoUtils::isCachedRead(ID)) {
		return Mem[HLSCL_PRESENT_VOLTAGE - HLSCL_PRESENT_POSITION_L];
	} else {
		return readByte(ID, HLSCL_PRESENT_VOLTAGE);
	}
}

/**
 * @brief Read temperature from servo or cached data
 * 
 * @param ID Servo ID or -1 for cached data
 * @return Temperature in °C or -1 on error
 */
int HLSCL::ReadTemper(int ID)
{
	if(ServoUtils::isCachedRead(ID)) {
		return Mem[HLSCL_PRESENT_TEMPERATURE - HLSCL_PRESENT_POSITION_L];
	} else {
		return readByte(ID, HLSCL_PRESENT_TEMPERATURE);
	}
}

/**
 * @brief Read movement status from servo or cached data
 * 
 * @param ID Servo ID or -1 for cached data
 * @return 1=moving, 0=stopped, -1=error
 */
int HLSCL::ReadMove(int ID)
{
	if(ServoUtils::isCachedRead(ID)) {
		return Mem[HLSCL_MOVING - HLSCL_PRESENT_POSITION_L];
	} else {
		return readByte(ID, HLSCL_MOVING);
	}
}

/**
 * @brief Read current from servo or cached data
 * 
 * Uses ServoUtils for direction bit decoding.
 * 
 * @param ID Servo ID or -1 for cached data
 * @return Current in mA or -1 on error
 */
int HLSCL::ReadCurrent(int ID)
{
	if(ServoUtils::isCachedRead(ID)) {
		u16 encodedCurrent = ServoUtils::readWordFromBuffer(Mem,
			HLSCL_PRESENT_CURRENT_L - HLSCL_PRESENT_POSITION_L,
			HLSCL_PRESENT_CURRENT_H - HLSCL_PRESENT_POSITION_L);
		return ServoUtils::decodeSignedValue(encodedCurrent, HLSCL_DIRECTION_BIT_CURRENT);
	} else {
		int Current = readWord(ID, HLSCL_PRESENT_CURRENT_L);
		if(Current == -1) {
			return -1;
		}
		return ServoUtils::decodeSignedValue(static_cast<u16>(Current), HLSCL_DIRECTION_BIT_CURRENT);
	}
}

int HLSCL::ServoMode(u8 ID)
{
	return writeByte(ID, HLSCL_MODE, 0);
}
```

## Файл: `scservo_sdk/HLSCL.h`

```cpp
/**
 * @file HLSCL.h
 * @brief Feetech HLS Series Serial Servo Application Layer
 *
 * @details This file provides the application programming interface for
 * controlling Feetech HLS series serial bus servo motors.
 * Supports three operating modes:
 * - Mode 0: Servo (position control)
 * - Mode 1: Wheel (constant velocity control with feedback)
 * - Mode 2: Electric/Force (constant torque output control)
 */

#ifndef _HLSCL_H
#define _HLSCL_H

//Memory table definition
//-------EPROM (Read only)--------
#define HLSCL_MODEL_L 3
#define HLSCL_MODEL_H 4

//-------EPROM (Read and Write)--------
#define HLSCL_ID 5
#define HLSCL_BAUD_RATE 6
#define HLSCL_SECOND_ID 7
#define HLSCL_MIN_ANGLE_LIMIT_L 9
#define HLSCL_MIN_ANGLE_LIMIT_H 10
#define HLSCL_MAX_ANGLE_LIMIT_L 11
#define HLSCL_MAX_ANGLE_LIMIT_H 12
#define HLSCL_CW_DEAD 26
#define HLSCL_CCW_DEAD 27
#define HLSCL_OFS_L 31
#define HLSCL_OFS_H 32
#define HLSCL_MODE 33

//-------SRAM (Read and Write)--------
#define HLSCL_TORQUE_ENABLE 40
#define HLSCL_ACC 41
#define HLSCL_GOAL_POSITION_L 42
#define HLSCL_GOAL_POSITION_H 43
#define HLSCL_GOAL_TORQUE_L 44
#define HLSCL_GOAL_TORQUE_H 45
#define HLSCL_GOAL_SPEED_L 46
#define HLSCL_GOAL_SPEED_H 47
#define HLSCL_TORQUE_LIMIT_L 48
#define HLSCL_TORQUE_LIMIT_H 49
#define HLSCL_LOCK 55

//-------SRAM (Read only)--------
#define HLSCL_PRESENT_POSITION_L 56
#define HLSCL_PRESENT_POSITION_H 57
#define HLSCL_PRESENT_SPEED_L 58
#define HLSCL_PRESENT_SPEED_H 59
#define HLSCL_PRESENT_LOAD_L 60
#define HLSCL_PRESENT_LOAD_H 61
#define HLSCL_PRESENT_VOLTAGE 62
#define HLSCL_PRESENT_TEMPERATURE 63
#define HLSCL_MOVING 66
#define HLSCL_PRESENT_CURRENT_L 69
#define HLSCL_PRESENT_CURRENT_H 70

// Operating mode values
#define HLSCL_MODE_SERVO 0        // Servo mode (position control)
#define HLSCL_MODE_WHEEL 1        // Wheel mode (constant velocity control)
#define HLSCL_MODE_ELECTRIC 2     // Electric/Force mode (constant torque output)

// Direction bit positions (for encoding signed values)
#define HLSCL_DIRECTION_BIT_POS 15    // Position direction bit (bit 15)
#define HLSCL_DIRECTION_BIT_SPEED 15  // Speed direction bit (bit 15)
#define HLSCL_DIRECTION_BIT_TORQUE 15 // Torque direction bit (bit 15)
#define HLSCL_DIRECTION_BIT_LOAD 10   // Load direction bit (bit 10)
#define HLSCL_DIRECTION_BIT_CURRENT 15 // Current direction bit (bit 15)

#include "SCSerial.h"
#include "ServoUtils.h"
#include "ServoErrors.h"

/**
 * @class HLSCL
 * @brief Application layer interface for HLS series serial servos
 *
 * @details Provides high-level control functions for Feetech HLS series
 * servo motors. Supports three operating modes with complete read/write functionality.
 *
 * **Operating Modes:**
 * - Mode 0: Servo mode (position control) - precise positioning
 * - Mode 1: Wheel mode (velocity control) - constant speed rotation
 * - Mode 2: Electric mode (force control) - constant torque output
 *
 * **Key Features:**
 * - Position control with speed, acceleration, and torque limiting
 * - Velocity control with acceleration and torque parameters
 * - Force/torque control mode (unique to HLS series)
 * - Synchronized multi-servo commands
 * - Buffered command execution with RegWrite
 * - Comprehensive feedback reading (position, speed, load, voltage, temperature, current)
 *
 * @see SCSerial for base serial communication functionality
 */
class HLSCL : public SCSerial
{
public:
	HLSCL();
	HLSCL(u8 End);
	HLSCL(u8 End, u8 Level);

	/**
	 * @brief Write position command to single servo
	 * @param ID Servo ID (0-253, 254=broadcast)
	 * @param Position Target position (0-4095 for 12-bit, 0-1023 for 10-bit)
	 * @param Speed Movement speed (0-3400 steps/s, 0=maximum)
	 * @param ACC Acceleration (0-254, units of 100 steps/s², 0=maximum)
	 * @param Torque Torque limit (0-1000, 0=no limit)
	 * @return 1 on success, 0 on failure
	 */
	int WritePosEx(u8 ID, s16 Position, u16 Speed, u8 ACC = 0, u16 Torque = 0);

	/**
	 * @brief Buffered position write (executes on RegWriteAction)
	 * @param ID Servo ID (0-253, 254=broadcast)
	 * @param Position Target position (0-4095 for 12-bit, 0-1023 for 10-bit)
	 * @param Speed Movement speed (0-3400 steps/s)
	 * @param ACC Acceleration (0-254, units of 100 steps/s²)
	 * @param Torque Torque limit (0-1000)
	 * @return 1 on success, 0 on failure
	 */
	int RegWritePosEx(u8 ID, s16 Position, u16 Speed, u8 ACC = 0, u16 Torque = 0);

	/**
	 * @brief Synchronized position write for multiple servos
	 * @param ID Array of servo IDs
	 * @param IDN Number of servos
	 * @param Position Array of target positions
	 * @param Speed Array of speeds
	 * @param ACC Array of accelerations
	 * @param Torque Array of torque limits
	 */
	void SyncWritePosEx(u8 ID[], u8 IDN, s16 Position[], u16 Speed[], u8 ACC[], u16 Torque[]);

	/**
	 * @brief Synchronized speed write for multiple servos
	 * @param ID Array of servo IDs
	 * @param IDN Number of servos
	 * @param Speed Array of speeds (±3400 steps/s)
	 * @param ACC Array of accelerations
	 * @param Torque Array of torque limits
	 */
	void SyncWriteSpe(u8 ID[], u8 IDN, s16 Speed[], u8 ACC[], u16 Torque[]);

	/**
	 * @brief Set servo to position control mode (mode 0)
	 * @param ID Servo ID
	 * @return 1 on success, 0 on failure
	 */
	int ServoMode(u8 ID);

	/**
	 * @brief Set servo to constant velocity mode (mode 1)
	 * @param ID Servo ID
	 * @return 1 on success, 0 on failure
	 */
	int WheelMode(u8 ID);

	/**
	 * @brief Set servo to constant torque/force mode (mode 2)
	 * @param ID Servo ID
	 * @return 1 on success, 0 on failure
	 */
	int EleMode(u8 ID);

	/**
	 * @brief Write speed command for constant velocity mode
	 * @param ID Servo ID (0-253, 254=broadcast)
	 * @param Speed Target velocity (±3400 steps/s, negative=reverse)
	 * @param ACC Acceleration (0-254)
	 * @param Torque Torque limit (0-1000)
	 * @return 1 on success, 0 on failure
	 */
	int WriteSpe(u8 ID, s16 Speed, u8 ACC = 0, u16 Torque = 0);

	/**
	 * @brief Write torque command for electric/force mode
	 * @param ID Servo ID (0-253, 254=broadcast)
	 * @param Torque Target torque (±1000, negative=CCW, positive=CW)
	 * @return 1 on success, 0 on failure
	 */
	int WriteEle(u8 ID, s16 Torque);

	/**
	 * @brief Enable or disable motor torque
	 * @param ID Servo ID (0-253, 254=broadcast)
	 * @param Enable 1=enable, 0=disable
	 * @return 1 on success, 0 on failure
	 */
	int EnableTorque(u8 ID, u8 Enable);

	/**
	 * @brief Unlock EEPROM for writing
	 * @param ID Servo ID (0-253, 254=broadcast)
	 * @return 1 on success, 0 on failure
	 */
	int unLockEprom(u8 ID);

	/**
	 * @brief Lock EEPROM to prevent writes
	 * @param ID Servo ID (0-253, 254=broadcast)
	 * @return 1 on success, 0 on failure
	 */
	int LockEprom(u8 ID);

	/**
	 * @brief Calibrate servo center position
	 * @param ID Servo ID (0-253)
	 * @return 1 on success, 0 on failure
	 */
	int CalibrationOfs(u8 ID);

	/**
	 * @brief Request all feedback data from servo
	 * @param ID Servo ID (0-253, not broadcast)
	 * @return Size of data on success, -1 on failure
	 */
	int FeedBack(int ID);

	/**
	 * @brief Read current position
	 * @param ID Servo ID or -1 for cached data
	 * @return Position (0-4095) or -1 on error
	 */
	int ReadPos(int ID);

	/**
	 * @brief Read current speed
	 * @param ID Servo ID or -1 for cached data
	 * @return Speed in steps/s (signed) or -1 on error
	 */
	int ReadSpeed(int ID);

	/**
	 * @brief Read current load torque
	 * @param ID Servo ID or -1 for cached data
	 * @return Load (-1000 to +1000) or -1 on error
	 */
	int ReadLoad(int ID);

	/**
	 * @brief Read input voltage
	 * @param ID Servo ID or -1 for cached data
	 * @return Voltage in 0.1V units (e.g., 120=12.0V) or -1 on error
	 */
	int ReadVoltage(int ID);

	/**
	 * @brief Read servo temperature
	 * @param ID Servo ID or -1 for cached data
	 * @return Temperature in °C or -1 on error
	 */
	int ReadTemper(int ID);

	/**
	 * @brief Read movement status
	 * @param ID Servo ID or -1 for cached data
	 * @return 1=moving, 0=stopped, -1=error
	 */
	int ReadMove(int ID);

	/**
	 * @brief Read current draw
	 * @param ID Servo ID or -1 for cached data
	 * @return Current in mA or -1 on error
	 */
	int ReadCurrent(int ID);

private:
	u8 Mem[HLSCL_PRESENT_CURRENT_H-HLSCL_PRESENT_POSITION_L+1];
};

#endif
```

## Файл: `scservo_sdk/INST.h`

```cpp
/**
 * @file INST.h
 * @brief Feetech serial servo protocol instruction definitions and data types
 *
 * @details This file defines the fundamental protocol instructions, data types,
 * and buffer size constants used across all Feetech servo series. It provides
 * the low-level protocol command set for servo communication.
 *
 * **Protocol Instructions:**
 * - PING: Check servo connection
 * - READ/WRITE: Memory table access
 * - REG_WRITE/REG_ACTION: Asynchronous write operations
 * - SYNC_READ/SYNC_WRITE: Synchronized multi-servo operations
 *
 * **Data Types:**
 * - Signed/unsigned 8, 16, and 32-bit integers
 * - Platform-independent type definitions
 *
 * @note This file is included by all servo class headers (SMS_STS, SCSCL, HLSCL)
 */

#ifndef _INST_H
#define _INST_H

typedef	char s8;
typedef	unsigned char u8;	
typedef	unsigned short u16;	
typedef	short s16;
typedef	unsigned long u32;	
typedef	long s32;

#define INST_PING 0x01
#define INST_READ 0x02
#define INST_WRITE 0x03
#define INST_REG_WRITE 0x04
#define INST_REG_ACTION 0x05
#define INST_SYNC_READ 0x82
#define INST_SYNC_WRITE 0x83

// Buffer size constants
#define SCSERVO_BUFFER_SIZE 255
#define SCSERVO_HEADER_SIZE 6
#define SCSERVO_MAX_DATA_SIZE (SCSERVO_BUFFER_SIZE - SCSERVO_HEADER_SIZE)  // 249 bytes

#endif
```

## Файл: `scservo_sdk/SCS.cpp`

```cpp
﻿/**
 * @file SCS.cpp
 * @brief Feetech serial servo communication protocol layer implementation
 *
 * @details This file implements the low-level protocol for Feetech serial servo
 * communication. It handles packet construction, checksum calculation, command
 * encoding/decoding, and synchronized read/write operations.
 *
 * **Key Responsibilities:**
 * - Protocol packet formatting (header, ID, length, instruction, data, checksum)
 * - Command execution (PING, READ, WRITE, REG_WRITE, REG_ACTION, SYNC_WRITE, SYNC_READ)
 * - Checksum validation
 * - Byte ordering (endianness handling)
 * - Response parsing and error detection
 *
 * @note This is an abstract base class - use concrete implementations (SCSerial)
 * @see SCS.h for class interface documentation
 */
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include "SCS.h"

/**
 * @brief Default constructor for SCS base class
 * 
 * Initializes the servo communication with:
 * - Level = 1 (all instructions except broadcast return acknowledgement)
 * - Error = 0 (no error)
 */
SCS::SCS()
{
	Level = 1;  // All instructions except broadcast return acknowledgement
	Error = 0;
}

/**
 * @brief Constructor with endianness parameter
 * 
 * @param End Endianness flag (0=little-endian, 1=big-endian)
 */
SCS::SCS(u8 End)
{
	Level = 1;
	this->End = End;
	Error = 0;
}

/**
 * @brief Constructor with endianness and response level
 * 
 * @param End Endianness flag (0=little-endian, 1=big-endian)
 * @param Level Response level (1=return ACK, 0=no ACK for broadcast)
 */
SCS::SCS(u8 End, u8 Level)
{
	this->Level = Level;
	this->End = End;
	Error = 0;
}

/**
 * @brief Split one 16-bit value into two 8-bit values (host to servo format)
 * 
 * Handles endianness conversion between host and servo protocols.
 * 
 * @param DataL Pointer to store low byte
 * @param DataH Pointer to store high byte
 * @param Data 16-bit data value to split
 */
void SCS::Host2SCS(u8 *DataL, u8* DataH, u16 Data)
{
	if(End){
		*DataL = (Data>>8);
		*DataH = (Data&0xff);
	}else{
		*DataH = (Data>>8);
		*DataL = (Data&0xff);
	}
}

/**
 * @brief Combine two 8-bit values into one 16-bit value (servo to host format)
 * 
 * Handles endianness conversion between servo and host protocols.
 * 
 * @param DataL Low byte
 * @param DataH High byte
 * @return 16-bit combined value
 */
u16 SCS::SCS2Host(u8 DataL, u8 DataH)
{
	u16 Data;
	if(End){
		Data = DataL;
		Data<<=8;
		Data |= DataH;
	}else{
		Data = DataH;
		Data<<=8;
		Data |= DataL;
	}
	return Data;
}

/**
 * @brief Build and write command packet buffer
 * 
 * Internal function to construct protocol packets with header, ID, length, and checksum.
 * 
 * @param ID Servo ID
 * @param MemAddr Memory address to access
 * @param nDat Pointer to data buffer (NULL for commands without data)
 * @param nLen Data length
 * @param Fun Function code/instruction
 */
void SCS::writeBuf(u8 ID, u8 MemAddr, u8 *nDat, u8 nLen, u8 Fun)
{
	u8 msgLen = 2;
	u8 bBuf[6];
	u8 CheckSum = 0;
	bBuf[0] = 0xff;
	bBuf[1] = 0xff;
	bBuf[2] = ID;
	bBuf[4] = Fun;
	if(nDat){
		msgLen += nLen + 1;
		bBuf[3] = msgLen;
		bBuf[5] = MemAddr;
		writeSCS(bBuf, 6);
		
	}else{
		bBuf[3] = msgLen;
		writeSCS(bBuf, 5);
	}
	CheckSum = ID + msgLen + Fun + MemAddr;
	u8 i = 0;
	if(nDat){
		for(i=0; i<nLen; i++){
			CheckSum += nDat[i];
		}
		writeSCS(nDat, nLen);
	}
	writeSCS(~CheckSum);
}

/**
 * @brief Normal write instruction (synchronous)
 * 
 * Writes data to servo memory and waits for acknowledgement.
 * 
 * @param ID Servo ID (0-253, 0xFE for broadcast)
 * @param MemAddr Memory table address
 * @param nDat Pointer to data to write
 * @param nLen Length of data in bytes
 * @return 1 on success, 0 on failure
 */
int SCS::genWrite(u8 ID, u8 MemAddr, u8 *nDat, u8 nLen)
{
	rFlushSCS();
	writeBuf(ID, MemAddr, nDat, nLen, INST_WRITE);
	wFlushSCS();
	return Ack(ID);
}

/**
 * @brief Asynchronous write instruction (register write)
 * 
 * Registers a write command to be executed later with RegWriteAction().
 * Useful for synchronized multi-servo movements.
 * 
 * @param ID Servo ID
 * @param MemAddr Memory table address
 * @param nDat Pointer to data to write
 * @param nLen Length of data in bytes
 * @return 1 on success, 0 on failure
 */
int SCS::regWrite(u8 ID, u8 MemAddr, u8 *nDat, u8 nLen)
{
	rFlushSCS();
	writeBuf(ID, MemAddr, nDat, nLen, INST_REG_WRITE);
	wFlushSCS();
	return Ack(ID);
}

/**
 * @brief Execute all registered write commands
 * 
 * Triggers execution of all commands previously registered with regWrite().
 * Enables synchronized movement of multiple servos.
 * 
 * @param ID Servo ID (use 0xFE for broadcast to all servos)
 * @return 1 on success, 0 on failure
 */
int SCS::RegWriteAction(u8 ID)
{
	rFlushSCS();
	writeBuf(ID, 0, NULL, 0, INST_REG_ACTION);
	wFlushSCS();
	return Ack(ID);
}

/**
 * @brief Synchronous write instruction for multiple servos
 * 
 * Writes the same memory address with different data to multiple servos
 * in a single packet. More efficient than individual writes.
 * 
 * @param ID Array of servo IDs
 * @param IDN Number of servos (array length)
 * @param MemAddr Memory table address (same for all servos)
 * @param nDat Pointer to data buffer (IDN * nLen bytes)
 * @param nLen Length of data per servo
 */
void SCS::syncWrite(u8 ID[], u8 IDN, u8 MemAddr, u8 *nDat, u8 nLen)
{
	rFlushSCS();
	u8 mesLen = ((nLen+1)*IDN+4);
	u8 Sum = 0;
	u8 bBuf[7];
	bBuf[0] = 0xff;
	bBuf[1] = 0xff;
	bBuf[2] = 0xfe;
	bBuf[3] = mesLen;
	bBuf[4] = INST_SYNC_WRITE;
	bBuf[5] = MemAddr;
	bBuf[6] = nLen;
	writeSCS(bBuf, 7);

	Sum = 0xfe + mesLen + INST_SYNC_WRITE + MemAddr + nLen;
	u8 i, j;
	for(i=0; i<IDN; i++){
		writeSCS(ID[i]);
		writeSCS(nDat+i*nLen, nLen);
		Sum += ID[i];
		for(j=0; j<nLen; j++){
			Sum += nDat[i*nLen+j];
		}
	}
	writeSCS(~Sum);
	wFlushSCS();
}

int SCS::writeByte(u8 ID, u8 MemAddr, u8 bDat)
{
	rFlushSCS();
	writeBuf(ID, MemAddr, &bDat, 1, INST_WRITE);
	wFlushSCS();
	return Ack(ID);
}

int SCS::writeWord(u8 ID, u8 MemAddr, u16 wDat)
{
	u8 bBuf[2];
	Host2SCS(bBuf+0, bBuf+1, wDat);
	rFlushSCS();
	writeBuf(ID, MemAddr, bBuf, 2, INST_WRITE);
	wFlushSCS();
	return Ack(ID);
}

// Read instruction
// Servo ID, MemAddr memory table address, return data nData, data length nLen
int SCS::Read(u8 ID, u8 MemAddr, u8 *nData, u8 nLen)
{
	// NULL pointer check
	if(!nData){
		return 0;
	}

	// Validate buffer size against maximum data size
	if(nLen > SCSERVO_MAX_DATA_SIZE){
		return 0;
	}

	rFlushSCS();
	writeBuf(ID, MemAddr, &nLen, 1, INST_READ);
	wFlushSCS();

	u8 bBuf[SCSERVO_BUFFER_SIZE];
	u8 i;
	u8 calSum = 0;
	int Size = readSCS(bBuf, nLen+6);
	//printf("nLen+6 = %d, Size = %d\n", nLen+6, Size);
	if(Size!=(nLen+6)){
		return 0;
	}
	//for(i=0; i<Size; i++){
		//printf("%x\n", bBuf[i]);
	//}
	if(bBuf[0]!=0xff || bBuf[1]!=0xff){
		return 0;
	}
	for(i=2; i<(Size-1); i++){
		calSum += bBuf[i];
	}
	calSum = ~calSum;
	if(calSum!=bBuf[Size-1]){
		return 0;
	}
	memcpy(nData, bBuf+5, nLen);
	Error = bBuf[4];
	return nLen;
}

/**
 * @brief Read single byte from servo memory
 * 
 * Convenience function for reading one byte.
 * 
 * @param ID Servo ID
 * @param MemAddr Memory address to read
 * @return Byte value on success, -1 on timeout/error
 */
int SCS::readByte(u8 ID, u8 MemAddr)
{
	u8 bDat;
	int Size = Read(ID, MemAddr, &bDat, 1);
	if(Size!=1){
		return -1;
	}else{
		return bDat;
	}
}

/**
 * @brief Read 16-bit word from servo memory
 * 
 * Reads two consecutive bytes and combines them into a 16-bit value.
 * Handles endianness conversion automatically.
 * 
 * @param ID Servo ID
 * @param MemAddr Memory address to read (reads 2 bytes)
 * @return 16-bit word value on success, -1 on timeout/error
 */
int SCS::readWord(u8 ID, u8 MemAddr)
{	
	u8 nDat[2];
	int Size;
	u16 wDat;
	Size = Read(ID, MemAddr, nDat, 2);
	if(Size!=2)
		return -1;
	wDat = SCS2Host(nDat[0], nDat[1]);
	return wDat;
}

/**
 * @brief Ping servo to check if it's online
 * 
 * Sends ping command and waits for response to verify servo connectivity.
 * 
 * @param ID Servo ID to ping
 * @return Servo ID on success, -1 on timeout (servo not responding)
 */
int	SCS::Ping(u8 ID)
{
	rFlushSCS();
	writeBuf(ID, 0, NULL, 0, INST_PING);
	wFlushSCS();
	Error = 0;

	u8 bBuf[6];
	u8 i;
	u8 calSum = 0;
	int Size = readSCS(bBuf, 6);
	if(Size!=6){
		return -1;
	}
	if(bBuf[0]!=0xff || bBuf[1]!=0xff){
		return -1;
	}
	if(bBuf[2]!=ID && ID!=0xfe){
		return -1;
	}
	if(bBuf[3]!=2){
		return -1;
	}
	for(i=2; i<(Size-1); i++){
		calSum += bBuf[i];
	}
	calSum = ~calSum;
	if(calSum!=bBuf[Size-1]){
		return -1;
	}
	Error = bBuf[2];
	return Error;
}

/**
 * @brief Wait for and validate acknowledgment packet from servo
 * 
 * Reads response packet and validates checksum. Only waits if response
 * level is enabled (Level != 0) and not broadcasting (ID != 0xfe).
 * 
 * @param ID Servo ID to receive ack from
 * @return 1 on success (valid ack received), 0 on failure or when Level=0
 */
int	SCS::Ack(u8 ID)
{
	Error = 0;
	if(ID!=0xfe && Level){
		u8 bBuf[6];
		u8 i;
		u8 calSum = 0;
		int Size = readSCS(bBuf, 6);
		if(Size!=6){
			return 0;
		}
		if(bBuf[0]!=0xff || bBuf[1]!=0xff){
			return 0;
		}
		if(bBuf[2]!=ID){
			return 0;
		}
		if(bBuf[3]!=2){
			return 0;
		}
		for(i=2; i<(Size-1); i++){
			calSum += bBuf[i];
		}
		calSum = ~calSum;
		if(calSum!=bBuf[Size-1]){
			return 0;
		}
		Error = bBuf[4];
	}
	return 1;
}

/**
 * @brief Transmit sync read command and receive all responses
 * 
 * Sends INST_SYNC_READ command to multiple servos and reads all responses
 * into internal buffer for later parsing.
 * 
 * @param ID Array of servo IDs to read from
 * @param IDN Number of servo IDs in array
 * @param MemAddr Starting memory address to read
 * @param nLen Number of bytes to read per servo
 * @return Total bytes received in syncReadRxBuff
 */
int	SCS::syncReadPacketTx(u8 ID[], u8 IDN, u8 MemAddr, u8 nLen)
{
	rFlushSCS();
	syncReadRxPacketLen = nLen;
	u8 checkSum = (4+0xfe)+IDN+MemAddr+nLen+INST_SYNC_READ;
	u8 i;
	writeSCS(0xff);
	writeSCS(0xff);
	writeSCS(0xfe);
	writeSCS(IDN+4);
	writeSCS(INST_SYNC_READ);
	writeSCS(MemAddr);
	writeSCS(nLen);
	for(i=0; i<IDN; i++){
		writeSCS(ID[i]);
		checkSum += ID[i];
	}
	checkSum = ~checkSum;
	writeSCS(checkSum);
	wFlushSCS();
	
	syncReadRxBuffLen = readSCS(syncReadRxBuff, syncReadRxBuffMax);
	return syncReadRxBuffLen;
}

/**
 * @brief Initialize sync read buffer for multiple servo responses
 * 
 * Allocates buffer sized for expected number of servo responses.
 * Cleans up any existing buffer first to prevent memory leaks.
 * 
 * @param IDN Number of servos to read from
 * @param rxLen Number of data bytes expected per servo response
 */
void SCS::syncReadBegin(u8 IDN, u8 rxLen)
{
	// Clean up existing buffer to prevent memory leak
	if(syncReadRxBuff){
		delete[] syncReadRxBuff;
		syncReadRxBuff = NULL;
	}
	syncReadRxBuffMax = IDN*(rxLen+6);
	syncReadRxBuff = new u8[syncReadRxBuffMax];
	// Check allocation success
	if(!syncReadRxBuff){
		syncReadRxBuffMax = 0;
	}
}

/**
 * @brief Cleanup sync read buffer
 * 
 * Deallocates the sync read response buffer. Must be called after
 * completing sync read operations to free memory.
 */
void SCS::syncReadEnd()
{
	if(syncReadRxBuff){
		delete[] syncReadRxBuff;  // Correct: use delete[] for arrays
		syncReadRxBuff = NULL;
	}
}

/**
 * @brief Extract one servo's response from sync read buffer
 * 
 * Parses sync read response buffer to extract packet for specific servo ID.
 * Validates packet format, checksum, and copies data to output buffer.
 * 
 * @param ID Servo ID to extract response for
 * @param nDat Output buffer for response data (must be at least syncReadRxPacketLen bytes)
 * @return Number of data bytes extracted on success, 0 on error/not found
 */
int SCS::syncReadPacketRx(u8 ID, u8 *nDat)
{
	// NULL pointer checks
	if(!nDat || !syncReadRxBuff){
		return 0;
	}

	u16 syncReadRxBuffIndex = 0;
	syncReadRxPacket = nDat;
	syncReadRxPacketIndex = 0;
	while((syncReadRxBuffIndex+6+syncReadRxPacketLen)<=syncReadRxBuffLen){
		u8 bBuf[] = {0, 0, 0};
		u8 calSum = 0;
		while(syncReadRxBuffIndex<syncReadRxBuffLen){
			bBuf[0] = bBuf[1];
			bBuf[1] = bBuf[2];
			bBuf[2] = syncReadRxBuff[syncReadRxBuffIndex++];
			if(bBuf[0]==0xff && bBuf[1]==0xff && bBuf[2]!=0xff){
				break;
			}
		}
		if(bBuf[2]!=ID){
			continue;
		}
		// Bounds check before accessing buffer
		if(syncReadRxBuffIndex >= syncReadRxBuffLen){
			break;
		}
		if(syncReadRxBuff[syncReadRxBuffIndex++]!=(syncReadRxPacketLen+2)){
			continue;
		}
		// Bounds check before accessing buffer
		if(syncReadRxBuffIndex >= syncReadRxBuffLen){
			break;
		}
		Error = syncReadRxBuff[syncReadRxBuffIndex++];
		calSum = ID+(syncReadRxPacketLen+2)+Error;
		for(u8 i=0; i<syncReadRxPacketLen; i++){
			// Bounds check in loop
			if(syncReadRxBuffIndex >= syncReadRxBuffLen){
				return 0;
			}
			syncReadRxPacket[i] = syncReadRxBuff[syncReadRxBuffIndex++];
			calSum += syncReadRxPacket[i];
		}
		calSum = ~calSum;
		// Bounds check before final access
		if(syncReadRxBuffIndex >= syncReadRxBuffLen){
			return 0;
		}
		if(calSum!=syncReadRxBuff[syncReadRxBuffIndex++]){
			return 0;
		}
		return syncReadRxPacketLen;
	}
	return 0;
}

/**
 * @brief Read next byte from sync read response packet
 * 
 * Sequential access to parsed sync read packet data. Advances internal index.
 * 
 * @return Next byte from packet, or -1 if all bytes have been read
 */
int SCS::syncReadRxPacketToByte()
{
	if(syncReadRxPacketIndex>=syncReadRxPacketLen){
		return -1;
	}
	return syncReadRxPacket[syncReadRxPacketIndex++];
}

/**
 * @brief Read next 16-bit word from sync read response packet
 * 
 * Sequential access to parsed sync read packet data. Reads two bytes,
 * converts to host byte order, and optionally handles negative values.
 * 
 * @param negBit Bit position for sign extension (0 = unsigned, >0 = treat as signed with negBit as sign bit)
 * @return 16-bit word value (possibly sign-extended), or -1 if insufficient bytes remain
 */
int SCS::syncReadRxPacketToWrod(u8 negBit)
{
	if((syncReadRxPacketIndex+1)>=syncReadRxPacketLen){
		return -1;
	}
	int Word = SCS2Host(syncReadRxPacket[syncReadRxPacketIndex], syncReadRxPacket[syncReadRxPacketIndex+1]);
	syncReadRxPacketIndex += 2;
	if(negBit){
		if(Word&(1<<negBit)){
			Word = -(Word & ~(1<<negBit));
		}
	}
	return Word;
}
```

## Файл: `scservo_sdk/SCS.h`

```cpp
﻿/**
 * @file SCS.h
 * @brief Feetech serial servo communication protocol layer
 *
 * @details This file defines the base protocol layer for Feetech serial servo
 * communication. It provides the abstract interface for packet construction,
 * command execution, and response parsing.
 *
 * **Protocol Commands:**
 * - PING: Check servo connectivity
 * - READ/WRITE: Memory table access
 * - REG_WRITE/REG_ACTION: Asynchronous write operations
 * - SYNC_READ/SYNC_WRITE: Multi-servo synchronized operations
 *
 * **Key Responsibilities:**
 * - Packet formatting and checksum calculation
 * - Command encoding and response decoding
 * - Byte ordering (endianness) management
 * - Error detection and reporting
 * - Synchronized read buffer management
 *
 * **Inheritance Hierarchy:**
 * @code
 * SCS (abstract protocol layer)
 *  └── SCSerial (concrete Linux serial implementation)
 *       ├── SMS_STS (SMS/STS series application layer)
 *       ├── SCSCL (SCSCL series application layer)
 *       └── HLSCL (HLSCL series application layer)
 * @endcode
 *
 * @note This is an abstract base class - use SCSerial or its derivatives
 * @see SCSerial.h for concrete implementation
 * @see SMS_STS.h, SCSCL.h, HLSCL.h for application layers
 */

#ifndef _SCS_H
#define _SCS_H

#include "INST.h"

/**
 * @class SCS
 * @brief Abstract base class for Feetech serial servo communication protocol
 *
 * @details Implements the Feetech servo communication protocol, handling packet
 * construction, checksum validation, and command encoding/decoding. This class
 * defines the protocol layer and must be inherited by a concrete implementation
 * that provides actual serial I/O operations.
 *
 * **Protocol Packet Format:**
 * @code
 * [Header1] [Header2] [ID] [Length] [Instruction] [Parameters...] [Checksum]
 * 0xFF      0xFF      ID   Len      Inst          Param1 Param2   CS
 * @endcode
 *
 * **Virtual Methods (must be implemented by derived class):**
 * - writeSCS(): Write data to serial port
 * - readSCS(): Read data from serial port
 * - rFlushSCS(): Flush receive buffer
 * - wFlushSCS(): Flush transmit buffer
 *
 * **Public Members:**
 * - Level: Response level control
 * - End: Endianness flag (0=little-endian, 1=big-endian)
 * - Error: Last error status from servo
 *
 * @note Implements Rule of Five (copy operations deleted, virtual destructor)
 * @see SCSerial for Linux serial port implementation
 */
class SCS{
public:
	SCS();
	SCS(u8 End);
	SCS(u8 End, u8 Level);
	virtual ~SCS() {}  // Virtual destructor for proper cleanup in derived classes

	// Disable copying (Rule of Three/Five) - class owns dynamic memory (syncReadRxBuff)
	SCS(const SCS&) = delete;
	SCS& operator=(const SCS&) = delete;

	/**
	 * @brief Write data to servo memory (normal write command)
	 * @param ID Servo ID (0-253, 0xFE for broadcast)
	 * @param MemAddr Memory table address
	 * @param nDat Pointer to data buffer
	 * @param nLen Data length (bytes)
	 * @return 1 on success, 0 on failure
	 */
	int genWrite(u8 ID, u8 MemAddr, u8 *nDat, u8 nLen);

	/**
	 * @brief Asynchronous write command
	 * @note Requires RegWriteAction() to execute
	 */
	int regWrite(u8 ID, u8 MemAddr, u8 *nDat, u8 nLen);

	/**
	 * @brief Execute asynchronous write commands
	 * @param ID Servo ID (default: 0xFE for all servos)
	 * @return 1 on success, 0 on failure
	 */
	int RegWriteAction(u8 ID = 0xfe);

	/**
	 * @brief Synchronous write to multiple servos
	 * @param ID Array of servo IDs
	 * @param IDN Number of servos
	 * @param MemAddr Memory table start address
	 * @param nDat Data buffer (nLen bytes per servo)
	 * @param nLen Data length per servo
	 */
	void syncWrite(u8 ID[], u8 IDN, u8 MemAddr, u8 *nDat, u8 nLen);

	int writeByte(u8 ID, u8 MemAddr, u8 bDat);//写1个字节
	int writeWord(u8 ID, u8 MemAddr, u16 wDat);//写2个字节

	/**
	 * @brief Read data from servo memory
	 * @param ID Servo ID
	 * @param MemAddr Memory table address
	 * @param nData Buffer to store read data
	 * @param nLen Number of bytes to read (max 249)
	 * @return Number of bytes read on success, 0 on failure
	 */
	int Read(u8 ID, u8 MemAddr, u8 *nData, u8 nLen);

	int readByte(u8 ID, u8 MemAddr);//读1个字节
	int readWord(u8 ID, u8 MemAddr);//读2个字节

	/**
	 * @brief Ping servo to check connection
	 * @param ID Servo ID to ping
	 * @return Servo error status on success, -1 on failure
	 */
	int Ping(u8 ID);
	int syncReadPacketTx(u8 ID[], u8 IDN, u8 MemAddr, u8 nLen);//同步读指令包发送
	int syncReadPacketRx(u8 ID, u8 *nDat);//同步读返回包解码，成功返回内存字节数，失败返回0
	int syncReadRxPacketToByte();//解码一个字节
	int syncReadRxPacketToWrod(u8 negBit=0);//解码两个字节，negBit为方向为，negBit=0表示无方向
	void syncReadBegin(u8 IDN, u8 rxLen);//同步读开始
	void syncReadEnd();//同步读结束
public:
	u8	Level;//舵机返回等级
	u8	End;// Processor endianness structure
	u8	Error;// Servo status
	u8 syncReadRxPacketIndex;
	u8 syncReadRxPacketLen;
	u8 *syncReadRxPacket;
	u8 *syncReadRxBuff;
	u16 syncReadRxBuffLen;
	u16 syncReadRxBuffMax;
protected:
	virtual int writeSCS(unsigned char *nDat, int nLen) = 0;
	virtual int readSCS(unsigned char *nDat, int nLen) = 0;
	virtual int writeSCS(unsigned char bDat) = 0;
	virtual void rFlushSCS() = 0;
	virtual void wFlushSCS() = 0;
protected:
	void writeBuf(u8 ID, u8 MemAddr, u8 *nDat, u8 nLen, u8 Fun);
	void Host2SCS(u8 *DataL, u8* DataH, u16 Data);// Split one 16-bit value into two 8-bit values
	u16 SCS2Host(u8 DataL, u8 DataH);// Combine two 8-bit values into one 16-bit value
	int Ack(u8 ID);// Return response

	// Helper for reading signed words with direction bit
	int readSignedWord(int ID, u8 addr, u8 signBit) {
		int value = readWord(ID, addr);
		if(value == -1) {
			Error = 1;
			return -1;
		}
		if((value & (1 << signBit))) {
			value = -(value & ~(1 << signBit));
		}
		return value;
	}
};
#endif
```

## Файл: `scservo_sdk/SCS0009.cpp`

```cpp
﻿/**
 * @file SCS0009.cpp
 * @brief Implementation of SCS0009 Series Serial Servo Application Layer
 *
 * @details This file implements the control functions for Feetech SCS0009 series
 *  This code is quite similar to that used to control the SCSCL series. 
 *  Most of the relevant changes can be found in SCS0009.h which has the values
 *  and register memory locations used in the SCS0009 firmware vs. the SCSCL series. 
 */ 

#include "SCS0009.h"
#include "INST.h"
#include "SyncWriteBuffer.h"
#include <cstring>
#include <iostream>

/**
 * @brief Default constructor - initializes with little-endian byte order
 */
SCS0009::SCS0009() : SCSerial(1) {}

/**
 * @brief Constructor with endianness parameter
 * @param End Endianness flag (0=little-endian, 1=big-endian)
 */
SCS0009::SCS0009(u8 End) : SCSerial(End) {}

/**
 * @brief Constructor with endianness and response level
 * @param End Endianness flag (0=little-endian, 1=big-endian)
 * @param Level Response level (0=ping only, 1=read only, 2=all commands)
 */
SCS0009::SCS0009(u8 End, u8 Level) : SCSerial(End, Level) {}

int SCS0009::WritePos(u8 ID, u16 Position, u16 Time, u16 Speed)
{
	u8 bBuf[6];
	this->Host2SCS(bBuf+0, bBuf+1, Position);
	this->Host2SCS(bBuf+2, bBuf+3, Time);
	this->Host2SCS(bBuf+4, bBuf+5, Speed);
    
	return this->genWrite(ID, SCS0009_GOAL_POSITION_L, bBuf, 6);
}

int SCS0009::RegWritePos(u8 ID, u16 Position, u16 Time, u16 Speed)
{
	u8 bBuf[6];
	this->Host2SCS(bBuf+0, bBuf+1, Position);
	this->Host2SCS(bBuf+2, bBuf+3, Time);
	this->Host2SCS(bBuf+4, bBuf+5, Speed);
    
	return this->regWrite(ID, SCS0009_GOAL_POSITION_L, bBuf, 6);
}

void SCS0009::SyncWritePos(u8 ID[], u8 IDN, u16 Position[], u16 Time[], u16 Speed[])
{
	SyncWriteBuffer buffer(IDN, 6);
	if(!buffer.isValid()){
            this->Err = 1;
		return;
	}
	for(u8 i = 0; i<IDN; i++){
		u8 bBuf[6];
		u16 T, V;
		T = Time ? Time[i] : 0;
		V = Speed ? Speed[i] : 0;
		this->Host2SCS(bBuf+0, bBuf+1, Position[i]);
		this->Host2SCS(bBuf+2, bBuf+3, T);
		this->Host2SCS(bBuf+4, bBuf+5, V);
		buffer.writeMotorData(i, bBuf, 6);
	}
	this->syncWrite(ID, IDN, SCS0009_GOAL_POSITION_L, buffer.getBuffer(), 6);
}

/** @brief Set operating mode (SCS0009 uses angle limits for mode control) */
int SCS0009::Mode(u8 ID, u8 mode)
{
	// For SCS0009: mode 0 = position mode (set angle limits), mode 1 = PWM mode (clear angle limits)
	if (mode == 0) {
                u8 bBuf[4];
                this->Host2SCS(bBuf+0,bBuf+1,0);
                this->Host2SCS(bBuf+2,bBuf+3,1023);
                return this->genWrite(ID, SCS0009_MIN_ANGLE_LIMIT_L,bBuf,4);
        }
        else if (mode == 1) { 
               return PWMMode(ID);
	} else { 
                /* defensively refuse other values for mode-setting */
		return 0; 
	}
}

/**
 * @brief Initialize motor with mode and torque settings
 * @param ID Servo ID
 * @param mode Operating mode
 * @param enableTorque 1 to enable torque, 0 to disable
 * @return 1 on success, 0 on failure
 */
int SCS0009::InitMotor(u8 ID, u8 mode, u8 enableTorque)
{
	// Unlock EEPROM
	int ret = unLockEeprom(ID);
	if (ret == 0) {
		this->Err = 1;
		return 0;
	}

	// Set mode
	ret = Mode(ID, mode);
	if (ret == 0) {
                LockEeprom(ID);
		this->Err = 1;
		return 0;
	}

	// Lock EEPROM
	ret = LockEeprom(ID);
	if (ret == 0) {
		this->Err = 1;
		return 0;
	}

	// Enable/disable torque
	ret = EnableTorque(ID, enableTorque);
	if (ret == 0) {
		this->Err = 1;
		return 0;
	}

	this->Err = 0;
	return 1;
}

int SCS0009::PWMMode(u8 ID)
{
	u8 bBuf[4] = {0, 0, 0, 0};
	return this->genWrite(ID, SCS0009_MIN_ANGLE_LIMIT_L, bBuf, 4);
}

int SCS0009::WritePWM(u8 ID, s16 pwmOut)
{
	u16 encodedPwm = ServoUtils::encodeSignedValue(pwmOut, SCS0009_DIRECTION_BIT_POS);

	u8 bBuf[2];
	this->Host2SCS(bBuf+0, bBuf+1, encodedPwm);
	return this->genWrite(ID, SCS0009_GOAL_TIME_L, bBuf, 2);
}

/** @brief Enable/disable servo torque */
int SCS0009::EnableTorque(u8 ID, u8 Enable)
{
	return this->writeByte(ID, SCS0009_TORQUE_ENABLE, Enable);
}

/** @brief Unlock EEPROM */
int SCS0009::unLockEeprom(u8 ID)
{
	return this->writeByte(ID, SCS0009_LOCK, 0);
}

/** @brief Lock EEPROM */
int SCS0009::LockEeprom(u8 ID)
{
	return this->writeByte(ID, SCS0009_LOCK, 1);
}

/**
 * @brief Read all feedback data from servo
 * @param ID Servo ID
 * @return 1 on success, 0 on failure
 */
int SCS0009::FeedBack(u8 ID)
{
	int nLen = this->Read(ID, SCS0009_PRESENT_POSITION_L, Mem, sizeof(Mem));
	if(nLen!=sizeof(Mem)){
		this->Err = 1;
		return 0;
	}
	this->Err = 0;
	return 1;
}
	
/** @brief Read current position from cache (FeedBack must be called first) or directly from servo */
int SCS0009::ReadPos(u8 ID)
{
	int Pos = -1;
	if(ID==(u8)-1){
		Pos = Mem[SCS0009_PRESENT_POSITION_H-SCS0009_PRESENT_POSITION_L];
		Pos <<= 8;
		Pos |= Mem[SCS0009_PRESENT_POSITION_L-SCS0009_PRESENT_POSITION_L];
	}else{
		this->Err = 0;
		Pos = this->readWord(ID, SCS0009_PRESENT_POSITION_L);
		if(Pos==-1){
			this->Err = 1;
		}
	}
	return Pos;
}

/** @brief Read current speed */
int SCS0009::ReadSpeed(u8 ID)
{
	if(ID == (u8)-1) {
		return ServoUtils::readSignedWordFromBuffer(
			Mem,
			SCS0009_PRESENT_SPEED_L - SCS0009_PRESENT_POSITION_L,
			SCS0009_PRESENT_SPEED_H - SCS0009_PRESENT_POSITION_L,
			SCS0009_DIRECTION_BIT_POS
		);
	}
	this->Err = 0;
        int speed = this->readSignedWord(ID, SCS0009_PRESENT_SPEED_L, SCS0009_DIRECTION_BIT_POS);
	if (speed == -1) this->Err = 1;
	return speed;
}

/** @brief Read current load */
int SCS0009::ReadLoad(u8 ID)
{
	if(ID == (u8)-1) {
		return ServoUtils::readSignedWordFromBuffer(
			Mem,
			SCS0009_PRESENT_LOAD_L - SCS0009_PRESENT_POSITION_L,
			SCS0009_PRESENT_LOAD_H - SCS0009_PRESENT_POSITION_L,
			SCS0009_DIRECTION_BIT_POS
		);
	}
	this->Err = 0;
        int load = this->readSignedWord(ID, SCS0009_PRESENT_LOAD_L, 
                                            SCS0009_DIRECTION_BIT_POS
                         );
	if (load == -1) this->Err = 1;
	return load;
}

/** @brief Read supply voltage */
int SCS0009::ReadVoltage(u8 ID)
{
	int Voltage = -1;
	if(ID==(u8)-1){
		Voltage = Mem[SCS0009_PRESENT_VOLTAGE-SCS0009_PRESENT_POSITION_L];
	}else{
		this->Err = 0;
		Voltage = this->readByte(ID, SCS0009_PRESENT_VOLTAGE);
		if(Voltage==-1){
			this->Err = 1;
		}
	}
	return Voltage;
}

/** @brief Read internal temperature */
int SCS0009::ReadTemper(u8 ID)
{
	int Temper = -1;
	if(ID==(u8)-1){
		Temper = Mem[SCS0009_PRESENT_TEMPERATURE-SCS0009_PRESENT_POSITION_L];
	}else{
		this->Err = 0;
		Temper = this->readByte(ID, SCS0009_PRESENT_TEMPERATURE);
		if(Temper==-1){
			this->Err = 1;
		}
	}
	return Temper;
}

/** @brief Read movement status */
int SCS0009::ReadMove(u8 ID)
{
	int Move = -1;
	if(ID==(u8)-1){
		Move = Mem[SCS0009_MOVING-SCS0009_PRESENT_POSITION_L];
	}else{
		this->Err = 0;
		Move = this->readByte(ID, SCS0009_MOVING);
		if(Move==-1){
			this->Err = 1;
		}
	}
	return Move;
}

/** @brief Read motor current */
int SCS0009::ReadCurrent(u8 ID)
{
	if(ID == (u8)-1) {
		return ServoUtils::readSignedWordFromBuffer(
			Mem,
			SCS0009_PRESENT_CURRENT_L - SCS0009_PRESENT_POSITION_L,
			SCS0009_PRESENT_CURRENT_H - SCS0009_PRESENT_POSITION_L,
			SCS0009_DIRECTION_BIT_POS
		);
	}
	this->Err = 0;
        int current = this->readSignedWord(ID, SCS0009_PRESENT_CURRENT_L,
                                           SCS0009_DIRECTION_BIT_POS
                                      );
	if (current == -1) this->Err = 1;
	return current;
}
```

## Файл: `scservo_sdk/SCS0009.h`

```cpp
﻿/**
 * @file SCS0009.h
 * @brief Feetech SCS0009 Series Serial Servo Application Layer
 *
 * @details This file maps very closely to the SCSCL.h file modulo 
 * - a few details 
 */

#ifndef _SCS0009_H
#define _SCS0009_H
#include "INST.h"
#include "SCSerial.h"

// Baud rate definitions
#define	SCS0009_1M 0
#define	SCS0009_0_5M 1
#define	SCS0009_250K 2
#define	SCS0009_128K 3
#define	SCS0009_115200 4
#define	SCS0009_76800	5
#define	SCS0009_57600	6
#define	SCS0009_38400	7

// Memory table definitions
//-------EEPROM (Read-only)--------
#define SCS0009_VERSION_L 3
#define SCS0009_VERSION_H 4

//-------EEPROM (Read/Write)--------
#define SCS0009_ID 5
#define SCS0009_BAUD_RATE 6

// hang-time of how long the servo waits to respond to a host
// Might need to be tuned if you start seeing a lot of unacked status packets or retransmits
// at either end of the conversation
// These registers are present on the SCS0009 but not on some other Feetech servos 
#define SCS0009_RETURN_DELAY        7    // 0x07 — response delay in 2µs units
#define SCS0009_STATUS_RETURN_LEVEL 8    // 0x08 — 0=ping only, 1=read, 2=all

#define SCS0009_MIN_ANGLE_LIMIT_L 9
#define SCS0009_MIN_ANGLE_LIMIT_H 10
#define SCS0009_MAX_ANGLE_LIMIT_L 11
#define SCS0009_MAX_ANGLE_LIMIT_H 12

#define SCS0009_CW_DEAD 26
#define SCS0009_CCW_DEAD 27

//-------SRAM (Read/Write)--------
#define SCS0009_TORQUE_ENABLE 40
#define SCS0009_ACC 41
#define SCS0009_GOAL_POSITION_L 42
#define SCS0009_GOAL_POSITION_H 43
#define SCS0009_GOAL_TIME_L 44
#define SCS0009_GOAL_TIME_H 45
#define SCS0009_GOAL_SPEED_L 46
#define SCS0009_GOAL_SPEED_H 47
#define SCS0009_LOCK 48

//-------SRAM (Read-only)--------
#define SCS0009_PRESENT_POSITION_L 56
#define SCS0009_PRESENT_POSITION_H 57
#define SCS0009_PRESENT_SPEED_L 58
#define SCS0009_PRESENT_SPEED_H 59
#define SCS0009_PRESENT_LOAD_L 60
#define SCS0009_PRESENT_LOAD_H 61
#define SCS0009_PRESENT_VOLTAGE 62
#define SCS0009_PRESENT_TEMPERATURE 63
#define SCS0009_MOVING 66
#define SCS0009_PRESENT_CURRENT_L 69
#define SCS0009_PRESENT_CURRENT_H 70

// Direction bit positions

#define SCS0009_DIRECTION_BIT_POS 15

// hang-time of how long the servo waits to respond to a host
// Might need to be tuned if you start seeing a lot of unacked status packets or retransmits
// at either end of the conversation

/**
 * @class SCS0009
 * @brief Application layer interface for SCS0009 series serial servos
 *
 * **Inheritance:**
 * Inherits from SCSerial for serial communication and SCS protocol handling
 *
 * @see SCSCL and others for similar servo series interface
 * @see SCSerial for communication layer
 */
class SCS0009 : public SCSerial
{
public:
	SCS0009();
	SCS0009(u8 End);
	SCS0009(u8 End, u8 Level);


	virtual int WritePos(u8 ID, u16 Position, u16 Time, u16 Speed = 0);
	virtual int RegWritePos(u8 ID, u16 Position, u16 Time, u16 Speed = 0);
	virtual void SyncWritePos(u8 ID[], u8 IDN, u16 Position[], u16 Time[], u16 Speed[]);
	virtual int Mode(u8 ID, u8 mode); // Set operating mode (implementation via angle limits for SCS0009)
	virtual int InitMotor(u8 ID, u8 mode, u8 enableTorque = 1); // Initialize motor with mode and torque (unlocks EEPROM, sets mode, locks EEPROM, enables/disables torque)
	virtual int PWMMode(u8 ID);
	virtual int WritePWM(u8 ID, s16 pwmOut);
	virtual int EnableTorque(u8 ID, u8 Enable);
	virtual int unLockEeprom(u8 ID);
	virtual int LockEeprom(u8 ID);
	virtual int FeedBack(u8 ID);
	virtual int ReadPos(u8 ID);
	virtual int ReadSpeed(u8 ID);
	virtual int ReadLoad(u8 ID);
	virtual int ReadVoltage(u8 ID);
	virtual int ReadTemper(u8 ID);
	virtual int ReadMove(u8 ID);
	virtual int ReadCurrent(u8 ID);
private:
	u8 Mem[SCS0009_PRESENT_CURRENT_H-SCS0009_PRESENT_POSITION_L+1];
};

#endif
```

## Файл: `scservo_sdk/SCSCL.cpp`

```cpp
﻿/**
 * @file SCSCL.cpp
 * @brief Implementation of SCSCL Series Serial Servo Application Layer
 *
 * @details This file implements the control functions for Feetech SCSCL series
 * serial bus servo motors. Provides position control, PWM output, and comprehensive
 * feedback reading capabilities.
 *
 * **Implemented Features:**
 * - Position control with time and speed parameters
 * - Asynchronous (RegWrite) and synchronous (SyncWrite) operations
 * - PWM output control for open-loop operation
 * - Complete servo status feedback
 * - EEPROM lock/unlock for parameter persistence
 * - Mode configuration via angle limits
 *
 * @see SCSCL.h for class interface documentation
 * @see SMS_STS.cpp for reference implementation style
 */

#include "SCSCL.h"
#include "INST.h"
#include "SyncWriteBuffer.h"
#include <cstring>

/**
 * @brief Default constructor - initializes with little-endian byte order
 */
SCSCL::SCSCL() : SCSerial(1) {}

/**
 * @brief Constructor with endianness parameter
 * @param End Endianness flag (0=little-endian, 1=big-endian)
 */
SCSCL::SCSCL(u8 End) : SCSerial(End) {}

/**
 * @brief Constructor with endianness and response level
 * @param End Endianness flag (0=little-endian, 1=big-endian)
 * @param Level Response level (0=ping only, 1=read only, 2=all commands)
 */
SCSCL::SCSCL(u8 End, u8 Level) : SCSerial(End, Level) {}

int SCSCL::WritePos(u8 ID, u16 Position, u16 Time, u16 Speed)
{
	u8 bBuf[6];
	this->Host2SCS(bBuf+0, bBuf+1, Position);
	this->Host2SCS(bBuf+2, bBuf+3, Time);
	this->Host2SCS(bBuf+4, bBuf+5, Speed);
    
	return this->genWrite(ID, SCSCL_GOAL_POSITION_L, bBuf, 6);
}

int SCSCL::RegWritePos(u8 ID, u16 Position, u16 Time, u16 Speed)
{
	u8 bBuf[6];
	this->Host2SCS(bBuf+0, bBuf+1, Position);
	this->Host2SCS(bBuf+2, bBuf+3, Time);
	this->Host2SCS(bBuf+4, bBuf+5, Speed);
    
	return this->regWrite(ID, SCSCL_GOAL_POSITION_L, bBuf, 6);
}

void SCSCL::SyncWritePos(u8 ID[], u8 IDN, u16 Position[], u16 Time[], u16 Speed[])
{
	SyncWriteBuffer buffer(IDN, 6);
	if(!buffer.isValid()){
		return;
	}
	for(u8 i = 0; i<IDN; i++){
		u8 bBuf[6];
		u16 T, V;
		T = Time ? Time[i] : 0;
		V = Speed ? Speed[i] : 0;
		this->Host2SCS(bBuf+0, bBuf+1, Position[i]);
		this->Host2SCS(bBuf+2, bBuf+3, T);
		this->Host2SCS(bBuf+4, bBuf+5, V);
		buffer.writeMotorData(i, bBuf, 6);
	}
	this->syncWrite(ID, IDN, SCSCL_GOAL_POSITION_L, buffer.getBuffer(), 6);
}

/** @brief Set operating mode (SCSCL uses angle limits for mode control) */
int SCSCL::Mode(u8 ID, u8 mode)
{
	// For SCSCL: mode 0 = position mode (set angle limits), mode 1 = PWM mode (clear angle limits)
	if (mode == 0) {
		// Position mode - would need to set appropriate angle limits
		// For now, just return success as angle limits should be set separately
		return 1;
	} else {
		// PWM mode - set angle limits to 0
		return PWMMode(ID);
	}
}

/**
 * @brief Initialize motor with mode and torque settings
 * @param ID Servo ID
 * @param mode Operating mode
 * @param enableTorque 1 to enable torque, 0 to disable
 * @return 1 on success, 0 on failure
 */
int SCSCL::InitMotor(u8 ID, u8 mode, u8 enableTorque)
{
	// Unlock EEPROM
	int ret = unLockEeprom(ID);
	if (ret == 0) {
		this->Err = 1;
		return 0;
	}

	// Set mode
	ret = Mode(ID, mode);
	if (ret == 0) {
		this->Err = 1;
		return 0;
	}

	// Lock EEPROM
	ret = LockEeprom(ID);
	if (ret == 0) {
		this->Err = 1;
		return 0;
	}

	// Enable/disable torque
	ret = EnableTorque(ID, enableTorque);
	if (ret == 0) {
		this->Err = 1;
		return 0;
	}

	this->Err = 0;
	return 1;
}

int SCSCL::PWMMode(u8 ID)
{
	u8 bBuf[4] = {0, 0, 0, 0};
	return this->genWrite(ID, SCSCL_MIN_ANGLE_LIMIT_L, bBuf, 4);
}

int SCSCL::WritePWM(u8 ID, s16 pwmOut)
{
	u16 encodedPwm = ServoUtils::encodeSignedValue(pwmOut, SCSCL_PWM_DIRECTION_BIT_POS);

	u8 bBuf[2];
	this->Host2SCS(bBuf+0, bBuf+1, encodedPwm);
	return this->genWrite(ID, SCSCL_GOAL_TIME_L, bBuf, 2);
}

/** @brief Enable/disable servo torque */
int SCSCL::EnableTorque(u8 ID, u8 Enable)
{
	return this->writeByte(ID, SCSCL_TORQUE_ENABLE, Enable);
}

/** @brief Unlock EEPROM */
int SCSCL::unLockEeprom(u8 ID)
{
	return this->writeByte(ID, SCSCL_LOCK, 0);
}

/** @brief Lock EEPROM */
int SCSCL::LockEeprom(u8 ID)
{
	return this->writeByte(ID, SCSCL_LOCK, 1);
}

/**
 * @brief Read all feedback data from servo
 * @param ID Servo ID
 * @return 1 on success, 0 on failure
 */
int SCSCL::FeedBack(u8 ID)
{
	int nLen = this->Read(ID, SCSCL_PRESENT_POSITION_L, Mem, sizeof(Mem));
	if(nLen!=sizeof(Mem)){
		this->Err = 1;
		return 0;
	}
	this->Err = 0;
	return 1;
}
	
/** @brief Read current position */
int SCSCL::ReadPos(u8 ID)
{
	int Pos = -1;
	if(ID==(u8)-1){
		Pos = Mem[SCSCL_PRESENT_POSITION_L-SCSCL_PRESENT_POSITION_L];
		Pos <<= 8;
		Pos |= Mem[SCSCL_PRESENT_POSITION_H-SCSCL_PRESENT_POSITION_L];
	}else{
		this->Err = 0;
		Pos = this->readWord(ID, SCSCL_PRESENT_POSITION_L);
		if(Pos==-1){
			this->Err = 1;
		}
	}
	return Pos;
}

/** @brief Read current speed */
int SCSCL::ReadSpeed(u8 ID)
{
	if(ID == (u8)-1) {
		return ServoUtils::readSignedWordFromBuffer(
			Mem,
			SCSCL_PRESENT_SPEED_L - SCSCL_PRESENT_POSITION_L,
			SCSCL_PRESENT_SPEED_H - SCSCL_PRESENT_POSITION_L,
			SCSCL_DIRECTION_BIT_POS
		);
	}
	this->Err = 0;
	return this->readSignedWord(ID, SCSCL_PRESENT_SPEED_L, SCSCL_DIRECTION_BIT_POS);
}

/** @brief Read current load */
int SCSCL::ReadLoad(u8 ID)
{
	if(ID == (u8)-1) {
		return ServoUtils::readSignedWordFromBuffer(
			Mem,
			SCSCL_PRESENT_LOAD_L - SCSCL_PRESENT_POSITION_L,
			SCSCL_PRESENT_LOAD_H - SCSCL_PRESENT_POSITION_L,
			SCSCL_LOAD_DIRECTION_BIT_POS
		);
	}
	this->Err = 0;
	return this->readSignedWord(ID, SCSCL_PRESENT_LOAD_L, SCSCL_LOAD_DIRECTION_BIT_POS);
}

/** @brief Read supply voltage */
int SCSCL::ReadVoltage(u8 ID)
{
	int Voltage = -1;
	if(ID==(u8)-1){
		Voltage = Mem[SCSCL_PRESENT_VOLTAGE-SCSCL_PRESENT_POSITION_L];
	}else{
		this->Err = 0;
		Voltage = this->readByte(ID, SCSCL_PRESENT_VOLTAGE);
		if(Voltage==-1){
			this->Err = 1;
		}
	}
	return Voltage;
}

/** @brief Read internal temperature */
int SCSCL::ReadTemper(u8 ID)
{
	int Temper = -1;
	if(ID==(u8)-1){
		Temper = Mem[SCSCL_PRESENT_TEMPERATURE-SCSCL_PRESENT_POSITION_L];
	}else{
		this->Err = 0;
		Temper = this->readByte(ID, SCSCL_PRESENT_TEMPERATURE);
		if(Temper==-1){
			this->Err = 1;
		}
	}
	return Temper;
}

/** @brief Read movement status */
int SCSCL::ReadMove(u8 ID)
{
	int Move = -1;
	if(ID==(u8)-1){
		Move = Mem[SCSCL_MOVING-SCSCL_PRESENT_POSITION_L];
	}else{
		this->Err = 0;
		Move = this->readByte(ID, SCSCL_MOVING);
		if(Move==-1){
			this->Err = 1;
		}
	}
	return Move;
}

/** @brief Read motor current */
int SCSCL::ReadCurrent(u8 ID)
{
	if(ID == (u8)-1) {
		return ServoUtils::readSignedWordFromBuffer(
			Mem,
			SCSCL_PRESENT_CURRENT_L - SCSCL_PRESENT_POSITION_L,
			SCSCL_PRESENT_CURRENT_H - SCSCL_PRESENT_POSITION_L,
			SCSCL_DIRECTION_BIT_POS
		);
	}
	this->Err = 0;
	return this->readSignedWord(ID, SCSCL_PRESENT_CURRENT_L, SCSCL_DIRECTION_BIT_POS);
}
```

## Файл: `scservo_sdk/SCSCL.h`

```cpp
﻿/**
 * @file SCSCL.h
 * @brief Feetech SCSCL Series Serial Servo Application Layer
 *
 * @details This file provides the application programming interface for
 * controlling Feetech SCSCL series serial bus servo motors.
 * Supports two primary operating modes:
 * - Servo mode: Position control with time and speed parameters
 * - PWM mode: Direct PWM output control (open-loop)
 *
 * **Key Features:**
 * - Position control with time and speed settings
 * - PWM output control for open-loop operation
 * - Feedback reading (position, speed, load, voltage, temperature, current)
 * - EEPROM lock/unlock for parameter persistence
 * - Mode switching via angle limit configuration
 *
 * **Memory Map:**
 * - EEPROM (Read-only): Model number, version
 * - EEPROM (Read/Write): ID, baud rate, angle limits, dead zones
 * - SRAM (Read/Write): Torque enable, goal position/time/speed, EEPROM lock
 * - SRAM (Read-only): Present position, speed, load, voltage, temperature, current, moving status
 *
 * @note SCSCL series uses angle limit configuration for mode switching
 * @see SMS_STS.h for reference implementation
 */

#ifndef _SCSCL_H
#define _SCSCL_H
#include "INST.h"
#include "SCSerial.h"
#include "ServoErrors.h"
#include "ServoUtils.h"

// Baud rate definitions
#define	SCSCL_1M 0
#define	SCSCL_0_5M 1
#define	SCSCL_250K 2
#define	SCSCL_128K 3
#define	SCSCL_115200 4
#define	SCSCL_76800	5
#define	SCSCL_57600	6
#define	SCSCL_38400	7

// Memory table definitions
//-------EEPROM (Read-only)--------
#define SCSCL_VERSION_L 3
#define SCSCL_VERSION_H 4

//-------EEPROM (Read/Write)--------
#define SCSCL_ID 5
#define SCSCL_BAUD_RATE 6
#define SCSCL_MIN_ANGLE_LIMIT_L 9
#define SCSCL_MIN_ANGLE_LIMIT_H 10
#define SCSCL_MAX_ANGLE_LIMIT_L 11
#define SCSCL_MAX_ANGLE_LIMIT_H 12
#define SCSCL_CW_DEAD 26
#define SCSCL_CCW_DEAD 27

//-------SRAM (Read/Write)--------
#define SCSCL_TORQUE_ENABLE 40
#define SCSCL_GOAL_POSITION_L 42
#define SCSCL_GOAL_POSITION_H 43
#define SCSCL_GOAL_TIME_L 44
#define SCSCL_GOAL_TIME_H 45
#define SCSCL_GOAL_SPEED_L 46
#define SCSCL_GOAL_SPEED_H 47
#define SCSCL_LOCK 48

//-------SRAM (Read-only)--------
#define SCSCL_PRESENT_POSITION_L 56
#define SCSCL_PRESENT_POSITION_H 57
#define SCSCL_PRESENT_SPEED_L 58
#define SCSCL_PRESENT_SPEED_H 59
#define SCSCL_PRESENT_LOAD_L 60
#define SCSCL_PRESENT_LOAD_H 61
#define SCSCL_PRESENT_VOLTAGE 62
#define SCSCL_PRESENT_TEMPERATURE 63
#define SCSCL_MOVING 66
#define SCSCL_PRESENT_CURRENT_L 69
#define SCSCL_PRESENT_CURRENT_H 70

// Direction bit positions
#define SCSCL_DIRECTION_BIT_POS 15
#define SCSCL_LOAD_DIRECTION_BIT_POS 10
// SCSCL uses bit 10 for PWM direction in open-loop mode
#define SCSCL_PWM_DIRECTION_BIT_POS 10

// Missing register definitions (needed for full compatibility)
#define SCSCL_ACC 41
#define SCSCL_MODE 35
#define SCSCL_OFS_L 33
#define SCSCL_OFS_H 34

/**
 * @class SCSCL
 * @brief Application layer interface for SCSCL series serial servos
 *
 * @details Provides high-level control functions for Feetech SCSCL series
 * servo motors. Supports position control and PWM output modes.
 *
 * **Core Functions:**
 * - WritePos: Write position with time and speed
 * - RegWritePos: Asynchronous position write
 * - SyncWritePos: Synchronized multi-servo position control
 * - WritePWM: Direct PWM output control
 * - Mode/InitMotor: Operating mode configuration
 * - FeedBack: Read all servo status
 * - Read* methods: Individual parameter reading
 *
 * **Inheritance:**
 * Inherits from SCSerial for serial communication and SCS protocol handling
 *
 * @see SMS_STS for similar servo series interface
 * @see SCSerial for communication layer
 */
class SCSCL : public SCSerial
{
public:
	SCSCL();
	SCSCL(u8 End);
	SCSCL(u8 End, u8 Level);


	virtual int WritePos(u8 ID, u16 Position, u16 Time, u16 Speed = 0);
	virtual int RegWritePos(u8 ID, u16 Position, u16 Time, u16 Speed = 0);
	virtual void SyncWritePos(u8 ID[], u8 IDN, u16 Position[], u16 Time[], u16 Speed[]);
	virtual int Mode(u8 ID, u8 mode); // Set operating mode (implementation via angle limits for SCSCL)
	virtual int InitMotor(u8 ID, u8 mode, u8 enableTorque = 1); // Initialize motor with mode and torque (unlocks EEPROM, sets mode, locks EEPROM, enables/disables torque)
	virtual int PWMMode(u8 ID);
	virtual int WritePWM(u8 ID, s16 pwmOut);
	virtual int EnableTorque(u8 ID, u8 Enable);
	virtual int unLockEeprom(u8 ID);
	virtual int LockEeprom(u8 ID);
	virtual int FeedBack(u8 ID);
	virtual int ReadPos(u8 ID);
	virtual int ReadSpeed(u8 ID);
	virtual int ReadLoad(u8 ID);
	virtual int ReadVoltage(u8 ID);
	virtual int ReadTemper(u8 ID);
	virtual int ReadMove(u8 ID);
	virtual int ReadCurrent(u8 ID);
private:
	u8 Mem[SCSCL_PRESENT_CURRENT_H-SCSCL_PRESENT_POSITION_L+1];
};

#endif
```

## Файл: `scservo_sdk/SCSerial.cpp`

```cpp
/**
 * @file SCSerial.cpp
 * @brief Feetech serial servo hardware interface layer implementation
 *
 * @details This file implements POSIX serial port communication for Feetech
 * servo motors on Linux platforms. It provides the hardware abstraction layer
 * between the protocol layer (SCS) and the actual serial port device.
 *
 * **Key Responsibilities:**
 * - Serial port initialization and configuration (termios)
 * - Baud rate configuration (38400 to 1M baud)
 * - Raw data transmission and reception
 * - Timeout handling using select()
 * - Buffer flushing and flow control
 * - Resource cleanup (file descriptor management)
 *
 * **Platform Support:**
 * - Linux (POSIX termios API)
 * - Supports USB-to-serial adapters (/dev/ttyUSB*, /dev/ttyACM*)
 *
 * @note Uses POSIX termios for serial port control
 * @see SCSerial.h for class interface documentation
 */

#include "SCSerial.h"

// macOS compatibility: Define missing baud rate constants
#ifdef __APPLE__
#ifndef B500000
#define B500000 500000
#endif
#ifndef B1000000
#define B1000000 1000000
#endif
#endif

/**
 * @brief Default constructor
 * 
 * Initializes serial port with:
 * - IOTimeOut = 100ms
 * - fd = -1 (not open)
 * - txBufLen = 0 (empty buffer)
 */
SCSerial::SCSerial()
{
	IOTimeOut = 100;
	fd = -1;
	txBufLen = 0;
}

/**
 * @brief Constructor with endianness parameter
 * 
 * @param End Endianness flag (0=little-endian, 1=big-endian)
 */
SCSerial::SCSerial(u8 End):SCS(End)
{
	IOTimeOut = 100;
	fd = -1;
	txBufLen = 0;
}

/**
 * @brief Constructor with endianness and response level
 * 
 * @param End Endianness flag
 * @param Level Response level
 */
SCSerial::SCSerial(u8 End, u8 Level):SCS(End, Level)
{
	IOTimeOut = 100;
	fd = -1;
	txBufLen = 0;
}

/**
 * @brief Initialize and open serial port
 * 
 * Opens serial port with specified baud rate and configures it for
 * 8N1 (8 data bits, no parity, 1 stop bit) communication in raw mode.
 * 
 * @param baudRate Baud rate (e.g., 1000000 for 1Mbps)
 * @param serialPort Device path (e.g., "/dev/ttyUSB0")
 * @return true on success, false on failure
 */
bool SCSerial::begin(int baudRate, const char* serialPort)
{
	if(fd != -1){
		close(fd);
		fd = -1;
	}
	//printf("servo port:%s\n", serialPort);
    if(serialPort == NULL)
		return false;
    fd = open(serialPort, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if(fd == -1){
		perror("open:");
        return false;
	}
    fcntl(fd, F_SETFL, FNDELAY);
    tcgetattr(fd, &orgopt);
    tcgetattr(fd, &curopt);
    speed_t CR_BAUDRATE;
    switch(baudRate){
    case 9600:
        CR_BAUDRATE = B9600;
        break;
    case 19200:
        CR_BAUDRATE = B19200;
        break;
    case 38400:
        CR_BAUDRATE = B38400;
        break;
    case 57600:
        CR_BAUDRATE = B57600;
        break;
    case 115200:
        CR_BAUDRATE = B115200;
        break;
    case 500000:
        CR_BAUDRATE = B500000;
        break;
    case 1000000:
        CR_BAUDRATE = B1000000;
        break;
    default:
        CR_BAUDRATE = B115200;
        break;
    }
    cfsetispeed(&curopt, CR_BAUDRATE);
    cfsetospeed(&curopt, CR_BAUDRATE);

	printf("serial speed %d\n", baudRate);
    //Mostly 8N1
    curopt.c_cflag &= ~PARENB;
    curopt.c_cflag &= ~CSTOPB;
    curopt.c_cflag &= ~CSIZE;
    curopt.c_cflag |= CS8;
    curopt.c_cflag |= CREAD;
    curopt.c_cflag |= CLOCAL;//disable modem statuc check
    cfmakeraw(&curopt);//make raw mode
    curopt.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    if(tcsetattr(fd, TCSANOW, &curopt) == 0){
        return true;
    }else{
		perror("tcsetattr:");
		return false;
	}
}

/**
 * @brief Change serial port baud rate
 * 
 * @param baudRate New baud rate
 * @return 1 on success, -1 if port not open
 */
int SCSerial::setBaudRate(int baudRate)
{ 
    if(fd==-1){
		return -1;
	}
    tcgetattr(fd, &orgopt);
    tcgetattr(fd, &curopt);
    speed_t CR_BAUDRATE = baudRate;
    cfsetispeed(&curopt, CR_BAUDRATE);
    cfsetospeed(&curopt, CR_BAUDRATE);
    return 1;
}

int SCSerial::readSCS(unsigned char *nDat, int nLen)
{
    int fs_sel;
    fd_set fs_read;
	int rvLen = 0;

	// Use select() to implement multi-channel serial communication
	while(1){
		// Reinitialize timeout for each select() call
		// select() modifies the timeout structure on Linux
		struct timeval time;
		time.tv_sec = 0;
		time.tv_usec = IOTimeOut*1000;

		FD_ZERO(&fs_read);
		FD_SET(fd,&fs_read);

		fs_sel = select(fd+1, &fs_read, NULL, NULL, &time);
		if(fs_sel){
			rvLen += read(fd, nDat+rvLen, nLen-rvLen);
			//printf("nLen = %d rvLen = %d\n", nLen, rvLen);
			if(rvLen<nLen){
				continue;
			}else{
				return rvLen;
			}
		}else{
			//printf("serial read fd read return 0\n");
			return rvLen;
		}
	}
}

/**
 * @brief Write data buffer to transmit buffer
 * 
 * Copies data to internal transmit buffer. Data is sent when wFlushSCS() is called.
 * Includes NULL pointer and buffer overflow protection.
 * 
 * @param nDat Pointer to data to write
 * @param nLen Number of bytes to write
 * @return Current buffer length on success, -1 on error
 */
int SCSerial::writeSCS(unsigned char *nDat, int nLen)
{
	// NULL pointer check
	if(!nDat){
		return -1;
	}

	// Buffer overflow protection
	if(txBufLen + nLen > SCSERVO_BUFFER_SIZE){
		return -1;
	}
	while(nLen--){
		txBuf[txBufLen++] = *nDat++;
	}
	return txBufLen;
}

/**
 * @brief Write single byte to transmit buffer
 * 
 * @param bDat Byte to write
 * @return Current buffer length on success, -1 if buffer full
 */
int SCSerial::writeSCS(unsigned char bDat)
{
	// Buffer overflow protection
	if(txBufLen >= SCSERVO_BUFFER_SIZE){
		return -1;
	}
	txBuf[txBufLen++] = bDat;
	return txBufLen;
}

/**
 * @brief Flush receive buffer
 * 
 * Discards any unread data in the serial port receive buffer.
 */
void SCSerial::rFlushSCS()
{
	tcflush(fd, TCIFLUSH);
}

/**
 * @brief Flush transmit buffer
 * 
 * Sends all buffered data from txBuf to the serial port.
 */
void SCSerial::wFlushSCS()
{
	if(txBufLen){
		ssize_t written = write(fd, txBuf, txBufLen);
		// Note: write errors are not critical for this protocol, servo will timeout
		// In production code, consider checking: if(written < 0) { handle error }
		(void)written;  // Suppress unused variable warning
		txBufLen = 0;
	}
}

/**
 * @brief Close serial port and cleanup
 * 
 * Closes the serial port file descriptor if open.
 */
void SCSerial::end() noexcept
{
	if(fd != -1){
		close(fd);
		fd = -1;
	}
}
```

## Файл: `scservo_sdk/SCSerial.h`

```cpp
/**
 * @file SCSerial.h
 * @brief Feetech serial servo hardware interface layer
 *
 * @details This file provides the hardware interface layer for serial communication
 * with Feetech servo motors on Linux platforms. It handles POSIX serial port operations,
 * baud rate configuration, and low-level data transmission/reception.
 *
 * **Key Features:**
 * - POSIX serial port communication (termios)
 * - Configurable baud rates (38400 to 1M)
 * - Timeout handling for robust communication
 * - Resource management (file descriptor ownership)
 * - Buffer management for transmit operations
 *
 * **Inherits From:**
 * - SCS: Protocol layer for command encoding/decoding
 *
 * **Derived Classes:**
 * - SMS_STS: SMS/STS series servo control
 * - SCSCL: SCSCL series servo control
 * - HLSCL: HLSCL series servo control
 *
 * **Usage Example:**
 * @code
 * SCSerial serial;
 * if (!serial.begin(1000000, "/dev/ttyUSB0")) {
 *     printf("Failed to open serial port\n");
 *     return -1;
 * }
 * // Use serial communication methods
 * serial.end();  // Clean up
 * @endcode
 *
 * @note This class owns the file descriptor and implements RAII cleanup
 * @see SCS for protocol-level operations
 */

#ifndef _SCSERIAL_H
#define _SCSERIAL_H

#include "SCS.h"
#include <stdio.h>
#include <termios.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/select.h>

class SCSerial : public SCS
{
public:
	SCSerial();
	SCSerial(u8 End);
	SCSerial(u8 End, u8 Level);

	// Disable copying (Rule of Three/Five) - class owns file descriptor resource
	SCSerial(const SCSerial&) = delete;
	SCSerial& operator=(const SCSerial&) = delete;

protected:
	int writeSCS(unsigned char *nDat, int nLen);// Output nLen bytes
	int readSCS(unsigned char *nDat, int nLen);// Input nLen bytes
	int writeSCS(unsigned char bDat);// Output 1 byte
	void rFlushSCS();//
	void wFlushSCS();//
public:
	unsigned long int IOTimeOut;// Input/output timeout
	int Err;
public:
	virtual int getErr(){  return Err;  }
	virtual int setBaudRate(int baudRate);
	virtual bool begin(int baudRate, const char* serialPort);
	virtual void end() noexcept;
protected:
    int fd;//serial port handle
    struct termios orgopt;//fd ort opt
	struct termios curopt;//fd cur opt
	unsigned char txBuf[SCSERVO_BUFFER_SIZE];
	int txBufLen;
};

#endif
```

## Файл: `scservo_sdk/SCServo.h`

```cpp
/**
 * @file SCServo.h
 * @brief Master include file for Feetech Serial Servo SDK
 *
 * @details This is the main header file that includes all servo series interfaces.
 * Include this single file to access all Feetech servo series classes.
 *
 * **Supported Servo Series:**
 * - SMS_STS: SMS and STS series (3 operating modes: servo, wheel closed-loop, wheel open-loop)
 * - SCSCL: SCSCL series (position control and PWM mode)
 * - HLSCL: HLS series (servo, wheel, and force control modes)
 * - SCS0009: Servos used by the AmazingHand v1 servos 
 *
 * **Usage:**
 * @code
 * #include "SCServo.h"
 *
 * SMS_STS servo;
 * servo.begin(1000000, "/dev/ttyUSB0");
 * servo.InitMotor(1, 0, 1);  // ID 1, servo mode, enable torque
 * servo.WritePosEx(1, 2048, 1000, 50);  // Move to position 2048
 * @endcode
 *
 * @note This file only includes headers; link against libSCServo.a for implementations
 * @see SMS_STS.h, SCSCL.h, SCS0009.h HLSCL.h for protocol documentation
 */

#ifndef _SCSERVO_H
#define _SCSERVO_H

#include "SCSCL.h"
#include "SMS_STS.h"
#include "HLSCL.h"
#include "SCS0009.h"
/* additional support for SCS0009 from Amazing Hand */
#endif
```

## Файл: `scservo_sdk/SMS_STS.cpp`

```cpp
﻿/**
 * @file SMS_STS.cpp
 * @brief Feetech SMS/STS series serial servo application layer implementation
 *
 * @details This file implements high-level control functions for Feetech SMS
 * and STS series servo motors. It provides three operating modes with complete
 * read/write functionality and LSP-compliant initialization.
 *
 * **Implemented Features:**
 * - Mode 0: Position control with speed and acceleration
 * - Mode 1: Velocity control (closed-loop wheel mode)
 * - Mode 2: PWM control (open-loop wheel mode)
 * - Synchronized writes for multi-motor coordination
 * - Asynchronous writes with RegWriteAction
 * - Comprehensive feedback reading (position, speed, load, voltage, temp, current)
 * - EEPROM configuration management
 *
 * **Refactoring Improvements:**
 * - Uses ServoUtils for direction bit encoding/decoding (DRY principle)
 * - Uses SyncWriteBuffer for automatic memory management (RAII)
 * - Standardized error handling with ServoErrors
 *
 * @note All sync write operations use RAII-based buffer management
 * @see SMS_STS.h for class interface and usage examples
 */

#include "INST.h"
#include "SMS_STS.h"
#include "SyncWriteBuffer.h"

/**
 * @brief Default constructor
 * 
 * Initializes SMS/STS servo controller with default End byte (0).
 */
SMS_STS::SMS_STS()
{
	End = 0;
}

/**
 * @brief Constructor with custom End byte
 * 
 * @param End Protocol end byte (0 or 1)
 */
SMS_STS::SMS_STS(u8 End):SCSerial(End)
{
}

/**
 * @brief Constructor with End byte and response level
 * 
 * @param End Protocol end byte (0 or 1)
 * @param Level Response level (0=no response, 1=response for read/write commands)
 */
SMS_STS::SMS_STS(u8 End, u8 Level):SCSerial(End, Level)
{
}

/**
 * @brief Write position, speed, and acceleration to servo (extended command)
 * 
 * Sends single-servo position command with acceleration and speed control.
 * Negative positions are converted to absolute value with direction bit set.
 * 
 * @param ID Servo ID
 * @param Position Target position (0-4095 steps)
 * @param Speed Moving speed (0-3400 steps/s)
 * @param ACC Acceleration value (0-254)
 * @return 1 on success, 0 on failure
 */
int SMS_STS::WritePosEx(u8 ID, s16 Position, u16 Speed, u8 ACC)
{
	u16 encodedPosition = ServoUtils::encodeSignedValue(Position, SMS_STS_DIRECTION_BIT_POS);

	u8 bBuf[7];
	bBuf[0] = ACC;
	Host2SCS(bBuf+1, bBuf+2, encodedPosition);
	Host2SCS(bBuf+3, bBuf+4, 0);
	Host2SCS(bBuf+5, bBuf+6, Speed);

	return genWrite(ID, SMS_STS_ACC, bBuf, 7);
}

/**
 * @brief Register write position command (executes on RegWriteAction)
 * 
 * Queues position/speed/acceleration command for later execution.
 * Use with RegWriteAction for synchronized multi-servo motion.
 * 
 * @param ID Servo ID
 * @param Position Target position (0-4095 steps)
 * @param Speed Moving speed (0-3400 steps/s)
 * @param ACC Acceleration value (0-254)
 * @return 1 on success, 0 on failure
 */
int SMS_STS::RegWritePosEx(u8 ID, s16 Position, u16 Speed, u8 ACC)
{
	u16 encodedPosition = ServoUtils::encodeSignedValue(Position, SMS_STS_DIRECTION_BIT_POS);

	u8 bBuf[7];
	bBuf[0] = ACC;
	Host2SCS(bBuf+1, bBuf+2, encodedPosition);
	Host2SCS(bBuf+3, bBuf+4, 0);
	Host2SCS(bBuf+5, bBuf+6, Speed);

	return regWrite(ID, SMS_STS_ACC, bBuf, 7);
}

/**
 * @brief Synchronized position write for multiple servos
 * 
 * Sends position/speed/acceleration commands to multiple servos simultaneously.
 * All servos receive commands in single transmission for coordinated motion.
 * 
 * @param ID Array of servo IDs
 * @param IDN Number of servos
 * @param Position Array of target positions
 * @param Speed Array of speeds (NULL for 0 speed)
 * @param ACC Array of accelerations (NULL for 0 acceleration)
 */
void SMS_STS::SyncWritePosEx(u8 ID[], u8 IDN, s16 Position[], u16 Speed[], u8 ACC[])
{
	SyncWriteBuffer buffer(IDN, 7);
	if(!buffer.isValid()){
		return;  // Allocation failed
	}
	for(u8 i = 0; i<IDN; i++){
		u16 encodedPosition = ServoUtils::encodeSignedValue(Position[i], SMS_STS_DIRECTION_BIT_POS);

		u8 bBuf[7];
		u16 V = Speed ? Speed[i] : 0;
		bBuf[0] = ACC ? ACC[i] : 0;
		Host2SCS(bBuf+1, bBuf+2, encodedPosition);
		Host2SCS(bBuf+3, bBuf+4, 0);
		Host2SCS(bBuf+5, bBuf+6, V);
		buffer.writeMotorData(i, bBuf, 7);
	}
	syncWrite(ID, IDN, SMS_STS_ACC, buffer.getBuffer(), 7);
}

/**
 * @brief Set servo operating mode
 * 
 * Configures servo for different operation modes:
 * - Mode 0: Servo position mode (default)
 * - Mode 1: Wheel closed-loop speed mode
 * - Mode 2: Wheel open-loop power mode
 * 
 * @param ID Servo ID
 * @param mode Operating mode (0-2)
 * @return 1 on success, 0 on failure (invalid mode or communication error)
 */
int SMS_STS::Mode(u8 ID, u8 mode)
{
	// Modes: 0 (servo), 1 (wheel closed-loop), 2 (wheel open-loop), 3 (stepper - not implemented)
	if(!(mode == SMS_STS_MODE_SERVO || mode == SMS_STS_MODE_WHEEL_CLOSED || mode == SMS_STS_MODE_WHEEL_OPEN)){
		Err = 1;
		return 0;
	}
	Err = 0;
	return writeByte(ID, SMS_STS_MODE, mode);
}

/**
 * @brief Initialize motor with operating mode and torque setting
 *
 * Convenience function that performs complete motor initialization:
 * 1. Unlocks EEPROM (to allow Operating_Mode write)
 * 2. Sets operating mode (0=position, 1=velocity, 2=PWM)
 * 3. Locks EEPROM (to save the mode setting)
 * 4. Optionally enables/disables torque
 *
 * This is the recommended way to initialize motors as it ensures the
 * Operating_Mode register (stored in EEPROM) is properly written and persists.
 *
 * @param ID Servo ID
 * @param mode Operating mode (0=servo, 1=wheel closed-loop, 2=wheel open-loop)
 * @param enableTorque 1 to enable torque, 0 to disable (default: 1)
 * @return 1 on success, 0 on failure
 */
int SMS_STS::InitMotor(u8 ID, u8 mode, u8 enableTorque)
{
	// Unlock EEPROM to allow writing to Operating_Mode register (address 33, stored in EEPROM)
	int ret = unLockEeprom(ID);
	if(ret == 0){
		Err = 1;
		return 0;
	}

	// Set operating mode
	ret = Mode(ID, mode);
	if(ret == 0){
		Err = 1;
		return 0;
	}

	// Lock EEPROM to save the Operating_Mode setting
	ret = LockEeprom(ID);
	if(ret == 0){
		Err = 1;
		return 0;
	}

	// Enable or disable torque as requested
	ret = EnableTorque(ID, enableTorque);
	if(ret == 0){
		Err = 1;
		return 0;
	}

	Err = 0;
	return 1;
}

/**
 * @brief Write speed command for wheel mode
 *
 * Controls servo speed in wheel mode. Negative speeds reverse direction.
 *
 * @param ID Servo ID
 * @param Speed Target speed (-3400 to +3400 steps/s, negative = reverse)
 * @param ACC Acceleration value (0-254)
 * @return 1 on success, 0 on failure
 */
int SMS_STS::WriteSpe(u8 ID, s16 Speed, u8 ACC)
{
	u16 encodedSpeed = ServoUtils::encodeSignedValue(Speed, SMS_STS_DIRECTION_BIT_POS);

	u8 bBuf[2];
	bBuf[0] = ACC;
	int ret = genWrite(ID, SMS_STS_ACC, bBuf, 1);
	if(ret == 0){
		Err = 1;
		return 0;
	}
	Host2SCS(bBuf+0, bBuf+1, encodedSpeed);

	return genWrite(ID, SMS_STS_GOAL_SPEED_L, bBuf, 2);
}

/**
 * @brief Register write speed command (executes on RegWriteAction)
 *
 * Queues speed command for later synchronized execution.
 *
 * @param ID Servo ID
 * @param Speed Target speed (-3400 to +3400 steps/s)
 * @param ACC Acceleration value (0-254)
 * @return 1 on success, 0 on failure
 */
int SMS_STS::RegWriteSpe(u8 ID, s16 Speed, u8 ACC)
{
	u16 encodedSpeed = ServoUtils::encodeSignedValue(Speed, SMS_STS_DIRECTION_BIT_POS);

	u8 bBuf[2];
	bBuf[0] = ACC;
	int ret = regWrite(ID, SMS_STS_ACC, bBuf, 1);
	if(ret == 0){
		Err = 1;
		return 0;
	}
	Host2SCS(bBuf+0, bBuf+1, encodedSpeed);

	return regWrite(ID, SMS_STS_GOAL_SPEED_L, bBuf, 2);
}

/**
 * @brief Synchronized speed and acceleration write for multiple servos (atomic)
 *
 * Sends speed and acceleration commands to multiple servos simultaneously in a
 * single sync-write packet. The packet spans 7 bytes starting from SMS_STS_ACC
 * (register 41): ACC + GOAL_POSITION(0) + GOAL_TIME(0) + GOAL_SPEED.
 *
 * This is the correct approach for per-servo ACC control in multi-motor systems
 * (e.g. omni-wheel robots). Sending ACC and GOAL_SPEED in the same atomic packet
 * guarantees every motor's acceleration profile is applied together with its
 * speed command in a single bus transaction, with no risk of the ACC write being
 * lost or arriving out of sync.
 *
 * @note GOAL_POSITION and GOAL_TIME bytes are set to 0 and are ignored by the
 *       servo in wheel/velocity mode (Mode 1).
 *
 * @param ID    Array of servo IDs
 * @param IDN   Number of servos
 * @param Speed Array of target speeds (steps/s; negative = reverse direction)
 * @param ACC   Array of accelerations (0-254, units of 100 steps/s²), or NULL
 *              to use ACC=0 for all servos (maximum hardware slew rate)
 */
void SMS_STS::SyncWriteSpe(u8 ID[], u8 IDN, s16 Speed[], u8 ACC[])
{
	// 7 bytes per servo: ACC(41) + GOAL_POS_L(42) + GOAL_POS_H(43)
	//                  + GOAL_TIME_L(44) + GOAL_TIME_H(45)
	//                  + GOAL_SPEED_L(46) + GOAL_SPEED_H(47)
	SyncWriteBuffer buffer(IDN, 7);
	if(!buffer.isValid()){
		return;  // Allocation failed
	}
	for(u8 i = 0; i<IDN; i++){
		u16 encodedSpeed = ServoUtils::encodeSignedValue(Speed[i], SMS_STS_DIRECTION_BIT_POS);

		u8 bBuf[7];
		bBuf[0] = ACC ? ACC[i] : 0;  // SMS_STS_ACC (reg 41)
		bBuf[1] = 0; bBuf[2] = 0;    // SMS_STS_GOAL_POSITION (regs 42-43, unused in wheel mode)
		bBuf[3] = 0; bBuf[4] = 0;    // SMS_STS_GOAL_TIME (regs 44-45)
		Host2SCS(bBuf+5, bBuf+6, encodedSpeed);  // SMS_STS_GOAL_SPEED (regs 46-47)
		buffer.writeMotorData(i, bBuf, 7);
	}
	syncWrite(ID, IDN, SMS_STS_ACC, buffer.getBuffer(), 7);
}

/**
 * @brief Write PWM output for open-loop mode
 * 
 * Directly controls motor PWM in open-loop wheel mode.
 * Negative values reverse direction.
 * 
 * @param ID Servo ID
 * @param Pwm PWM value (-1000 to 1000, negative = reverse)
 * @return 1 on success, 0 on failure
 */
int SMS_STS::WritePwm(u8 ID, s16 Pwm)
{
    u16 encodedPwm = ServoUtils::encodeSignedValue(Pwm, SMS_STS_LOAD_DIRECTION_BIT_POS);

    u8 bBuf[2];
    Host2SCS(bBuf+0, bBuf+1, encodedPwm);

    return genWrite(ID, SMS_STS_GOAL_TIME_L, bBuf, 2);
}

/**
 * @brief Register write PWM command (executes on RegWriteAction)
 * 
 * Queues PWM command for synchronized execution.
 * 
 * @param ID Servo ID
 * @param Pwm PWM value (-1000 to 1000)
 * @return 1 on success, 0 on failure
 */
int SMS_STS::RegWritePwm(u8 ID, s16 Pwm)
{
    u16 encodedPwm = ServoUtils::encodeSignedValue(Pwm, SMS_STS_LOAD_DIRECTION_BIT_POS);

    u8 bBuf[2];
    Host2SCS(bBuf+0, bBuf+1, encodedPwm);

    return regWrite(ID, SMS_STS_GOAL_TIME_L, bBuf, 2);
}

/**
 * @brief Synchronized PWM write for multiple servos
 * 
 * Sends PWM commands to multiple servos simultaneously.
 * 
 * @param ID Array of servo IDs
 * @param IDN Number of servos
 * @param Pwm Array of PWM values
 */
void SMS_STS::SyncWritePwm(u8 ID[], u8 IDN, s16 Pwm[])
{
	SyncWriteBuffer buffer(IDN, 2);
	if(!buffer.isValid()){
		return;  // Allocation failed
	}
	for(u8 i = 0; i<IDN; i++){
		u16 encodedPwm = ServoUtils::encodeSignedValue(Pwm[i], SMS_STS_LOAD_DIRECTION_BIT_POS);

		u8 bBuf[2];
		Host2SCS(bBuf+0, bBuf+1, encodedPwm);
		buffer.writeMotorData(i, bBuf, 2);
	}
	syncWrite(ID, IDN, SMS_STS_GOAL_TIME_L, buffer.getBuffer(), 2);
}

/**
 * @brief Enable or disable servo motor torque
 * 
 * Controls whether servo actively maintains position or is free-moving.
 * 
 * @param ID Servo ID
 * @param Enable 1 to enable torque, 0 to disable (free-moving)
 * @return 1 on success, 0 on failure
 */
int SMS_STS::EnableTorque(u8 ID, u8 Enable)
{
	return writeByte(ID, SMS_STS_TORQUE_ENABLE, Enable);
}

/**
 * @brief Unlock EEPROM for writing
 * 
 * Allows modification of EEPROM parameters (ID, baud rate, limits, etc.).
 * 
 * @param ID Servo ID
 * @return 1 on success, 0 on failure
 */
int SMS_STS::unLockEeprom(u8 ID)
{
	return writeByte(ID, SMS_STS_LOCK, 0);
}

/**
 * @brief Lock EEPROM to prevent accidental changes
 *
 * Protects EEPROM parameters from modification.
 *
 * @param ID Servo ID
 * @return 1 on success, 0 on failure
 */
int SMS_STS::LockEeprom(u8 ID)
{
	return writeByte(ID, SMS_STS_LOCK, 1);
}

/**
 * @brief Calibrate servo center position offset
 * 
 * Initiates automatic calibration of servo zero position.
 * Servo must be manually positioned at desired center before calling.
 * 
 * @param ID Servo ID
 * @return 1 on success, 0 on failure
 */
int SMS_STS::CalibrationOfs(u8 ID)
{
	return writeByte(ID, SMS_STS_TORQUE_ENABLE, SMS_STS_CALIBRATION_CMD);
}

/**
 * @brief Read all feedback data from servo into memory buffer
 *
 * Reads position, speed, load, voltage, temperature, movement status, and current.
 * Data is stored in Mem[] array and can be accessed via ReadPos(), ReadSpeed(), etc.
 *
 * @param ID Servo ID
 * @return 1 on success, 0 on failure
 */
int SMS_STS::FeedBack(int ID)
{
	int nLen = Read(ID, SMS_STS_PRESENT_POSITION_L, Mem, sizeof(Mem));
	if(nLen!=sizeof(Mem)){
		Err = 1;
		return 0;
	}
	Err = 0;
	return 1;
}

/**
 * @brief Read current servo position
 * 
 * Reads position from servo or from cached Mem[] buffer.
 * 
 * @param ID Servo ID, or -1 to read from cached buffer (after FeedBack())
 * @return Position value (-4095 to 4095), -1 on error
 */
int SMS_STS::ReadPos(int ID)
{
		if(ID == -1) {
			return ServoUtils::readSignedWordFromBuffer(
				Mem,
				SMS_STS_PRESENT_POSITION_L - SMS_STS_PRESENT_POSITION_L,
				SMS_STS_PRESENT_POSITION_H - SMS_STS_PRESENT_POSITION_L,
				SMS_STS_DIRECTION_BIT_POS
			);
		}
		Err = 0;
		return readSignedWord(ID, SMS_STS_PRESENT_POSITION_L, SMS_STS_DIRECTION_BIT_POS);
}

/**
 * @brief Read current servo speed
 * 
 * Reads speed from servo or from cached buffer.
 * 
 * @param ID Servo ID, or -1 to read from cached buffer
 * @return Speed value (-32767 to 32767), -1 on error
 */
int SMS_STS::ReadSpeed(int ID)
{
		if(ID == -1) {
			return ServoUtils::readSignedWordFromBuffer(
				Mem,
				SMS_STS_PRESENT_SPEED_L - SMS_STS_PRESENT_POSITION_L,
				SMS_STS_PRESENT_SPEED_H - SMS_STS_PRESENT_POSITION_L,
				SMS_STS_DIRECTION_BIT_POS
			);
		}
		Err = 0;
		return readSignedWord(ID, SMS_STS_PRESENT_SPEED_L, SMS_STS_DIRECTION_BIT_POS);
}

/**
 * @brief Read current servo load
 * 
 * Reads load torque from servo or from cached buffer.
 * 
 * @param ID Servo ID, or -1 to read from cached buffer
 * @return Load value (-1000 to 1000), -1 on error
 */
int SMS_STS::ReadLoad(int ID)
{
		if(ID == -1) {
			return ServoUtils::readSignedWordFromBuffer(
				Mem,
				SMS_STS_PRESENT_LOAD_L - SMS_STS_PRESENT_POSITION_L,
				SMS_STS_PRESENT_LOAD_H - SMS_STS_PRESENT_POSITION_L,
				SMS_STS_LOAD_DIRECTION_BIT_POS
			);
		}
		Err = 0;
		return readSignedWord(ID, SMS_STS_PRESENT_LOAD_L, SMS_STS_LOAD_DIRECTION_BIT_POS);
}

/**
 * @brief Read servo supply voltage
 * 
 * Reads operating voltage from servo or from cached buffer.
 * 
 * @param ID Servo ID, or -1 to read from cached buffer
 * @return Voltage in 0.1V units (e.g., 120 = 12.0V), -1 on error
 */
int SMS_STS::ReadVoltage(int ID)
{	
	int Voltage = -1;
	if(ID==-1){
		Voltage = Mem[SMS_STS_PRESENT_VOLTAGE-SMS_STS_PRESENT_POSITION_L];	
	}else{
		Err = 0;
		Voltage = readByte(ID, SMS_STS_PRESENT_VOLTAGE);
		if(Voltage==-1){
			Err = 1;
		}
	}
	return Voltage;
}

/**
 * @brief Read servo internal temperature
 * 
 * Reads temperature from servo or from cached buffer.
 * 
 * @param ID Servo ID, or -1 to read from cached buffer
 * @return Temperature in degrees Celsius, -1 on error
 */
int SMS_STS::ReadTemper(int ID)
{	
	int Temper = -1;
	if(ID==-1){
		Temper = Mem[SMS_STS_PRESENT_TEMPERATURE-SMS_STS_PRESENT_POSITION_L];	
	}else{
		Err = 0;
		Temper = readByte(ID, SMS_STS_PRESENT_TEMPERATURE);
		if(Temper==-1){
			Err = 1;
		}
	}
	return Temper;
}

/**
 * @brief Read servo movement status
 * 
 * Checks if servo is currently moving or has reached target position.
 * 
 * @param ID Servo ID, or -1 to read from cached buffer
 * @return 1 if moving, 0 if stopped, -1 on error
 */
int SMS_STS::ReadMove(int ID)
{
	int Move = -1;
	if(ID==-1){
		Move = Mem[SMS_STS_MOVING-SMS_STS_PRESENT_POSITION_L];	
	}else{
		Err = 0;
		Move = readByte(ID, SMS_STS_MOVING);
		if(Move==-1){
			Err = 1;
		}
	}
	return Move;
}

/**
 * @brief Read servo motor current
 * 
 * Reads motor current from servo or from cached buffer.
 * 
 * @param ID Servo ID, or -1 to read from cached buffer
 * @return Current value in mA, -1 on error
 */
int SMS_STS::ReadCurrent(int ID)
{
		if(ID == -1) {
			return ServoUtils::readSignedWordFromBuffer(
				Mem,
				SMS_STS_PRESENT_CURRENT_L - SMS_STS_PRESENT_POSITION_L,
				SMS_STS_PRESENT_CURRENT_H - SMS_STS_PRESENT_POSITION_L,
				SMS_STS_DIRECTION_BIT_POS
			);
		}
		Err = 0;
		return readSignedWord(ID, SMS_STS_PRESENT_CURRENT_L, SMS_STS_DIRECTION_BIT_POS);
}

```

## Файл: `scservo_sdk/SMS_STS.h`

```cpp
﻿/**
 * @file SMS_STS.h
 * @brief Feetech SMS/STS Series Serial Servo Application Layer
 *
 * @details This file provides the application programming interface for
 * controlling Feetech SMS and STS series serial bus servo motors.
 * Supports three operating modes:
 * - Mode 0: Servo (position control)
 * - Mode 1: Closed-loop wheel (velocity control with feedback)
 * - Mode 2: Open-loop wheel (PWM control without feedback)
 */

#ifndef _SMS_STS_H
#define _SMS_STS_H

// Baud rate definition
#define	SMS_STS_1M 0
#define	SMS_STS_0_5M 1
#define	SMS_STS_250K 2
#define	SMS_STS_128K 3
#define	SMS_STS_115200 4
#define	SMS_STS_76800 5
#define	SMS_STS_57600 6
#define	SMS_STS_38400 7

//Memory table definition
//-------EEPROM (Read only)--------
#define SMS_STS_FIRMWARE_VER_L 0
#define SMS_STS_FIRMWARE_VER_H 1
#define SMS_STS_MODEL_L 3
#define SMS_STS_MODEL_H 4

//-------EEPROM (Read and Write)--------
#define SMS_STS_ID 5
#define SMS_STS_BAUD_RATE 6
#define SMS_STS_RETURN_DELAY 7
#define SMS_STS_RESPONSE_STATUS_LEVEL 8
#define SMS_STS_MIN_ANGLE_LIMIT_L 9
#define SMS_STS_MIN_ANGLE_LIMIT_H 10
#define SMS_STS_MAX_ANGLE_LIMIT_L 11
#define SMS_STS_MAX_ANGLE_LIMIT_H 12
#define SMS_STS_MAX_TEMPERATURE_LIMIT 13
#define SMS_STS_MAX_INPUT_VOLT 14
#define SMS_STS_MIN_INPUT_VOLT 15
#define SMS_STS_MAX_TORQUE_L 16
#define SMS_STS_MAX_TORQUE_H 17
#define SMS_STS_PHASE 18
#define SMS_STS_UNLOADING_CONDITION 19
#define SMS_STS_LED_ALARM_CONDITION 20
#define SMS_STS_MODE0_P_COEF 21
#define SMS_STS_MODE0_D_COEF 22
#define SMS_STS_MODE0_I_COEF 23
#define SMS_STS_MINIMUM_STARTUP_FORCE_L 24
#define SMS_STS_MINIMUM_STARTUP_FORCE_H 25
#define SMS_STS_CW_DEAD 26
#define SMS_STS_CCW_DEAD 27
#define SMS_STS_PROTECTION_CURRENT_L 28
#define SMS_STS_PROTECTION_CURRENT_H 29
#define SMS_STS_ANGULAR_RESOLUTION 30
#define SMS_STS_OFS_L 31
#define SMS_STS_OFS_H 32
#define SMS_STS_MODE 33
#define SMS_STS_PROTECTIVE_TORQUE 34
#define SMS_STS_PROTECTION_TIME 35
#define SMS_STS_OVERLOAD_TORQUE 36
#define SMS_STS_MODE1_P_COEF 37
#define SMS_STS_OVER_CURRENT_PROTECTION_TIME 38
#define SMS_STS_MODE1_I_COEF 39

//-------SRAM (Read and Write)--------
#define SMS_STS_TORQUE_ENABLE 40
#define SMS_STS_ACC 41
#define SMS_STS_GOAL_POSITION_L 42
#define SMS_STS_GOAL_POSITION_H 43
#define SMS_STS_GOAL_TIME_L 44
#define SMS_STS_GOAL_TIME_H 45
#define SMS_STS_GOAL_SPEED_L 46
#define SMS_STS_GOAL_SPEED_H 47
#define SMS_STS_TORQUE_LIMIT_L 48
#define SMS_STS_TORQUE_LIMIT_H 49
#define SMS_STS_LOCK 55

//-------SRAM (Read only)--------
#define SMS_STS_PRESENT_POSITION_L 56
#define SMS_STS_PRESENT_POSITION_H 57
#define SMS_STS_PRESENT_SPEED_L 58
#define SMS_STS_PRESENT_SPEED_H 59
#define SMS_STS_PRESENT_LOAD_L 60
#define SMS_STS_PRESENT_LOAD_H 61
#define SMS_STS_PRESENT_VOLTAGE 62
#define SMS_STS_PRESENT_TEMPERATURE 63
#define SMS_STS_MOVING 66
#define SMS_STS_PRESENT_CURRENT_L 69
#define SMS_STS_PRESENT_CURRENT_H 70

// Bit position constants for data encoding
#define SMS_STS_DIRECTION_BIT_POS 15    // Bit position for direction flag in position/speed
#define SMS_STS_LOAD_DIRECTION_BIT_POS 10  // Bit position for direction flag in load/PWM

// Operating mode values
#define SMS_STS_MODE_SERVO 0        // Servo mode (position control)
#define SMS_STS_MODE_WHEEL_CLOSED 1 // Wheel mode - closed loop (velocity control)
#define SMS_STS_MODE_WHEEL_OPEN 2   // Wheel mode - open loop (PWM control)
#define SMS_STS_MODE_STEPPER 3      // Stepper mode (not implemented in SDK)

// Special servo IDs
#define SMS_STS_BROADCAST_ID 0xFE   // Broadcast ID for all servos

// Calibration command
#define SMS_STS_CALIBRATION_CMD 128 // Command value for midpoint calibration

#include "SCSerial.h"
#include "INST.h"
#include "ServoErrors.h"
#include "ServoUtils.h"

/**
 * @class SMS_STS
 * @brief Application layer interface for SMS/STS series serial servos
 *
 * @details Provides high-level control functions for Feetech SMS and STS series
 * servo motors. Supports three operating modes with complete read/write functionality.
 *
 * **Operating Modes:**
 * - Mode 0: Servo mode (position control) - precise positioning
 * - Mode 1: Wheel mode closed-loop (velocity control) - speed feedback
 * - Mode 2: Wheel mode open-loop (PWM control) - direct motor power
 *
 * **Key Features:**
 * - Write operations: immediate, asynchronous (Reg), and synchronized (Sync)
 * - Comprehensive feedback: position, speed, load, voltage, temperature, current
 * - EEPROM management: lock/unlock for persistent configuration
 * - LSP compliant: uniform InitMotor() and Mode() methods
 *
 * **Usage Example:**
 * @code
 * SMS_STS servo;
 * servo.begin(1000000, "/dev/ttyUSB0");
 * servo.InitMotor(1, 0, 1);  // ID=1, Mode=0 (servo), Enable torque
 * servo.WritePosEx(1, 2048, 1000, 50);  // Move to center position
 * @endcode
 *
 * @note Remember to call begin() before using any servo methods
 * @see SCSerial for serial communication layer methods
 */
class SMS_STS : public SCSerial
{
public:
	/** @brief Default constructor */
	SMS_STS();
	/** @brief Constructor with protocol end byte
	 *  @param End Protocol end byte (0 or 1) */
	SMS_STS(u8 End);
	/** @brief Constructor with protocol end byte and response level
	 *  @param End Protocol end byte (0 or 1)
	 *  @param Level Response level (0=no response, 1=response enabled) */
	SMS_STS(u8 End, u8 Level);
	/** @brief Write position to single servo (Mode 0)
	 *  @param ID Servo ID (0-253, 254=broadcast)
	 *  @param Position Target position (0-4095 steps)
	 *  @param Speed Movement speed (0-3400 steps/s)
	 *  @param ACC Acceleration (0-254, units of 100 steps/s²)
	 *  @return 1 on success, 0 on failure */
	virtual int WritePosEx(u8 ID, s16 Position, u16 Speed, u8 ACC = 0);

	/** @brief Async write position (execute with RegWriteAction)
	 *  @param ID Servo ID
	 *  @param Position Target position
	 *  @param Speed Movement speed
	 *  @param ACC Acceleration
	 *  @return 1 on success, 0 on failure */
	virtual int RegWritePosEx(u8 ID, s16 Position, u16 Speed, u8 ACC = 0);

	/** @brief Sync write position to multiple servos
	 *  @param ID Array of servo IDs
	 *  @param IDN Number of servos
	 *  @param Position Array of target positions
	 *  @param Speed Array of speeds (NULL for 0)
	 *  @param ACC Array of accelerations (NULL for 0) */
	virtual void SyncWritePosEx(u8 ID[], u8 IDN, s16 Position[], u16 Speed[], u8 ACC[]);

	/** @brief Set operating mode
	 *  @param ID Servo ID
	 *  @param mode 0=servo, 1=wheel closed-loop, 2=wheel open-loop
	 *  @return 1 on success, 0 on failure */
	virtual int Mode(u8 ID, u8 mode);

	/** @brief Initialize motor (unlock EEPROM → set mode → lock EEPROM → enable torque)
	 *  @param ID Servo ID
	 *  @param mode Operating mode (0/1/2)
	 *  @param enableTorque 1=enable, 0=disable (default: 1)
	 *  @return 1 on success, 0 on failure
	 *  @note LSP compliant - available on all servo classes */
	virtual int InitMotor(u8 ID, u8 mode, u8 enableTorque = 1);

	/** @brief Write speed to single servo (Mode 1)
	 *  @param ID Servo ID
	 *  @param Speed Target speed (-3400 to +3400 steps/s)
	 *  @param ACC Acceleration
	 *  @return 1 on success, 0 on failure */
	virtual int WriteSpe(u8 ID, s16 Speed, u8 ACC = 0);

	/** @brief Async write speed */
    virtual int RegWriteSpe(u8 ID, s16 Speed, u8 ACC = 0);

	/** @brief Sync write speed and acceleration to multiple servos (atomic)
	 *
	 *  Sends ACC + GOAL_SPEED to all servos in a single 7-byte sync-write
	 *  packet starting from SMS_STS_ACC (reg 41).  Writing both values in
	 *  one atomic bus transaction ensures per-servo acceleration profiles
	 *  are applied together with their speed commands, which is essential
	 *  for synchronised multi-motor systems (e.g. omni-wheel robots).
	 *
	 *  @param ID    Array of servo IDs
	 *  @param IDN   Number of servos
	 *  @param Speed Array of target speeds (steps/s; negative = reverse)
	 *  @param ACC   Array of accelerations (0-254, units of 100 steps/s²),
	 *               or NULL to use ACC=0 for all servos (max slew rate) */
    virtual void SyncWriteSpe(u8 ID[], u8 IDN, s16 Speed[], u8 ACC[]);

	/** @brief Write PWM to single servo (Mode 2)
	 *  @param ID Servo ID
	 *  @param Pwm PWM duty cycle (±1000 = ±100%)
	 *  @return 1 on success, 0 on failure */
    virtual int WritePwm(u8 ID, s16 Pwm);

	/** @brief Async write PWM */
    virtual int RegWritePwm(u8 ID, s16 Pwm);

	/** @brief Sync write PWM to multiple servos */
    virtual void SyncWritePwm(u8 ID[], u8 IDN, s16 Pwm[]);

	/** @brief Enable/disable motor torque
	 *  @param ID Servo ID
	 *  @param Enable 1=enable, 0=disable (free-moving)
	 *  @return 1 on success, 0 on failure */
	virtual int EnableTorque(u8 ID, u8 Enable);

	/** @brief Unlock EEPROM for writing configuration
	 *  @param ID Servo ID
	 *  @return 1 on success, 0 on failure */
	virtual int unLockEeprom(u8 ID);

	/** @brief Lock EEPROM to protect configuration
	 *  @param ID Servo ID
	 *  @return 1 on success, 0 on failure */
	virtual int LockEeprom(u8 ID);

	/** @brief Calibrate servo midpoint position
	 *  @param ID Servo ID
	 *  @return 1 on success, 0 on failure
	 *  @note Sets offset register to 128 (different from InitMotor which sets operating mode) */
	virtual int CalibrationOfs(u8 ID);

	/** @brief Read all feedback data into internal buffer
	 *  @param ID Servo ID
	 *  @return 1 on success, 0 on failure
	 *  @note Call before using Read* methods with ID=-1 for cached reads */
	virtual int FeedBack(int ID);

	/** @brief Read current position
	 *  @param ID Servo ID, or -1 to read from cache (after FeedBack)
	 *  @return Position (0-4095), -1 on error */
	virtual int ReadPos(int ID);

	/** @brief Read current speed
	 *  @param ID Servo ID, or -1 for cached read
	 *  @return Speed (±3400 steps/s), -1 on error */
	virtual int ReadSpeed(int ID);

	/** @brief Read motor load
	 *  @param ID Servo ID, or -1 for cached read
	 *  @return Load (±1000 = ±100% PWM), -1 on error */
	virtual int ReadLoad(int ID);

	/** @brief Read supply voltage
	 *  @param ID Servo ID, or -1 for cached read
	 *  @return Voltage in 0.1V units, -1 on error */
	virtual int ReadVoltage(int ID);

	/** @brief Read internal temperature
	 *  @param ID Servo ID, or -1 for cached read
	 *  @return Temperature in °C, -1 on error */
	virtual int ReadTemper(int ID);

	/** @brief Read movement status
	 *  @param ID Servo ID, or -1 for cached read
	 *  @return 1=moving, 0=stopped, -1=error */
	virtual int ReadMove(int ID);

	/** @brief Read motor current
	 *  @param ID Servo ID, or -1 for cached read
	 *  @return Current in milliamps, -1 on error */
	virtual int ReadCurrent(int ID);
private:
	u8 Mem[SMS_STS_PRESENT_CURRENT_H-SMS_STS_PRESENT_POSITION_L+1];
};

#endif
```

## Файл: `scservo_sdk/ServoErrors.h`

```cpp
/**
 * @file ServoErrors.h
 * @brief Standardized error handling for SCServo SDK
 *
 * @details This file provides structured error codes and result types for
 * consistent error handling across the servo SDK.
 *
 * **Components:**
 * - ServoError enum: Named error codes for all failure modes
 * - ServoResult class: Type-safe result wrapper with backward compatibility
 * - Helper functions: Semantic error checking predicates
 *
 * **Benefits:**
 * - Improved debugging with named error codes
 * - Type-safe error handling with ServoResult
 * - Backward compatible with existing int return values
 * - Clear semantics for cached vs direct reads
 *
 * **Usage Example:**
 * @code
 * ServoResult result = servo.WritePosEx(1, 2048, 1000);
 * if (result.isSuccess()) {
 *     printf("Success! Value: %d\n", result.getValue());
 * } else {
 *     printf("Error: %d\n", static_cast<int>(result.getError()));
 * }
 *
 * // Or use backward-compatible int conversion:
 * int retval = servo.WritePosEx(1, 2048, 1000);
 * if (retval == -1) { // handle error
 * }
 * @endcode
 */

#ifndef _SERVOERRORS_H
#define _SERVOERRORS_H

/**
 * @brief Standard error codes for servo operations
 *
 * Replaces inconsistent use of -1 return values with
 * named error codes for better debugging and error handling.
 */
enum class ServoError {
    SUCCESS = 0,              // Operation completed successfully
    COMM_TIMEOUT = -1,        // Communication timeout
    COMM_RX_FAIL = -2,        // Receive failure
    COMM_TX_FAIL = -3,        // Transmit failure
    INVALID_PARAMETER = -4,   // Invalid parameter passed
    ALLOCATION_FAILED = -5,   // Memory allocation failed
    REGISTER_WRITE_FAILED = -6, // Register write operation failed
    CHECKSUM_ERROR = -7,      // Checksum validation failed
    UNKNOWN_ERROR = -99       // Unknown error occurred
};

/**
 * @brief Result wrapper for servo operations
 *
 * Provides backward compatibility with int return values
 * while supporting structured error handling.
 *
 * Usage:
 *   ServoResult result = someServoOperation();
 *   if (result.isSuccess()) {
 *       int value = result.getValue();
 *   } else {
 *       ServoError err = result.getError();
 *       // Handle error
 *   }
 *
 * Or for backward compatibility:
 *   int value = someServoOperation(); // Implicit conversion
 *   if (value == -1) { ... }
 */
class ServoResult {
private:
    ServoError errorCode;
    int value;

public:
    /**
     * @brief Construct a result with error code and value
     * @param err Error code
     * @param val Associated value (default 0)
     */
    ServoResult(ServoError err, int val = 0)
        : errorCode(err), value(val) {}

    /**
     * @brief Construct a successful result with value
     * @param val Value to return
     */
    ServoResult(int val)
        : errorCode(ServoError::SUCCESS), value(val) {}

    /**
     * @brief Check if operation was successful
     * @return true if no error occurred
     */
    bool isSuccess() const {
        return errorCode == ServoError::SUCCESS;
    }

    /**
     * @brief Get the error code
     * @return ServoError enum value
     */
    ServoError getError() const {
        return errorCode;
    }

    /**
     * @brief Get the associated value
     * @return Integer value
     */
    int getValue() const {
        return value;
    }

    /**
     * @brief Implicit conversion to int for backward compatibility
     * @return Value on success, error code on failure
     */
    operator int() const {
        return isSuccess() ? value : static_cast<int>(errorCode);
    }
};

/**
 * @brief Helper function to check for invalid motor ID
 *
 * Provides a named function instead of magic -1 comparison.
 * Used to check if ID refers to cached data rather than a specific motor.
 *
 * @param ID Motor ID to check
 * @return true if ID == -1 (refers to cached data)
 */
inline bool isInvalidID(int ID) {
    return ID == -1;
}

/**
 * @brief Helper function to check for cached data request
 *
 * More semantic alternative to isInvalidID when checking
 * if we should read from cached buffer vs querying servo.
 *
 * @param ID Motor ID to check
 * @return true if ID == -1 (use cached data)
 */
inline bool isCachedRequest(int ID) {
    return ID == -1;
}

/**
 * @brief Check if a return value indicates an error
 *
 * Helper for legacy code that returns -1 on error.
 *
 * @param returnValue Value to check
 * @return true if value indicates error (< 0)
 */
inline bool isError(int returnValue) {
    return returnValue < 0;
}

/**
 * @brief Check if a return value indicates success
 *
 * Helper for legacy code that returns -1 on error.
 *
 * @param returnValue Value to check
 * @return true if value indicates success (>= 0)
 */
inline bool isSuccess(int returnValue) {
    return returnValue >= 0;
}

#endif // _SERVOERRORS_H

```

## Файл: `scservo_sdk/ServoUtils.h`

```cpp
/**
 * @file ServoUtils.h
 * @brief Utility functions for SCServo SDK implementing DRY principle
 *
 * @details This file provides reusable utility functions that eliminate code
 * duplication across the servo SDK.
 *
 * **Key Functions:**
 * - Direction bit encoding/decoding for signed values
 * - Buffer read operations with direction handling
 * - Cached vs direct servo read abstraction
 * - Motor ID validation
 *
 * **Eliminated Duplication:**
 * - 13+ instances of direction encoding logic
 * - 16+ instances of direction decoding logic
 * - 12+ instances of cache-or-servo read patterns
 *
 * **Usage Example:**
 * @code
 * u16 encoded = ServoUtils::encodeSignedValue(-100, 15);  // Encode with direction bit
 * s16 decoded = ServoUtils::decodeSignedValue(encoded, 15);  // Decode back to -100
 * @endcode
 *
 * @note All functions are inline for zero-overhead abstraction
 * @see SMS_STS.cpp, SCSCL.cpp, HLSCL.cpp for usage examples
 */

#ifndef _SERVO_UTILS_H
#define _SERVO_UTILS_H

#include "SCServo.h"
#include "ServoErrors.h"

namespace ServoUtils {

/**
 * @brief Encode a signed value with direction bit
 * @param value Signed value to encode
 * @param directionBit Bit position for direction flag (default 15)
 * @return Encoded unsigned value with direction bit set if negative
 *
 * This eliminates 13+ instances of duplicate direction encoding logic across:
 * SMS_STS.cpp, SCSCL.cpp, HLSCL.cpp
 */
inline u16 encodeSignedValue(s16 value, u8 directionBit = 15) {
    if (value < 0) {
        u16 absValue = static_cast<u16>(-value);
        return absValue | (1 << directionBit);
    }
    return static_cast<u16>(value);
}

/**
 * @brief Decode a value with direction bit
 * @param encodedValue Encoded unsigned value
 * @param directionBit Bit position for direction flag (default 15)
 * @return Decoded signed value
 *
 * This eliminates 16+ instances of duplicate direction decoding logic
 */
inline s16 decodeSignedValue(u16 encodedValue, u8 directionBit = 15) {
    if (encodedValue & (1 << directionBit)) {
        return -static_cast<s16>(encodedValue & ~(1 << directionBit));
    }
    return static_cast<s16>(encodedValue);
}

/**
 * @brief Read a 16-bit word from memory buffer and combine bytes
 * @param buffer Memory buffer containing data
 * @param offsetLow Offset for low byte (relative to buffer start)
 * @param offsetHigh Offset for high byte (relative to buffer start)
 * @return Combined 16-bit value (high byte << 8 | low byte)
 */
inline u16 readWordFromBuffer(const u8* buffer, u8 offsetLow, u8 offsetHigh) {
    u16 value = buffer[offsetHigh];
    value <<= 8;
    value |= buffer[offsetLow];
    return value;
}

/**
 * @brief Read a 16-bit signed value from memory buffer with direction decoding
 * @param buffer Memory buffer containing data
 * @param offsetLow Offset for low byte
 * @param offsetHigh Offset for high byte
 * @param directionBit Bit position for direction flag
 * @return Decoded signed value
 *
 * This eliminates the repeated pattern of reading two bytes, combining them,
 * and extracting the direction bit found in 16+ locations
 */
inline s16 readSignedWordFromBuffer(const u8* buffer, u8 offsetLow, u8 offsetHigh, u8 directionBit) {
    u16 value = readWordFromBuffer(buffer, offsetLow, offsetHigh);
    return decodeSignedValue(value, directionBit);
}

/**
 * @brief Read a single byte from cached buffer or directly from servo
 * @param servoInstance Reference to servo object (SMS_STS, SCSCL, HLSCL, etc)
 * @param ID Motor ID (-1 to read from cache, >=0 to read from servo)
 * @param registerAddr Register address to read from servo
 * @param cachedBuffer Cached memory buffer
 * @param cacheOffset Offset in cache buffer
 * @param err Error flag output (set to 0 on success, 1 on failure)
 * @return Byte value read, or -1 on error
 *
 * This eliminates 12+ instances of the same read pattern:
 * - If ID == -1: read from cache
 * - Otherwise: read from servo and update error flag
 */
template<typename ServoType>
inline int readByteFromCacheOrServo(
    ServoType& servoInstance,
    int ID,
    u8 registerAddr,
    const u8* cachedBuffer,
    u8 cacheOffset,
    int& err
) {
    if (ID == -1) {
        return cachedBuffer[cacheOffset];
    } else {
        err = 0;
        int value = servoInstance.readByte(ID, registerAddr);
        if (value == -1) {
            err = 1;
        }
        return value;
    }
}

/**
 * @brief Read a signed word from cached buffer or directly from servo
 * @param servoInstance Reference to servo object
 * @param ID Motor ID (-1 for cache, >=0 for servo)
 * @param registerAddrLow Low byte register address
 * @param cachedBuffer Cached memory buffer
 * @param offsetLow Low byte offset in cache
 * @param offsetHigh High byte offset in cache
 * @param directionBit Direction bit position
 * @param err Error flag output
 * @return Signed word value, or -1 on error
 *
 * Combines the common pattern of reading signed words with direction bits
 */
template<typename ServoType>
inline s16 readSignedWordFromCacheOrServo(
    ServoType& servoInstance,
    int ID,
    u8 registerAddrLow,
    const u8* cachedBuffer,
    u8 offsetLow,
    u8 offsetHigh,
    u8 directionBit,
    int& err
) {
    if (ID == -1) {
        return readSignedWordFromBuffer(cachedBuffer, offsetLow, offsetHigh, directionBit);
    } else {
        err = 0;
        s16 value = servoInstance.readSignedWord(ID, registerAddrLow, directionBit);
        if (value == -1) {
            err = 1;
        }
        return value;
    }
}

/**
 * @brief Check if an ID represents a cached read request
 * @param ID Motor ID value
 * @return true if ID is -1 (cached read), false otherwise
 *
 * This provides a semantic wrapper around the -1 comparison pattern
 * found throughout the codebase, improving readability
 */
inline bool isCachedRead(int ID) {
    return ID == -1;
}

/**
 * @brief Validate motor ID is in acceptable range
 * @param ID Motor ID to validate
 * @return true if ID is valid (0-253 or -1 for cached), false otherwise
 */
inline bool isValidMotorID(int ID) {
    return (ID >= 0 && ID <= 253) || ID == -1;
}

} // namespace ServoUtils

#endif // _SERVO_UTILS_H
```

## Файл: `scservo_sdk/SyncWriteBuffer.h`

```cpp
/**
 * @file SyncWriteBuffer.h
 * @brief RAII-based buffer management for synchronized write operations
 *
 * @details This file provides automatic buffer management for sync write operations,
 * eliminating manual allocation/deallocation and preventing memory leaks.
 *
 * **Benefits:**
 * - Automatic cleanup (no memory leaks)
 * - Exception-safe resource management
 * - Consistent error handling
 * - Type-safe operations
 * - Clear ownership semantics
 *
 * **Replaced Manual Allocations:**
 * - SMS_STS::SyncWritePosEx, SyncWriteSpe, SyncWritePwm
 * - SCSCL::SyncWritePos
 * - HLSCL::SyncWritePosEx
 *
 * **Usage Example:**
 * @code
 * SyncWriteBuffer buffer(3, 7);  // 3 motors, 7 bytes per motor
 * if (!buffer.isValid()) {
 *     return;  // Allocation failed
 * }
 * // Use buffer - automatic cleanup when buffer goes out of scope
 * servo.syncWrite(ids, 3, addr, buffer.getBuffer(), 7);
 * @endcode
 *
 * @note Uses RAII (Resource Acquisition Is Initialization) for automatic cleanup
 */

#ifndef _SYNC_WRITE_BUFFER_H
#define _SYNC_WRITE_BUFFER_H

#include "SCServo.h"
#include <cstring>

/**
 * @brief RAII wrapper for sync write buffer management
 *
 * This class eliminates the 6+ instances of manual buffer management:
 * - SMS_STS::SyncWritePosEx, SyncWriteSpe, SyncWritePwm
 * - SCSCL::SyncWritePwm
 * - HLSCL::SyncWritePosEx
 *
 * Benefits:
 * - Automatic cleanup (no memory leaks)
 * - Consistent error handling
 * - Type-safe operations
 * - Clear ownership semantics
 */
class SyncWriteBuffer {
private:
    u8* buffer;
    size_t capacity;
    size_t bytesPerMotor;
    size_t numMotors;

public:
    /**
     * @brief Construct buffer for sync write operations
     * @param motorCount Number of motors in the sync write
     * @param payloadSize Bytes per motor payload
     */
    SyncWriteBuffer(size_t motorCount, size_t payloadSize)
        : buffer(nullptr),
          capacity(motorCount * payloadSize),
          bytesPerMotor(payloadSize),
          numMotors(motorCount) {

        if (capacity > 0) {
            buffer = new u8[capacity];
        }
    }

    /**
     * @brief Destructor - automatically frees buffer
     */
    ~SyncWriteBuffer() {
        if (buffer) {
            delete[] buffer;
            buffer = nullptr;
        }
    }

    // Prevent copying (use move semantics if needed)
    SyncWriteBuffer(const SyncWriteBuffer&) = delete;
    SyncWriteBuffer& operator=(const SyncWriteBuffer&) = delete;

    /**
     * @brief Write data for a specific motor at given index
     * @param motorIndex Motor index (0 to numMotors-1)
     * @param data Data to write
     * @param dataSize Size of data (must be <= bytesPerMotor)
     * @return true on success, false on invalid parameters
     */
    bool writeMotorData(size_t motorIndex, const u8* data, size_t dataSize) {
        if (!buffer || motorIndex >= numMotors || dataSize > bytesPerMotor) {
            return false;
        }

        memcpy(buffer + (motorIndex * bytesPerMotor), data, dataSize);
        return true;
    }

    /**
     * @brief Get raw buffer pointer for passing to syncWrite
     * @return Pointer to buffer, or nullptr if allocation failed
     */
    u8* getBuffer() {
        return buffer;
    }

    /**
     * @brief Get buffer pointer (const version)
     */
    const u8* getBuffer() const {
        return buffer;
    }

    /**
     * @brief Get total buffer size in bytes
     */
    size_t getSize() const {
        return capacity;
    }

    /**
     * @brief Get bytes per motor
     */
    size_t getBytesPerMotor() const {
        return bytesPerMotor;
    }

    /**
     * @brief Get number of motors
     */
    size_t getNumMotors() const {
        return numMotors;
    }

    /**
     * @brief Check if allocation succeeded
     * @return true if buffer is valid, false if allocation failed
     */
    bool isValid() const {
        return buffer != nullptr;
    }

    /**
     * @brief Explicit bool conversion for validity checking
     */
    explicit operator bool() const {
        return isValid();
    }

    /**
     * @brief Fill entire buffer with zeros
     */
    void clear() {
        if (buffer) {
            memset(buffer, 0, capacity);
        }
    }

    /**
     * @brief Fill entire buffer with a specific byte value
     * @param value Byte value to fill with
     */
    void fill(u8 value) {
        if (buffer) {
            memset(buffer, value, capacity);
        }
    }
};

#endif // _SYNC_WRITE_BUFFER_H
```

