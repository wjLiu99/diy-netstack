#ifndef NET_ERR_H
#define NET_ERR_H

typedef enum _net_err_t{
    NET_ERR_OK = 0,
    NET_ERR_SYS = -1,
    NET_ERR_MEM = -2,
    NET_ERR_FULL = -3,
    NET_ERR_TMO = -4,
    NET_ERR_SIZE = -5,
    NET_ERR_PARAM = -6,
    NET_ERR_NONE = -7,
    NET_ERR_STATE = -8,
    NET_ERR_IO = -9,
    NET_ERR_EXIST = -10,
    NET_ERR_UNSUPPORT = -11,
    NET_ERR_UNREACH = -12,
    NET_ERR_CHECKSUM = -13,
    
}net_err_t;
#endif