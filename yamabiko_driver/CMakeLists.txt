cmake_minimum_required(VERSION 3.5)
project(yamabiko_driver)

find_package(ament_cmake_auto REQUIRED)
ament_auto_find_build_dependencies()

ament_auto_add_executable(yamabiko_driver
  src/yamabiko_driver_node.cpp)

target_link_libraries(yamabiko_driver ypspur)

if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  ament_lint_auto_find_test_dependencies()
endif()

ament_auto_package(
  INSTALL_TO_SHARE
  config
  launch
  scripts
)
