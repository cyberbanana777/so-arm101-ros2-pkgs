cmake_minimum_required(VERSION 3.16)
project(soarm101_hardware LANGUAGES CXX)

if(CMAKE_CXX_COMPILER_ID MATCHES "(GNU|Clang)")
  add_compile_options(-Wall -Wextra)
endif()

# ROS 2 dependencies for hardware interface
set(THIS_PACKAGE_INCLUDE_DEPENDS
  hardware_interface
  pluginlib
  rclcpp
  rclcpp_lifecycle
  std_srvs 
)

find_package(ament_cmake REQUIRED)
foreach(Dependency IN ITEMS ${THIS_PACKAGE_INCLUDE_DEPENDS})
  find_package(${Dependency} REQUIRED)
endforeach()

find_package(scservo_sdk REQUIRED)

## Build hardware-plugin
add_library(
  soarm101_hardware
  SHARED
  src/soarm101_hardware.cpp
)

target_compile_features(soarm101_hardware PUBLIC cxx_std_17)
target_include_directories(soarm101_hardware PUBLIC
  $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include/${PROJECT_NAME}>
)

# std ROS2-dependencies
ament_target_dependencies(
  soarm101_hardware PUBLIC
  ${THIS_PACKAGE_INCLUDE_DEPENDS}
)

# link lib scservo_sdk from scservo_sdk
target_link_libraries(soarm101_hardware PUBLIC
  scservo_sdk::scservo_sdk
)

# registration plugin for ros2_control
pluginlib_export_plugin_description_file(hardware_interface soarm101_hardware.xml)

## Install
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

install(FILES LICENSE NOTICE DESTINATION share/${PROJECT_NAME}/)

## Tests
if(BUILD_TESTING)
  find_package(ament_cmake_gtest REQUIRED)
endif()

## Export for others pkgs
ament_export_targets(export_soarm101_hardware HAS_LIBRARY_TARGET)
ament_export_dependencies(${THIS_PACKAGE_INCLUDE_DEPENDS})
ament_package()