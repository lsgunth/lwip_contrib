/****************************************************************//**
 *
 * @file tftp_server.c
 *
 * @author   Logan Gunthorpe <logang@deltatee.com>
 *
 * @brief    Trivial File Transfer Protocol (RFC 1350)
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

#include "tftp_server.h"

#include <lwip/opt.h>
#include <lwip/udp.h>
#include <lwip/debug.h>

#ifndef TFTP_DEBUG
#define TFTP_DEBUG LWIP_DBG_ON
#endif

#ifndef TFTP_PORT
#define TFTP_PORT 69
#endif

#ifndef TFTP_TIMEOUT_MSECS
#define TFTP_TIMEOUT_MSECS    10000
#endif

#ifndef TFTP_MAX_RETRIES
#define TFTP_MAX_RETRIES      5
#endif

#define RRQ   1
#define WRQ   2
#define DATA  3
#define ACK   4
#define ERROR 5

#define ERROR_FILE_NOT_FOUND    1
#define ERROR_ACCESS_VIOLATION  2
#define ERROR_DISK_FULL         3
#define ERROR_ILLEGAL_OPERATION 4
#define ERROR_UNKNOWN_TRFR_ID   5
#define ERROR_FILE_EXISTS       6
#define ERROR_NO_SUCH_USER      7

#include <string.h>

struct tftp_state {
    const struct tftp_context *ctx;
    struct tftp_handle *handle;
    int blknum;
    struct pbuf *last_data;
    int timer;
    int last_pkt;
    int retries;

    struct udp_pcb *upcb;
    int port;
    ip_addr_t *addr;
};

static struct tftp_state tftp_state;

static void close_handle(struct tftp_state *ts)
{
    ts->port = 0;
    ts->upcb = NULL;
    ts->addr = NULL;
    ts->last_data = NULL;

    if (ts->handle) {
        ts->ctx->close(ts->handle);
        ts->handle = NULL;
        LWIP_DEBUGF(TFTP_DEBUG | LWIP_DBG_STATE,
                    ("tftp: closing\n"));
    }
}

static void send_error(struct udp_pcb *upcb, ip_addr_t *addr, u16_t port,
                       int code, const char *str)
{
    int str_length = strlen(str);

    struct pbuf *p = pbuf_alloc(PBUF_TRANSPORT,  4 + str_length + 1 , PBUF_RAM);
    u16_t *payload = (u16_t *) p->payload;
    payload[0] = htons(ERROR);
    payload[1] = htons(code);
    memcpy(&payload[2], str, str_length+1);

    udp_sendto(upcb, p, addr, port);
    pbuf_free(p);
}

static void send_ack(struct udp_pcb *upcb, ip_addr_t *addr, u16_t port,
                     int blknum)
{
    struct pbuf *p = pbuf_alloc(PBUF_TRANSPORT, 4, PBUF_RAM);
    u16_t *payload = (u16_t *) p->payload;
    payload[0] = htons(ACK);
    payload[1] = htons(blknum);
    udp_sendto(upcb, p, addr, port);
    pbuf_free(p);
}

static void resend_data(struct udp_pcb *upcb, ip_addr_t *addr, u16_t port,
                        struct tftp_state *ts)
{
    struct pbuf *p = pbuf_alloc(PBUF_TRANSPORT, ts->last_data->len, PBUF_RAM);
    pbuf_copy(p, ts->last_data);
    udp_sendto(upcb, p, addr, port);
    pbuf_free(p);
}

static void send_data(struct udp_pcb *upcb, ip_addr_t *addr, u16_t port,
                      struct tftp_state *ts)
{
    ts->last_data = pbuf_alloc(PBUF_RAM, 4 + 512, PBUF_RAM);
    u16_t *payload = (u16_t *) ts->last_data->payload;
    payload[0] = htons(DATA);
    payload[1] = htons(ts->blknum);

    int ret = ts->ctx->read(ts->handle, &payload[2], 512);
    if (ret < 0) {
        send_error(upcb, addr, port, ERROR_ACCESS_VIOLATION,
                   "Error occured while reading the file.");
        pbuf_free(ts->last_data);
        ts->last_data = NULL;
        close_handle(ts);
        return;
    }

    pbuf_realloc(ts->last_data, 4 + ret);
    resend_data(upcb, addr, port, ts);
}

static void recv(void *arg, struct udp_pcb *upcb, struct pbuf *p,
                 ip_addr_t *addr, u16_t port)
{
    struct tftp_state *ts = (struct tftp_state *) arg;
    char *buf = (char *) p->payload;
    u16_t *sbuf = (u16_t *) p->payload;
    int blknum;
    int lastpkt;

    if (ts->port != 0 && port != ts->port) {
        pbuf_free(p);
        return;
    }

    int opcode = ntohs(sbuf[0]);
    buf[p->len] = 0;

    ts->last_pkt = ts->timer;
    ts->retries = 0;

    switch (opcode) {
    case RRQ:
    case WRQ:
        close_handle(ts);

        char *filename = &buf[2];
        char *mode = &buf[2];
        while(*mode) mode++;
        mode++;

        if ((mode - buf) >= p->len)
            break;

        ts->handle = ts->ctx->open(filename, mode, opcode == WRQ);
        ts->blknum = 1;

        if (!ts->handle) {
            send_error(upcb, addr, port, ERROR_FILE_NOT_FOUND,
                       "Unable to open requested file.");
            break;
        }

        LWIP_DEBUGF(TFTP_DEBUG | LWIP_DBG_STATE,
                    ("tftp: %s request from ",
                        (opcode == WRQ) ? "write" : "read"));
        ip_addr_debug_print(TFTP_DEBUG | LWIP_DBG_STATE, addr);
        LWIP_DEBUGF(TFTP_DEBUG | LWIP_DBG_STATE,
                    (" for '%s' mode '%s'\n", filename, mode));

        ts->upcb = upcb;
        ts->addr = addr;
        ts->port = port;

        if (opcode == WRQ)
            send_ack(upcb, addr, port, 0);
        else
            send_data(upcb, addr, port, ts);

        break;

    case DATA:
        if (ts->handle == NULL)
            break;

        blknum = ntohs(sbuf[1]);
        pbuf_header(p, -4);

        int ret = ts->ctx->write(ts->handle, p);
        if (ret < 0) {
            send_error(upcb, addr, port, ERROR_ACCESS_VIOLATION,
                       "error writing file");
            close_handle(ts);
        } else {
            send_ack(upcb, addr, port, blknum);
        }

        if (p->len < 512)
            close_handle(ts);
        break;

    case ACK:
        if (ts->handle == NULL)
            break;

        blknum = ntohs(sbuf[1]);
        if (blknum != ts->blknum)
            break;

        lastpkt = 0;

        if (ts->last_data != NULL) {
            lastpkt = ts->last_data->len != (512+4);
            pbuf_free(ts->last_data);
            ts->last_data = NULL;
        }

        if (!lastpkt) {
            ts->blknum++;
            send_data(upcb, addr, port, ts);
        } else {
            close_handle(ts);
        }

        break;
    }


    pbuf_free(p);
}

err_t tftp_init(const struct tftp_context *ctx)
{
    err_t ret;

    struct udp_pcb *pcb = udp_new();
    if (pcb == NULL)
        return ERR_MEM;

    if ((ret = udp_bind(pcb, IP_ADDR_ANY, TFTP_PORT)) != ERR_OK)
        goto error_exit;

    tftp_state.handle = NULL;
    tftp_state.port = 0;
    tftp_state.ctx = ctx;
    tftp_state.timer = 0;
    tftp_state.last_data = NULL;

    udp_recv(pcb, recv, &tftp_state);

    return ERR_OK;

error_exit:
    udp_remove(pcb);
    return ret;

}

void tftp_tmr(void)
{
    tftp_state.timer++;

    if (tftp_state.handle == NULL)
        return;

    if ((tftp_state.timer - tftp_state.last_pkt) >
        (TFTP_TIMEOUT_MSECS / TFTP_TIMER_MSECS))
    {
        if (tftp_state.last_data != NULL &&
            tftp_state.retries < TFTP_MAX_RETRIES)
        {
            LWIP_DEBUGF(TFTP_DEBUG | LWIP_DBG_STATE,
                    ("tftp: timeout, retrying\n"));

            resend_data(tftp_state.upcb, tftp_state.addr,
                        tftp_state.port, &tftp_state);
            tftp_state.retries++;
        } else {
            LWIP_DEBUGF(TFTP_DEBUG | LWIP_DBG_STATE,
                    ("tftp: timeout\n"));
            close_handle(&tftp_state);
        }

    }
}
