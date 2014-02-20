/****************************************************************//**
 *
 * @file network.c
 *
 * @author   Logan Gunthorpe <logang@deltatee.com>
 *
 * @brief    Network Setup and Management Code
 *
 * (c) Deltatee Enterprises Ltd. 2013
 *
 ********************************************************************/

#include "network.h"
#include "config.h"
#include "version.h"

#include <lwip/init.h>
#include <lwip/netif.h>
#include <lwip/dhcp.h>
#include <lwip/autoip.h>
#include <lwip/tcp_impl.h>
#include <lwip/iputil.h>
#include <lwip/stats.h>
#include <lwip/igmp.h>

#include <netif/etharp.h>
#include <netif/stif.h>

#include <iperf/iperf_server.h>
#include <mdns/mdns_responder.h>
#include <tcpecho_raw/echo.h>
#include <simple_discovery/simple_discovery.h>

#include <stmlib/debug.h>

#include <stdio.h>
#include <stdmacro.h>

static struct netif netif;

static const struct mdns_service services[] = {
    {
        .name = "\x06_iperf\x04_tcp\x05local",
        .port = IPERF_SERVER_PORT,
    },
    {
        .name = "\x05_echo\x04_tcp\x05local",
        .port = 7,
    },
};

static const char *txt_records[] = {
    "product=LWIP Example",
    "version=" VERSION,
    NULL
};

static void netif_status(struct netif *n)
{
    if (n->flags & NETIF_FLAG_UP) {
        printf("Interface Up %s:\n",
               n->flags & NETIF_FLAG_DHCP ? "(DHCP)" : "(STATIC)");

        printf("  IP Address: " IP_F "\n", IP_ARGS(&n->ip_addr));
        printf("  Net Mask:   " IP_F "\n", IP_ARGS(&n->netmask));
        printf("  Gateway:    " IP_F "\n", IP_ARGS(&n->gw));

        const char *speed = "10Mb/s";
        if (ETH->MACCR & ETH_MACCR_FES)
            speed = "100Mb/s";

        const char *duplex = "Half";
        if (ETH->MACCR & ETH_MACCR_DM)
            duplex = "Full";

        printf("  Mode:       %s  %s Duplex\n", speed, duplex);

    } else {
        printf("Interface Down.\n");
    }
}

static void netif_link(struct netif *n)
{
    static int dhcp_started = 0;

    if (n->flags & NETIF_FLAG_LINK_UP) {
        printf("Link Up.\n");

        if (!dhcp_started) {
            dhcp_started = 1;
            dhcp_start(n);
        }

    } else {
        printf("Link Down.\n");
    }
}

void network_init(void)
{
    struct ip_addr ip_addr = {0};
    struct ip_addr net_mask = {0};
    struct ip_addr gw_addr = {0};

    lwip_init();
    netif_add(&netif, &ip_addr, &net_mask, &gw_addr, NULL, stif_init,
              ethernet_input);
    netif_set_status_callback(&netif, netif_status);
    netif_set_link_callback(&netif, netif_link);
    netif_set_default(&netif);
    netif_set_hostname(&netif, "lwip");

    printf("Mac Address: %02x:%02x:%02x:%02x:%02x:%02x\n",
           netif.hwaddr[0], netif.hwaddr[1], netif.hwaddr[2],
           netif.hwaddr[3], netif.hwaddr[4], netif.hwaddr[5]);

    igmp_start(&netif);

    simple_discovery_init(&netif);
    mdns_responder_init(&netif, services, array_size(services),
                        txt_records);
    iperf_server_init();
    echo_init();
}

static inline int check_timer(void (*tmr_func)(), unsigned long *timer,
                              unsigned long ticks,
                              unsigned long interval)
{
    if (((ticks - *timer)*(1000/TICK_FREQ)) >= interval) {
        *timer = ticks;
        tmr_func();
        return 1;
    }

    return 0;
}

int network_loop(unsigned long ticks)
{
    static unsigned long etharp_timer = 0;
    static unsigned long tcp_timer = 0;
    static unsigned long dhcp_coarse_timer = 0;
    static unsigned long dhcp_fine_timer = 0;
    static unsigned long autoip_timer = 0;

    int ret = stif_loop(&netif);

    if (check_timer(etharp_tmr, &etharp_timer, ticks, ARP_TMR_INTERVAL))
        return ret;

    if (check_timer(tcp_tmr, &tcp_timer, ticks, TCP_TMR_INTERVAL))
        return ret;

    if (check_timer(autoip_tmr, &autoip_timer, ticks, AUTOIP_TMR_INTERVAL))
        return ret;

    if (check_timer(dhcp_coarse_tmr, &dhcp_coarse_timer, ticks,
                    DHCP_COARSE_TIMER_MSECS))
        return ret;

    if (check_timer(dhcp_fine_tmr, &dhcp_fine_timer, ticks,
                    DHCP_FINE_TIMER_MSECS))
        return ret;

    #if LWIP_STATS_DISPLAY
    if (debug_getchar() == 's')
        stats_display();
    #endif

    return ret;
}
