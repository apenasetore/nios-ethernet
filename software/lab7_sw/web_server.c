/*
 * Lab7 - NIOS II Web Server with Ethernet + User HW
 *
 * 	Two mandatory threads:
 *  RXTask - receives packets from the network
 *  TXTask - transmits packets to the network
 *
 * User_HW: adds 1 to each ASCII char (0xF7 -> 0x00)
 * DHCP is used to obtain the IP address.
 */

#include "includes.h"

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include "system.h"
#include "io.h"

#include "dm9000a.h"
#include "alt_error_handler.h"
#include "web_server.h"

/* NicheStack */
#include "ipport.h"
#include "libport.h"
#include "osport.h"
#include "tcpport.h"
#include "net.h"

#ifndef ALT_INICHE
  #error This application requires the Interniche IP Stack Software Component.
#endif

#ifndef __ucosii__
  #error This application requires MicroC/OS-II.
#endif

/* ------------------------------------------------------------------ */
/*  NicheStack network structure                                      */
/* ------------------------------------------------------------------ */
extern struct net netstatic[STATIC_NETS];

/* ------------------------------------------------------------------ */
/*  NicheStack task declarations (socket-using tasks)                  */
/* ------------------------------------------------------------------ */
TK_OBJECT(to_rxtask);
TK_ENTRY(RXTask);

TK_OBJECT(to_txtask);
TK_ENTRY(TXTask);

struct inet_taskinfo rxtaskinfo = {
    &to_rxtask,
    "rx task",
    RXTask,
    RX_PRIO,
    APP_STACK_SIZE,
};

struct inet_taskinfo txtaskinfo = {
    &to_txtask,
    "tx task",
    TXTask,
    TX_PRIO,
    APP_STACK_SIZE,
};

/* ------------------------------------------------------------------ */
/*  Shared message + synchronization                                   */
/* ------------------------------------------------------------------ */
static tx_message_t shared_msg;
OS_EVENT *tx_mbox;
OS_EVENT *ack_sem;

OS_STK WSInitialTaskStk[TASK_STACKSIZE];

/* ------------------------------------------------------------------ */
/*  Embedded HTML page (served by NIOS when browser GETs /)            */
/* ------------------------------------------------------------------ */
static const char html_page[] =
    "<!DOCTYPE html><html><head><meta charset=\"utf-8\">"
    "<title>Lab7 NIOS II</title>"
    "<style>body{font-family:sans-serif;max-width:600px;margin:40px auto}"
    "input{width:400px;padding:6px}button{padding:6px 18px}"
    "pre{background:#eee;padding:12px;min-height:24px}</style>"
    "</head><body>"
    "<h2>NIOS II &mdash; String Processor (User HW)</h2>"
    "<p>Digite uma string (max 100 caracteres):</p>"
    "<input type=\"text\" id=\"s\" maxlength=\"100\">"
    "<button onclick=\"go()\">Enviar</button>"
    "<h3>String original:</h3><pre id=\"o\"></pre>"
    "<h3>String processada (char+1):</h3><pre id=\"r\"></pre>"
    "<script>"
    "function go(){"
    "var s=document.getElementById('s').value;"
    "document.getElementById('o').textContent=s;"
    "var x=new XMLHttpRequest();"
    "x.open('POST','/process',true);"
    "x.setRequestHeader('Content-Type','text/plain');"
    "x.onload=function(){document.getElementById('r').textContent=x.responseText;};"
    "x.onerror=function(){document.getElementById('r').textContent='Erro de conexao';};"
    "x.send(s);}"
    "</script></body></html>";

/* ------------------------------------------------------------------ */
/*  User_HW access functions                                           */
/*                                                                     */
/*  Register map (sem BRAM, usa array interno de 100 bytes):           */
/*    offset 0 (addr "000"): addr_reg  - indice do byte (0..99)        */
/*    offset 1 (addr "001"): data_reg  - byte original / processado    */
/*    offset 2 (addr "010"): ctrl_reg  - bit0=gravar, bit1=ler         */
/*    offset 3 (addr "011"): len_reg   - comprimento da string         */
/* ------------------------------------------------------------------ */

static void hw_write_byte(int index, unsigned char byte_val)
{
    IOWR_32DIRECT(USER_HW_0_BASE, 0 * 4, index);      /* addr_reg  */
    IOWR_32DIRECT(USER_HW_0_BASE, 1 * 4, byte_val);   /* data_reg  */
    IOWR_32DIRECT(USER_HW_0_BASE, 2 * 4, 0x01);       /* ctrl: write */
}

static unsigned char hw_read_byte(int index)
{
    IOWR_32DIRECT(USER_HW_0_BASE, 0 * 4, index);      /* addr_reg  */
    IOWR_32DIRECT(USER_HW_0_BASE, 2 * 4, 0x02);       /* ctrl: read */
    return (unsigned char)(IORD_32DIRECT(USER_HW_0_BASE, 1 * 4) & 0xFF);
}

static void process_string_hw(const char *input, char *output, int len)
{
    int i;

    /* Gravar comprimento no len_reg */
    IOWR_32DIRECT(USER_HW_0_BASE, 3 * 4, len);

    /* Escrever string original no array interno do User_HW */
    for (i = 0; i < len; i++)
        hw_write_byte(i, (unsigned char)input[i]);

    /* Ler string processada (char+1, 0xF7->0x00) */
    for (i = 0; i < len; i++)
        output[i] = (char)hw_read_byte(i);

    output[len] = '\0';
}

/* ------------------------------------------------------------------ */
/*  TX response buffer (static to save stack space)                    */
/* ------------------------------------------------------------------ */
static char tx_response[2048];

/* ------------------------------------------------------------------ */
/*  RX Task  -  Thread de recepcao de pacotes                          */
/* ------------------------------------------------------------------ */
void RXTask(void *pdata)
{
    int listen_fd, conn_fd;
    struct sockaddr_in addr, client_addr;
    int client_len;
    char rx_buf[2048];
    int rx_len;
    INT8U err;

    /* Create listening socket on port 80 */
    listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        printf("[RX] socket() failed\n");
        while (1) TK_SLEEP(100);
    }

    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(80);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(listen_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        printf("[RX] bind() failed\n");
        while (1) TK_SLEEP(100);
    }

    if (listen(listen_fd, 3) < 0) {
        printf("[RX] listen() failed\n");
        while (1) TK_SLEEP(100);
    }

    printf("[RX] Listening on port 80...\n");

    while (1) {
        client_len = sizeof(client_addr);
        conn_fd = accept(listen_fd, (struct sockaddr *)&client_addr, &client_len);
        if (conn_fd < 0)
            continue;

        memset(rx_buf, 0, sizeof(rx_buf));
        rx_len = recv(conn_fd, rx_buf, sizeof(rx_buf) - 1, 0);
        if (rx_len <= 0) {
            close(conn_fd);
            continue;
        }
        rx_buf[rx_len] = '\0';

        /* ---- OPTIONS (CORS preflight) ---- */
        if (strstr(rx_buf, "OPTIONS")) {
            shared_msg.fd   = conn_fd;
            shared_msg.type = MSG_OPTIONS;
            OSMboxPost(tx_mbox, (void *)&shared_msg);
            OSSemPend(ack_sem, 0, &err);
        }
        /* ---- POST /process ---- */
        else if (strstr(rx_buf, "POST /process")) {
            char *body = strstr(rx_buf, "\r\n\r\n");
            if (body) {
                body += 4;
                int slen = strlen(body);
                if (slen > MAX_STRING_LEN) slen = MAX_STRING_LEN;

                char processed[MAX_STRING_LEN + 1];
                process_string_hw(body, processed, slen);

                printf("[RX] Received: '%.*s'\n", slen, body);
                printf("[RX] Processed: '%s'\n", processed);

                shared_msg.fd     = conn_fd;
                shared_msg.type   = MSG_PROCESS_STRING;
                shared_msg.length = slen;
                memcpy(shared_msg.data, processed, slen + 1);
                OSMboxPost(tx_mbox, (void *)&shared_msg);
                OSSemPend(ack_sem, 0, &err);
            } else {
                close(conn_fd);
            }
        }
        /* ---- GET / (serve HTML page) ---- */
        else if (strstr(rx_buf, "GET")) {
            shared_msg.fd   = conn_fd;
            shared_msg.type = MSG_SERVE_HTML;
            OSMboxPost(tx_mbox, (void *)&shared_msg);
            OSSemPend(ack_sem, 0, &err);
        }
        /* ---- Unknown request ---- */
        else {
            close(conn_fd);
        }
    }
}

/* ------------------------------------------------------------------ */
/*  TX Task  -  Thread de transmissao de pacotes                       */
/* ------------------------------------------------------------------ */
void TXTask(void *pdata)
{
    INT8U err;
    tx_message_t *msg;
    int resp_len;

    while (1) {
        msg = (tx_message_t *)OSMboxPend(tx_mbox, 0, &err);
        if (err != OS_ERR_NONE || msg == NULL) {
            OSSemPost(ack_sem);
            continue;
        }

        switch (msg->type) {

        case MSG_SERVE_HTML:
            resp_len = sprintf(tx_response,
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: text/html; charset=utf-8\r\n"
                "Content-Length: %d\r\n"
                "Access-Control-Allow-Origin: *\r\n"
                "Connection: close\r\n"
                "\r\n%s",
                (int)strlen(html_page), html_page);
            send(msg->fd, tx_response, resp_len, 0);
            printf("[TX] Served HTML page\n");
            break;

        case MSG_PROCESS_STRING:
            resp_len = sprintf(tx_response,
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: text/plain; charset=utf-8\r\n"
                "Content-Length: %d\r\n"
                "Access-Control-Allow-Origin: *\r\n"
                "Connection: close\r\n"
                "\r\n%s",
                msg->length, msg->data);
            send(msg->fd, tx_response, resp_len, 0);
            printf("[TX] Sent processed string: '%s'\n", msg->data);
            break;

        case MSG_OPTIONS:
            resp_len = sprintf(tx_response,
                "HTTP/1.1 200 OK\r\n"
                "Access-Control-Allow-Origin: *\r\n"
                "Access-Control-Allow-Methods: POST, GET, OPTIONS\r\n"
                "Access-Control-Allow-Headers: Content-Type\r\n"
                "Content-Length: 0\r\n"
                "Connection: close\r\n"
                "\r\n");
            send(msg->fd, tx_response, resp_len, 0);
            break;
        }

        close(msg->fd);
        OSSemPost(ack_sem);
    }
}

/* ------------------------------------------------------------------ */
/*  Initial task: inits NicheStack, starts RX + TX tasks               */
/* ------------------------------------------------------------------ */
void WSInitialTask(void *pdata)
{
    INT8U error_code;
    ip_addr *ipaddr;

    alt_iniche_init();
    netmain();

    while (!iniche_net_ready)
        TK_SLEEP(1);

    /* Display obtained IP */
    ipaddr = &nets[0]->n_ipaddr;
    printf("\n========================================\n");
    printf("  IP via DHCP: %d.%d.%d.%d\n",
        ip4_addr1(*ipaddr),
        ip4_addr2(*ipaddr),
        ip4_addr3(*ipaddr),
        ip4_addr4(*ipaddr));
    printf("  Open http://%d.%d.%d.%d/ in browser\n",
        ip4_addr1(*ipaddr),
        ip4_addr2(*ipaddr),
        ip4_addr3(*ipaddr),
        ip4_addr4(*ipaddr));
    printf("========================================\n\n");

    /* Create synchronization */
    tx_mbox = OSMboxCreate(NULL);
    ack_sem = OSSemCreate(0);

    /* Start RX and TX tasks */
    TK_NEWTASK(&rxtaskinfo);
    TK_NEWTASK(&txtaskinfo);

    printf("Web Server started with RX + TX threads.\n");

    /* Delete this init task */
    error_code = OSTaskDel(OS_PRIO_SELF);
    alt_uCOSIIErrorHandler(error_code, 0);

    while (1);
}

/* ------------------------------------------------------------------ */
/*  main()                                                             */
/* ------------------------------------------------------------------ */
int main(int argc, char *argv[], char *envp[])
{
    INT8U error_code;

    /* Initialize the DM9000A ethernet controller */
    DM9000A_INSTANCE(DM9000A_0, dm9000a_0);
    DM9000A_INIT(DM9000A_0, dm9000a_0);

    OSTimeSet(0);

    error_code = OSTaskCreateExt(WSInitialTask,
                                 NULL,
                                 (void *)&WSInitialTaskStk[TASK_STACKSIZE - 1],
                                 WS_INITIAL_TASK_PRIO,
                                 WS_INITIAL_TASK_PRIO,
                                 WSInitialTaskStk,
                                 TASK_STACKSIZE,
                                 NULL,
                                 0);
    alt_uCOSIIErrorHandler(error_code, 0);

    OSStart();

    while (1);
    return -1;
}
