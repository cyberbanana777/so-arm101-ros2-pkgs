cmake_minimum_required(VERSION 3.16)
project(soarm101_hardware LANGUAGES CXX)

if(CMAKE_CXX_COMPILER_ID MATCHES "(GNU|Clang)")
  add_compile_options(-Wall -Wextra)
endif()

# ROS 2 depends
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

# --- YAML-CPP (for parsing calibration) ---
find_package(yaml-cpp REQUIRED)

# --- Build lib with 2 classes
add_library(
  soarm101_hardware
  SHARED
  src/soarm101_hardware_leader.cpp
  src/soarm101_hardware_follower.cpp
)

target_compile_features(soarm101_hardware PUBLIC cxx_std_17)
target_include_directories(soarm101_hardware PUBLIC
  $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include/${PROJECT_NAME}>
)

ament_target_dependencies(
  soarm101_hardware PUBLIC
  ${THIS_PACKAGE_INCLUDE_DEPENDS}
)

target_link_libraries(soarm101_hardware PUBLIC
  scservo_sdk::scservo_sdk
  yaml-cpp 
)

# --- Plugin registration ---
pluginlib_export_plugin_description_file(hardware_interface soarm101_hardware_leader.xml)
pluginlib_export_plugin_description_file(hardware_interface soarm101_hardware_follower.xml)

# --- Install ---
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

install(
  FILES
    soarm101_hardware_leader.xml
    soarm101_hardware_follower.xml
  DESTINATION share/${PROJECT_NAME}
)

# --- Tests ---
if(BUILD_TESTING)
  find_package(ament_cmake_gtest REQUIRED)
endif()

# --- Export ---
ament_export_targets(export_soarm101_hardware HAS_LIBRARY_TARGET)
ament_export_dependencies(${THIS_PACKAGE_INCLUDE_DEPENDS})
ament_package()