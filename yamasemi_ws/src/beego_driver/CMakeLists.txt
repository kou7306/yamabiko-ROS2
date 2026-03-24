cmake_minimum_required(VERSION 3.5)
project(beego_driver)

find_package(ament_cmake_auto REQUIRED)
ament_auto_find_build_dependencies()

ament_auto_add_executable(beego_driver
  src/beego_driver_node.cpp)

target_link_libraries(beego_driver ypspur)

target_include_directories(beego_driver
  PUBLIC ${ypspur_INCLUDE_DIRS})

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
