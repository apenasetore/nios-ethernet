#ifndef __WEB_SERVER_H__
#define __WEB_SERVER_H__

#include "alt_types.h"
#include "includes.h"

#define TASK_STACKSIZE       4096

#define WS_INITIAL_TASK_PRIO 5
#define RX_PRIO              4
#define TX_PRIO              6

extern OS_STK WSInitialTaskStk[TASK_STACKSIZE];

/* IP defaults: all zeros = use DHCP */
#define IPADDR0  0
#define IPADDR1  0
#define IPADDR2  0
#define IPADDR3  0

#define GWADDR0  0
#define GWADDR1  0
#define GWADDR2  0
#define GWADDR3  0

#define MSKADDR0 255
#define MSKADDR1 255
#define MSKADDR2 255
#define MSKADDR3 0

#define DIE_WITH_ERROR_BUFFER 256

/* Message types for RX -> TX communication */
#define MSG_SERVE_HTML      0
#define MSG_PROCESS_STRING  1
#define MSG_OPTIONS         2

#define MAX_STRING_LEN      100

typedef struct {
    int  fd;
    int  type;
    char data[MAX_STRING_LEN + 1];
    int  length;
} tx_message_t;

#endif /* __WEB_SERVER_H__ */
