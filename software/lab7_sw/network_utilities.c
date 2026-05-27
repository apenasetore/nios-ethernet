/*
 * Lab7 - Network utilities
 * Based on Altera NicheStack example, simplified for lab use.
 * Generates MAC address at runtime (no flash dependency).
 * Uses DHCP for IP when DHCP_CLIENT is defined in BSP.
 */

#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>

#include <alt_iniche_dev.h>
#include "ipport.h"
#include "tcpport.h"

#include "alt_types.h"
#include "includes.h"
#include "io.h"
#include "web_server.h"

#define IP4_ADDR(ipaddr, a,b,c,d) ipaddr = \
    htonl((((alt_u32)(a & 0xff) << 24) | ((alt_u32)(b & 0xff) << 16) | \
          ((alt_u32)(c & 0xff) << 8) | (alt_u32)(d & 0xff)))

void die_with_error(char err_msg[DIE_WITH_ERROR_BUFFER])
{
    printf("\n%s\n", err_msg);
    OSTaskDel(OS_PRIO_SELF);
    while (1);
}

int get_mac_addr(NET net, unsigned char mac_addr[6])
{
    alt_u32 ser_num;

    printf("Generating MAC address...\n");
    ser_num = 100000000 + (rand() % 899999999);

    /* Altera Vendor ID: 00:07:ED */
    mac_addr[0] = 0x00;
    mac_addr[1] = 0x07;
    mac_addr[2] = 0xED;
    mac_addr[3] = 0xFF;
    mac_addr[4] = (ser_num >> 8) & 0xFF;
    mac_addr[5] = ser_num & 0xFF;

    printf("MAC address: %02x:%02x:%02x:%02x:%02x:%02x\n",
        mac_addr[0], mac_addr[1], mac_addr[2],
        mac_addr[3], mac_addr[4], mac_addr[5]);

    return 0;
}

int get_ip_addr(alt_iniche_dev *p_dev,
                ip_addr *ipaddr,
                ip_addr *netmask,
                ip_addr *gw,
                int *use_dhcp)
{
    IP4_ADDR(*ipaddr,  IPADDR0,  IPADDR1,  IPADDR2,  IPADDR3);
    IP4_ADDR(*gw,      GWADDR0,  GWADDR1,  GWADDR2,  GWADDR3);
    IP4_ADDR(*netmask, MSKADDR0, MSKADDR1, MSKADDR2, MSKADDR3);

#ifdef DHCP_CLIENT
    *use_dhcp = 1;
    printf("Using DHCP to obtain IP address...\n");
#else
    *use_dhcp = 0;
    printf("Static IP: %d.%d.%d.%d\n",
        ip4_addr1(*ipaddr), ip4_addr2(*ipaddr),
        ip4_addr3(*ipaddr), ip4_addr4(*ipaddr));
#endif

    return 1;
}
