/****************************************************************//**
 *
 * @file simple_discovery.c
 *
 * @author   Logan Gunthorpe <logang@deltatee.com>
 *
 * @brief    Simple discovery service
 *
 * Copyright (c) Deltatee Enterprises Ltd. 2013
 * All rights reserved.
 *
 ********************************************************************/

/* 
 * Redistribution and use in source and binary forms, with or without
 * modification,are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Author: Logan Gunthorpe <logang@deltatee.com>
 *
 */

#include "simple_discovery.h"

#include <lwip/opt.h>
#include <lwip/udp.h>
#include <lwip/igmp.h>
#include <lwip/debug.h>
#include <lwip/mem.h>

#ifndef SIMPLE_DISCOVERY_DEBUG
#define SIMPLE_DISCOVERY_DEBUG LWIP_DBG_OFF
#endif

#ifndef SIMPLE_DISCOVERY_PORT
#define SIMPLE_DISCOVERY_PORT 9824
#endif

#include <stdio.h>
#include <string.h>


struct response {
    ip_addr_t addr;
    char hwaddr[6];
    char hostname[];
} __attribute__((__packed__));

static void recv(void *arg, struct udp_pcb *upcb, struct pbuf *p,
                 ip_addr_t *addr, u16_t port)
{
    pbuf_free(p);

    LWIP_DEBUGF(SIMPLE_DISCOVERY_DEBUG | LWIP_DBG_STATE,
                ("simple discovery from "));
    ip_addr_debug_print(SIMPLE_DISCOVERY_DEBUG | LWIP_DBG_STATE, addr);
    LWIP_DEBUGF(SIMPLE_DISCOVERY_DEBUG | LWIP_DBG_STATE, ("\n"));

    struct netif *netif = (struct netif *) arg;
    int hostlen = strlen(netif->hostname) + 1;

    struct pbuf *sendp = pbuf_alloc(PBUF_TRANSPORT,
                                    sizeof(struct response) + hostlen,
                                    PBUF_RAM);

    struct response *resp = (struct response *) sendp->payload;
    resp->addr = netif->ip_addr;
    memcpy(resp->hwaddr, netif->hwaddr, sizeof(resp->hwaddr));
    memcpy(resp->hostname, netif->hostname, hostlen);

    udp_sendto(upcb, sendp, addr, port);
    pbuf_free(sendp);
}

err_t simple_discovery_init(struct netif *netif)
{
    err_t ret;

    struct udp_pcb *pcb = udp_new();
    if (pcb == NULL)
        return ERR_MEM;

#if LWIP_IGMP
    struct ip_addr ipgroup;
    IP4_ADDR(&ipgroup, 224, 0, 0, 178);
    if ((ret = igmp_joingroup(IP_ADDR_ANY, &ipgroup)) != ERR_OK)
        goto error_exit;
#endif

    if ((ret = udp_bind(pcb, IP_ADDR_ANY, SIMPLE_DISCOVERY_PORT)) != ERR_OK)
        goto error_exit;

    udp_recv(pcb, recv, netif);

    return ERR_OK;

error_exit:
    udp_remove(pcb);
    return ret;

}
