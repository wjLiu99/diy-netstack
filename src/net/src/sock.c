#include "sock.h"
#include "sys.h"
#define SOCKET_MAX_NR       10

static x_socket_t socket_tbl[SOCKET_MAX_NR];

//返回socket在表中的索引
static int get_index (x_socket_t *s) {
    return (int)(s - socket_tbl);
}

static x_socket_t *get_socket (int idx) {
    if ((idx < 0) || (idx >= SOCKET_MAX_NR)) {
        return (x_socket_t *)0;
    }

    return socket_tbl + idx;
}

//socket分配，直接查表不用mblock
static x_socket_t *socket_alloc (void) {
    x_socket_t *s = (x_socket_t *)0;
    for (int i = 0; i < SOCKET_MAX_NR; i++) {
        x_socket_t *cur = socket_tbl + i;
        if (cur->state == SOCKET_STATE_FREE) {
            cur->state = SOCKET_STATE_USED;
            s = cur;
            break;
        }
    }

    return s;
}
//socket释放
static void socket_free (x_socket_t *s) {
    s->state = SOCKET_STATE_FREE;

}


net_err_t socket_init (void) {
    plat_memset(socket_tbl, 0, sizeof(socket_tbl));

    return NET_ERR_OK;
}