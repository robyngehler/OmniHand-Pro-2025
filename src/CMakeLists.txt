# Copyright (c) 2025, Agibot Co., Ltd.
# OmniHand Pro 2025 SDK is licensed under Mulan PSL v2

# CanfdDevice is set at the top-level CMakeLists.txt via OMNIHAND_PRO_CAN_BACKEND.
# (1 = ZLG USB-CANFD x86 SDK, 2 = SocketCAN.) Do not set it here, or it would
# override the inherited value and force the x86 backend.

file(GLOB MODULE_SOURCES 
  ${CMAKE_CURRENT_SOURCE_DIR}/*.cc 
  ${CMAKE_CURRENT_SOURCE_DIR}/can_bus_device/*.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/kinematics_solver/*.cc)

if(CanfdDevice EQUAL 1) #ZLG周立功的usbcanfd的CAN盒SDK
  list(APPEND MODULE_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/can_bus_device/zlg_usb_canfd/c_zlg_usbcanfd_sdk.cc)
elseif(CanfdDevice EQUAL 2) #socketCAN
  list(APPEND MODULE_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/can_bus_device/socket_can/c_can_bus_device_socket_can.cc)
endif()

add_library(omniHandPro25Can SHARED ${MODULE_SOURCES})

set_target_properties(omniHandPro25Can PROPERTIES
  INSTALL_RPATH "$ORIGIN"
  BUILD_WITH_INSTALL_RPATH TRUE)

# src/ must be on the include path for BOTH backends: the sources include
# "kinematics_solver/..." and "can_bus_device/..." relative to here. Upstream
# only added it inside the ZLG branch, so the SocketCAN build could not find
# kinematics_solver.h. Add it unconditionally.
target_include_directories(omniHandPro25Can PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

if(CanfdDevice EQUAL 1)
  target_compile_definitions(omniHandPro25Can PRIVATE ZLG_USBCANFD_SDK)
  if(NOT USER_PATH)
    set(USER_PATH ${PROJECT_SOURCE_DIR}/thirdParty/usbcanfd_libusb_x64_1.0.10_250328)
  endif()

  find_path(THIRD_PARTY_PATH zcan.h HINTS ${USER_PATH})
  message(${THIRD_PARTY_PATH})
  target_include_directories(
    omniHandPro25Can
    PUBLIC ${THIRD_PARTY_PATH}
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
  target_link_directories(omniHandPro25Can PUBLIC ${THIRD_PARTY_PATH})
  target_link_libraries(omniHandPro25Can PUBLIC usbcanfd)
elseif(CanfdDevice EQUAL 2)
  target_compile_definitions(omniHandPro25Can PRIVATE SOCKET_CAN)
endif()
 
install(TARGETS omniHandPro25Can DESTINATION ${CMAKE_INSTALL_PREFIX}/lib)
install(FILES c_agibot_hand.h DESTINATION ${CMAKE_INSTALL_PREFIX}/include)
install(FILES export_symbols.h DESTINATION ${CMAKE_INSTALL_PREFIX}/include)
install(FILES proto.h DESTINATION ${CMAKE_INSTALL_PREFIX}/include)
install(FILES can_bus_device/c_can_bus_device.h DESTINATION ${CMAKE_INSTALL_PREFIX}/include/can_bus_device)
install(FILES kinematics_solver/kinematics_solver.h DESTINATION ${CMAKE_INSTALL_PREFIX}/include/kinematics_solver)
if(CanfdDevice EQUAL 1)
  install(FILES ${PROJECT_SOURCE_DIR}/thirdParty/usbcanfd_libusb_x64_1.0.10_250328/libusbcanfd.so DESTINATION ${CMAKE_INSTALL_PREFIX}/lib)
  install(FILES ${PROJECT_SOURCE_DIR}/thirdParty/usbcanfd_libusb_x64_1.0.10_250328/libusbcanfd.so.1.0.10 DESTINATION ${CMAKE_INSTALL_PREFIX}/lib)
endif()