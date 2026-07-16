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