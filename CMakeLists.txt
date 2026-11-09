cmake_minimum_required(VERSION 3.8)
project(fast_calib)

if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 17)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(std_msgs REQUIRED)
find_package(pcl_conversions REQUIRED)
find_package(rosbag2_cpp REQUIRED)
find_package(rosbag2_storage REQUIRED)
find_package(rosidl_default_generators REQUIRED)

# 先于 PCL 查找 OpenCV，确保 PCL 复用同一版本（含 aruco 的系统 OpenCV）
find_package(OpenCV REQUIRED)
if(NOT EXISTS "${OpenCV_INCLUDE_DIRS}/opencv2/aruco.hpp")
  # 找到的 OpenCV 缺少 aruco 模块时，回退到系统 OpenCV（含 contrib）
  set(OpenCV_DIR "/usr/lib/x86_64-linux-gnu/cmake/opencv4" CACHE PATH "OpenCV config directory" FORCE)
  unset(OpenCV_INCLUDE_DIRS)
  unset(OpenCV_INCLUDE_DIRS CACHE)
  unset(OpenCV_LIBRARIES)
  unset(OpenCV_LIBRARIES CACHE)
  unset(OpenCV_LIBS)
  unset(OpenCV_LIBS CACHE)
  find_package(OpenCV REQUIRED)
endif()

find_package(PCL 1.10 REQUIRED)

rosidl_generate_interfaces(fast_calib_msgs
  "msg/CustomPoint.msg"
  "msg/CustomMsg.msg"
  DEPENDENCIES std_msgs
)

include_directories(
  include
  ${PCL_INCLUDE_DIRS}
  ${OpenCV_INCLUDE_DIRS}
)

set(CMAKE_BUILD_TYPE Release)

if (CMAKE_BUILD_TYPE STREQUAL "Release")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3")
endif()

add_executable(fast_calib src/main.cpp)
rosidl_target_interfaces(fast_calib fast_calib_msgs "rosidl_typesupport_cpp")
ament_target_dependencies(fast_calib
  rclcpp
  sensor_msgs
  std_msgs
  pcl_conversions
  rosbag2_cpp
  rosbag2_storage
)
target_link_libraries(fast_calib
  ${PCL_LIBRARIES}
  ${OpenCV_LIBRARIES}
)

add_executable(multi_fast_calib src/multi_scene.cpp)
rosidl_target_interfaces(multi_fast_calib fast_calib_msgs "rosidl_typesupport_cpp")
ament_target_dependencies(multi_fast_calib
  rclcpp
  sensor_msgs
  std_msgs
  pcl_conversions
  rosbag2_cpp
  rosbag2_storage
)
target_link_libraries(multi_fast_calib
  ${PCL_LIBRARIES}
  ${OpenCV_LIBRARIES}
)

install(TARGETS
  fast_calib
  multi_fast_calib
  DESTINATION lib/${PROJECT_NAME}
)

install(DIRECTORY
  launch
  config
  rviz_cfg
  DESTINATION share/${PROJECT_NAME}
)

ament_package()