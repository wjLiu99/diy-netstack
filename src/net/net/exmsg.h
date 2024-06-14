//交换消息，处理消息,核心工作线程
#ifndef EXMSG_H
#define EXMSG_H

#include"net_err.h"

net_err_t exmsg_init(void);
net_err_t exmsg_start(void);

#endif