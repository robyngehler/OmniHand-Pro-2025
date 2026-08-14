// Copyright (c) 2025, Agibot Co., Ltd.
// OmniHand Pro 2025 SDK is licensed under Mulan PSL v2.

/**
 * @file c_can_bus_device_socket_can.cpp
 * @brief
 * @author agiuser
 * @date 25-7-31
 **/

#include "c_can_bus_device_socket_can.h"

#include <cstring>
#include <cstdlib>
#include <iostream>

#include <fcntl.h>
#include <linux/can.h>
#include <linux/can/raw.h>
#include <net/if.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

/* How long RecvFrame waits on the socket before re-checking the interrupt
 * request. It bounds shutdown, nothing else: a frame wakes the poll
 * immediately, so this does not add receive latency. */
static constexpr int kRecvPollTimeoutMs = 20;

CanBusDeviceSocketCan::CanBusDeviceSocketCan() {
  /*打开设备*/
  if (CanBusDeviceSocketCan::OpenDevice() == -1) {
    return;
  }

  /*启动接收帧线程*/
  pthread_ = new std::thread(&CanBusDeviceSocketCan::RecvFrame, this);
}

CanBusDeviceSocketCan::~CanBusDeviceSocketCan() {
  /*请求中止*/
  RequestInterrupt();

  /*等待线程释放*/
  if (pthread_ != nullptr) {
    pthread_->join();
    delete pthread_;
  }

  /*关闭设备*/
  CanBusDeviceSocketCan::CloseDevice();
}

int CanBusDeviceSocketCan::OpenDevice() {
  // TODO 发送和接收的CAN socket需要区分开吗？单CAN socket也可以双向通信
  /*创建socket*/
  fd_sock_ = socket(PF_CAN, SOCK_RAW, CAN_RAW);

  /*设置为非阻塞*/
  int flags = fcntl(fd_sock_, F_GETFL, 0);
  fcntl(fd_sock_, F_SETFL, flags | O_NONBLOCK);

  /* Interface from OMNIHAND_SOCKETCAN_IFACE env var, default can0. */
  struct ifreq ifr {};
  const char* env_iface = std::getenv("OMNIHAND_SOCKETCAN_IFACE");
  const char* iface = (env_iface && env_iface[0] != '\0') ? env_iface : "can0";
  std::strncpy(ifr.ifr_name, iface, IFNAMSIZ - 1);
  ifr.ifr_name[IFNAMSIZ - 1] = '\0';
  ioctl(fd_sock_, SIOCGIFINDEX, &ifr);

  /*地址*/
  struct sockaddr_can addr {};
  addr.can_family = AF_CAN;
  addr.can_ifindex = ifr.ifr_ifindex;

  /*使能CANFD*/
  int enableFD = 1;
  setsockopt(fd_sock_, SOL_CAN_RAW, CAN_RAW_FD_FRAMES, &enableFD, sizeof(enableFD));

  /*
  int rcvOwn = 1;
  setsockopt(m_fd_sock,SOL_CAN_RAW,CAN_RAW_RECV_OWN_MSGS,&rcvOwn,sizeof(rcvOwn));

  int rcvTimeout = 1;
  setsockopt(m_fd_sock,SOL_CAN_RAW,SO_RCVTIMEO,&rcvTimeout,sizeof(rcvTimeout));

  //设置一组接收过滤规则，received_can_id & can_mask = can_id & can_mask
  struct can_filter filters[3];
  filters[0].can_id = 0;
  filters[0].can_mask = CAN_EFF_MASK;   //扩展帧
  setsockopt(m_fd_sock,SOL_CAN_RAW,CAN_RAW_FILTER,&filters,sizeof(filters));
  */

  int bindRes = bind(fd_sock_, (struct sockaddr *)&addr, sizeof(addr));
  return bindRes;
}

int CanBusDeviceSocketCan::CloseDevice() {
  /*关闭套接字*/
  return close(fd_sock_);
}

void CanBusDeviceSocketCan::RecvFrame() {
  while (!IsInterruptRequested()) {
    /* Wait for the socket rather than polling it.
     *
     * This loop used to call read() on a non-blocking socket with nothing in
     * between, so on an idle bus it spun as fast as the CPU allowed: measured at
     * 100.0 % of one core per hand on a Jetson AGX Orin, with the bus carrying
     * 25 frames per second. Two hands cost two cores doing nothing.
     *
     * The original note here — a blocking read would keep the thread from being
     * released — is correct, and poll() is the answer to it rather than a reason
     * to spin: the timeout bounds how long the destructor waits for the join,
     * while a frame still wakes the thread immediately, so receive latency is
     * unchanged. The socket deliberately stays non-blocking, so even a spurious
     * wakeup cannot turn into a blocked read. */
    struct pollfd pfd {};
    pfd.fd = fd_sock_;
    pfd.events = POLLIN;
    const int ready = poll(&pfd, 1, kRecvPollTimeoutMs);
    if (ready <= 0) {
      continue; /* timeout or interrupted: re-check the interrupt request */
    }

    canfd_frame frame{};
    int ret = read(fd_sock_, &frame, sizeof(frame));
    if (ret > 0) {
      CanfdFrame rep{};
      rep.can_id_ = frame.can_id & CAN_EFF_MASK;
      rep.len_ = frame.len;
      memcpy(rep.data_, frame.data, rep.len_);

      if (show_data_details_.load()) {
        std::cout << "RCV: " << rep;
      }

      /*与已有等待中的请求相匹配时先保存下来，后续RPC操作进行匹配返回请求的应答结果，未匹配的视为主动上报信息，直接调用预先设定的回调函数进行处理*/
      {
        bool matchedReq = false;
        std::lock_guard<std::mutex> lockGuard(mutex_reqRep_);
        for (auto reqId : uset_req_) {
          if (msg_match_judge_) {
            matchedReq = msg_match_judge_(reqId, rep.can_id_);
            if (matchedReq) {
              umap_rep_[rep.can_id_] = rep;
              break;
            }
          }
        }

        if (matchedReq) {
          continue;
        }
      }

      if (callback_) {
        callback_(rep);
      }
    }
  }
}

int CanBusDeviceSocketCan::SendFrame(unsigned int id, unsigned char *data, unsigned char length) {
  canfd_frame frame{};
  frame.can_id = id | CAN_EFF_FLAG;  // 实际发送id需要和CAN_EFF_FLAG进行或运算才能正确发送（candump可监控）
  frame.len = length;
  memcpy(frame.data, data, length);

  int sndRet = write(fd_sock_, &frame, sizeof(frame));
  if (sndRet > 0) {
    return 0;
  } else {
    return -1;
  }
}