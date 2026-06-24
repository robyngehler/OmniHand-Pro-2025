cmake_minimum_required(VERSION 3.16)
project(omni_hand_pro_2025 CXX)

# Set cmake path
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Some option
option(BUILD_PYTHON_BINDING "Build Python binding" ON)
option(BUILD_CPP_EXAMPLES "Build CPP examples" ON)

# --- CAN backend selection (Jetson/aarch64 native SocketCAN) ---------------
# Defined at the top level so BOTH src/ and python/ subdirectories inherit it
# (a set() inside src/CMakeLists.txt would not be visible in python/).
# 1 = ZLG USB-CANFD SDK (x86 only); 2 = SocketCAN (Jetson native).
set(OMNIHAND_PRO_CAN_BACKEND "SOCKETCAN" CACHE STRING "CAN backend: SOCKETCAN or ZLG")
set_property(CACHE OMNIHAND_PRO_CAN_BACKEND PROPERTY STRINGS SOCKETCAN ZLG)
if(OMNIHAND_PRO_CAN_BACKEND STREQUAL "ZLG")
  set(CanfdDevice 1)
elseif(OMNIHAND_PRO_CAN_BACKEND STREQUAL "SOCKETCAN")
  set(CanfdDevice 2)
else()
  message(FATAL_ERROR "Unsupported OMNIHAND_PRO_CAN_BACKEND=${OMNIHAND_PRO_CAN_BACKEND}")
endif()
message(STATUS "OmniHand Pro CAN backend: ${OMNIHAND_PRO_CAN_BACKEND} (CanfdDevice=${CanfdDevice})")

# Some necessary settings
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Master project configuration
if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
  set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

  set(FETCHCONTENT_BASE_DIR ${CMAKE_SOURCE_DIR}/_deps)

  set(BUILD_SHARED_LIBS OFF)
  set(CMAKE_C_VISIBILITY_PRESET hidden)
  set(CMAKE_C_VISIBILITY_INLINES_HIDDEN ON)
  set(CMAKE_CXX_VISIBILITY_PRESET hidden)
  set(CMAKE_CXX_VISIBILITY_INLINES_HIDDEN ON)

  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    if(AIMRT_MUJOCO_SIM_BUILD_TESTS)
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --coverage")
      set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} --coverage")
    endif()
  endif()

  set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
endif()

set(CMAKE_POSITION_INDEPENDENT_CODE ON)


# find python dependencies
if(BUILD_PYTHON_BINDING)
  find_package(
    Python3
    COMPONENTS Interpreter Development
    REQUIRED)
  find_package(pybind11 QUIET)
  if(NOT pybind11_FOUND)
    message(STATUS "System pybind11 not found, will download from git...")
    include(GetPybind11)
  endif()

  include(CheckPythonPackage)
  check_python_package(build BUILD_FOUND)
  check_python_package(setuptools SETUPTOOLS_FOUND)
  check_python_package(wheel WHEEL_FOUND)

  if(NOT BUILD_FOUND
     OR NOT SETUPTOOLS_FOUND
     OR NOT WHEEL_FOUND)
    message(WARNING "Can not find build, setuptools or wheel in your python environment, will not generate aimrt_py package! ")
    message(WARNING "Try to install build, setuptools and wheel by `pip3 install build setuptools wheel --upgrade`")
  endif()
endif()

add_subdirectory(src)

# The vendor node/ is a full ROS2/ament package (rclcpp, rosidl). agx_arm_ctrl
# owns the hardware session via its own bridge, so we do not build it here; it
# also cannot be configured by plain cmake/build.sh (needs colcon/ament).
option(BUILD_VENDOR_NODE "Build the vendor ROS node (unused by agx_arm_ctrl)" OFF)
if(BUILD_VENDOR_NODE)
  add_subdirectory(node)
endif()


if(BUILD_PYTHON_BINDING)
  add_subdirectory(python)
endif()

if(BUILD_CPP_EXAMPLES)
  add_subdirectory(examples)
endif()
