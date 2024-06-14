//协议栈初始化以及启动
#ifndef NET_H
#define NET_H
#include "net_err.h"
 
net_err_t net_init(void);
net_err_t net_start(void);
#endif