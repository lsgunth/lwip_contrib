/****************************************************************//**
 *
 * @file iperf_server.c
 *
 * @author   Logan Gunthorpe <logang@deltatee.com>
 *
 * @brief    Iperf server implementation
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
 
#include "iperf_server.h"

#include <lwip/tcp.h>
#include <lwip/debug.h>

#include <stdint.h>

#ifndef IPERF_DEBUG
#define IPERF_DEBUG LWIP_DBG_ON
#endif

extern unsigned long ticks;

static unsigned long send_data[TCP_MSS / sizeof(unsigned long)];

#define RUN_NOW 0x00000001

struct client_hdr {
    int32_t flags;
    int32_t numThreads;
    int32_t mPort;
    int32_t bufferlen;
    int32_t mWinBand;
    int32_t mAmount;
};

struct iperf_state {
    struct tcp_pcb *server_pcb;
    struct tcp_pcb *client_pcb;
    unsigned long recv_bytes;
    unsigned long sent_bytes;
    unsigned long recv_start_ticks;
    unsigned long recv_end_ticks;
    unsigned long send_start_ticks;
    unsigned long send_end_ticks;
    int32_t flags;
    int32_t amount;
    int valid_hdr;
};

static err_t disconnect(struct iperf_state *is, struct tcp_pcb *tpcb);

static void print_connection_msg(const char *id, struct tcp_pcb *tpcb)
{
    LWIP_DEBUGF(IPERF_DEBUG | LWIP_DBG_STATE,
                ("iperf: [%s] local ", id));
    ip_addr_debug_print(IPERF_DEBUG | LWIP_DBG_STATE, &tpcb->local_ip);
    LWIP_DEBUGF(IPERF_DEBUG | LWIP_DBG_STATE,
                (" port %d - rem ", tpcb->local_port));
    ip_addr_debug_print(IPERF_DEBUG | LWIP_DBG_STATE, &tpcb->remote_ip);
    LWIP_DEBUGF(IPERF_DEBUG | LWIP_DBG_STATE,
                (" port %d\n", tpcb->remote_port));
}

static const char * calc_prefix(unsigned long *val, int radix)
{
    if (*val > radix*radix*radix) {
        *val /= radix;
        *val *= 10;
        *val /= radix;
        *val /= radix;
        return "G";
    } else if (*val > radix*radix) {
        *val /= radix;
        *val *= 10;
        *val /= radix;
        return "M";
    }  else if (*val > radix) {
        *val *= 10;
        *val /= radix;
        return "K";
    }

    return "";
}

static void print_result(const char *id, unsigned long start_ticks,
                         unsigned long end_ticks, unsigned long bytes)
{
    unsigned long duration = end_ticks - start_ticks;
    duration *= 10;
    duration = (duration + TICK_FREQ/2) / TICK_FREQ;

    unsigned long speed = bytes / duration * 8 * 10;

    const char *size_prefix = calc_prefix(&bytes, 1024);
    const char *speed_prefix = calc_prefix(&speed, 1000);

    LWIP_DEBUGF(IPERF_DEBUG | LWIP_DBG_STATE,
                ("iperf: 0.0- %ld.%ld sec  %ld.%ld %sBytes  %ld.%ld %sbits/sec\n",
                    duration / 10, duration % 10, bytes / 10, bytes % 10,
                    size_prefix, speed / 10, speed % 10, speed_prefix));
}

static void finish_send(struct iperf_state *is, struct tcp_pcb *tpcb)
{
    tcp_sent(tpcb, NULL);
    is->send_end_ticks = ticks;

    print_result("tx", is->send_start_ticks, is->send_end_ticks,
                 is->sent_bytes);

    disconnect(is, tpcb);

}

static err_t sent(void *arg, struct tcp_pcb *tpcb, u16_t len)
{
    struct iperf_state *is = (struct iperf_state *) arg;

    is->sent_bytes += len;

    if (is->amount > 0 && is->sent_bytes > is->amount) {
        finish_send(is, tpcb);
        return ERR_OK;
    } else if (is->amount < 0 && (ticks-is->send_start_ticks) > -is->amount) {
        finish_send(is, tpcb);
        return ERR_OK;
    }

    int space;
    while ((space = tcp_sndbuf(tpcb)) >= sizeof(send_data)) {
        tcp_write(tpcb, send_data, sizeof(send_data), 0);
    }

    return ERR_OK;
}

static err_t connected(void *arg, struct tcp_pcb *tpcb, err_t err)
{
    struct iperf_state *is = (struct iperf_state *) arg;

    print_connection_msg("tx", tpcb);
    tcp_sent(tpcb, sent);
    sent(is, tpcb, 0);

    return ERR_OK;
}

static void connect_error(void *arg, err_t err)
{
    struct iperf_state *is = (struct iperf_state *) arg;
    is->client_pcb = NULL;
    disconnect(is, NULL);
}

static void reverse_connect(struct iperf_state *is, struct ip_addr *remote_ip)
{
    is->client_pcb = tcp_new();
    if (is->client_pcb != NULL) {
        tcp_arg(is->client_pcb, is);
        tcp_err(is->client_pcb, connect_error);
        tcp_connect(is->client_pcb, remote_ip, IPERF_SERVER_PORT,
                    connected);
    }

    is->send_start_ticks = ticks;
}

static err_t disconnect(struct iperf_state *is, struct tcp_pcb *tpcb)
{
    err_t ret;

    if (tpcb != NULL && (ret = tcp_close(tpcb)) != ERR_OK)
        return ret;

    if (tpcb != NULL && is->server_pcb == tpcb) {
        if (is->client_pcb == NULL && !(is->flags & RUN_NOW))
            reverse_connect(is, &tpcb->remote_ip);

        print_result("rx", is->recv_start_ticks, is->recv_end_ticks,
                     is->recv_bytes);

        is->server_pcb = NULL;
    }

    if (is->client_pcb == tpcb)
        is->client_pcb = NULL;

    if (is->client_pcb == NULL && is->server_pcb == NULL) {
        mem_free(is);
    }

    return ERR_OK;
}

static void parse_header(struct iperf_state *is, struct tcp_pcb *tpcb, void *data)
{
    struct client_hdr *chdr = (struct client_hdr *) data;
    is->flags = ntohl(chdr->flags);
    is->amount = ntohl(chdr->mAmount);

    is->amount *= TICK_FREQ;
    is->amount /= 100;

    is->recv_start_ticks = ticks;
    is->valid_hdr = 1;

    if (is->flags & RUN_NOW)
        reverse_connect(is, &tpcb->remote_ip);
}

static err_t recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err)
{
    struct iperf_state *is = (struct iperf_state *) arg;

    if (p == NULL) {
        is->recv_end_ticks = ticks;
        return disconnect(is, tpcb);
    }

    if (!is->valid_hdr)
        parse_header(is, tpcb, p->payload);

    is->recv_bytes += p->tot_len;
    tcp_recved(tpcb, p->tot_len);
    pbuf_free(p);

    return ERR_OK;
}

static err_t accept(void *arg, struct tcp_pcb *newpcb, err_t err)
{
    print_connection_msg("rx", newpcb);

    struct iperf_state *is = mem_malloc(sizeof(struct iperf_state));
    if (is == NULL)
        return ERR_MEM;

    is->server_pcb = newpcb;
    is->client_pcb = NULL;
    is->valid_hdr = 0;
    is->recv_bytes = 0;
    is->sent_bytes = 0;

    tcp_arg(newpcb, is);
    tcp_recv(newpcb, recv);
    return ERR_OK;
}

err_t iperf_server_init(void)
{
    err_t ret = ERR_OK;

    for (int i = 0; i < sizeof(send_data) / sizeof(*send_data); i++)
        send_data[i] = i;

    struct tcp_pcb *pcb;
    pcb = tcp_new();

    if (pcb == NULL)
        return ERR_MEM;

    if ((ret = tcp_bind(pcb, IP_ADDR_ANY, IPERF_SERVER_PORT)) != ERR_OK) {
        tcp_close(pcb);
        return ret;
    }

    pcb = tcp_listen(pcb);
    tcp_accept(pcb, accept);

    return ret;
}
