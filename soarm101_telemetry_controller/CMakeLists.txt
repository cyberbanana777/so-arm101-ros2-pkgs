cmake_minimum_required(VERSION 3.8)
project(soarm101_telemetry_controller)

# Строгие предупреждения (хороший тон)
if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# ---- Находим все зависимости ----
find_package(ament_cmake REQUIRED)
find_package(controller_interface REQUIRED)
find_package(pluginlib REQUIRED)
find_package(rclcpp REQUIRED)
find_package(rclcpp_lifecycle REQUIRED)
find_package(soarm101_interfaces REQUIRED)   # твой кастомный пакет с сообщениями

# ---- Создаём библиотеку контроллера ----
add_library(${PROJECT_NAME} SHARED
  src/soarm101_telemetry_controller.cpp
)

# ---- Указываем заголовочные файлы ----
target_include_directories(${PROJECT_NAME} PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include/${PROJECT_NAME}>
)

# ---- Подключаем зависимости к библиотеке ----
ament_target_dependencies(${PROJECT_NAME}
  controller_interface
  pluginlib
  rclcpp
  rclcpp_lifecycle
  soarm101_interfaces
)

# ---- Экспортируем плагин (файл my_controller.xml должен лежать в корне пакета) ----
pluginlib_export_plugin_description_file(controller_interface soarm101_telemetry_controller.xml)

# ---- Установка (куда класть собранную библиотеку и заголовки) ----
install(
  TARGETS ${PROJECT_NAME}
  EXPORT ${PROJECT_NAME}_targets
  DESTINATION lib/${PROJECT_NAME}   # стандартное место для плагинов
)

install(
  DIRECTORY include/
  DESTINATION include/${PROJECT_NAME}
)

# Установка файла описания плагина (чтобы он попал в share)
install(
  FILES soarm101_telemetry_controller.xml
  DESTINATION share/${PROJECT_NAME}
)

# ---- Тесты (можно оставить как есть) ----
if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  set(ament_cmake_copyright_FOUND TRUE)
  set(ament_cmake_cpplint_FOUND TRUE)
  ament_lint_auto_find_test_dependencies()
endif()

ament_package()