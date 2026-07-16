cmake_minimum_required(VERSION 3.16)
project(soarm101_hardware LANGUAGES CXX)

if(CMAKE_CXX_COMPILER_ID MATCHES "(GNU|Clang)")
  add_compile_options(-Wall -Wextra)
endif()

# ROS 2 зависимости самого hardware-интерфейса
set(THIS_PACKAGE_INCLUDE_DEPENDS
  hardware_interface
  pluginlib
  rclcpp
  rclcpp_lifecycle
)

find_package(ament_cmake REQUIRED)
foreach(Dependency IN ITEMS ${THIS_PACKAGE_INCLUDE_DEPENDS})
  find_package(${Dependency} REQUIRED)
endforeach()

# Находим наш SDK
find_package(scservo_sdk REQUIRED)

## Собираем hardware-плагин (теперь только наши исходники, без SDK)
add_library(
  soarm101_hardware
  SHARED
  src/soarm101_hardware.cpp
  # добавь сюда другие .cpp, если есть
)

target_compile_features(soarm101_hardware PUBLIC cxx_std_17)
target_include_directories(soarm101_hardware PUBLIC
  $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include/${PROJECT_NAME}>
)

# Стандартные ROS-зависимости (заголовки, флаги компиляции)
ament_target_dependencies(
  soarm101_hardware PUBLIC
  ${THIS_PACKAGE_INCLUDE_DEPENDS}
)

# Линкуемся с библиотекой scservo_sdk из soarm101_sdk
target_link_libraries(soarm101_hardware PUBLIC
  scservo_sdk::scservo_sdk
)

# Регистрация плагина для ros2_control
pluginlib_export_plugin_description_file(hardware_interface soarm101_hardware.xml)

## Установка
install(
  DIRECTORY include/
  DESTINATION include/${PROJECT_NAME}
)

install(TARGETS soarm101_hardware
  EXPORT export_soarm101_hardware
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)

## Тестирование (если будет)
if(BUILD_TESTING)
  find_package(ament_cmake_gtest REQUIRED)
endif()

## Экспорт для других пакетов
ament_export_targets(export_soarm101_hardware HAS_LIBRARY_TARGET)
ament_export_dependencies(${THIS_PACKAGE_INCLUDE_DEPENDS})
ament_package()