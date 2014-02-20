/****************************************************************//**
 *
 * @File stif.c
 *
 * @author   Logan Gunthorpe <logang@deltatee.com>
 *
 * @brief    STM32F2x7 interface driver
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


#include "lwip/mem.h"
#include "netif/etharp.h"
#include "lwip/igmp.h"
#include "stif.h"
#include "stif_dma.h"

#include "config.h"
#include "phy_ks8721.h"
#include <stmlib/clock.h>
#include <stmlib/int.h>

#include <string.h>

#ifndef STIF_NUM_TX_DMA_DESC
#define STIF_NUM_TX_DMA_DESC 20
#endif
static struct dma_desc tx_dma_desc[STIF_NUM_TX_DMA_DESC];
static struct dma_desc *tx_cur_dma_desc;

#ifndef STIF_NUM_RX_DMA_DESC
#define STIF_NUM_RX_DMA_DESC 5
#endif
static struct dma_desc rx_dma_desc[STIF_NUM_RX_DMA_DESC];
static struct dma_desc *rx_cur_dma_desc;

static enum {
    NO_CHANGE,
    LINK_UP,
    LINK_DOWN,
} phy_link_status;

static void mac_reset(void)
{
    ETH->DMABMR |= ETH_DMABMR_SR;
    while (ETH->DMABMR & ETH_DMABMR_SR);
}

static int phy_read_reg(int phy_reg)
{
    int regval = ETH->MACMIIAR;
    regval &= ~(ETH_MACMIIAR_MR | ETH_MACMIIAR_MW);
    regval |= phy_reg << 6;
    regval |= ETH_MACMIIAR_MB;

    ETH->MACMIIAR = regval;

    int timeout = 0x50000;
    while(ETH->MACMIIAR & ETH_MACMIIAR_MB) {
        if (timeout-- <= 0)
            return 0;
    }

    return ETH->MACMIIDR;
}

static void phy_write_reg(int phy_reg, int value)
{
    int regval = ETH->MACMIIAR;
    regval &= ~ETH_MACMIIAR_MR;
    regval |= phy_reg << 6;
    regval |= ETH_MACMIIAR_MW | ETH_MACMIIAR_MB;

    ETH->MACMIIDR = value;
    ETH->MACMIIAR = regval;

    int timeout = 0x50000;
    while(ETH->MACMIIAR & ETH_MACMIIAR_MB) {
        if (timeout-- <= 0)
            break;
    }
}

static err_t phy_setup_access(void)
{
    int hclk = clock_get_freq(ETH);
    int cr_div;

    if (hclk >= 20000000 && hclk < 35000000)
        cr_div = ETH_MACMIIAR_CR_Div16;
    else if (hclk >= 35000000 && hclk < 60000000)
        cr_div = ETH_MACMIIAR_CR_Div26;
    else if (hclk >= 60000000 && hclk < 100000000)
        cr_div = ETH_MACMIIAR_CR_Div42;
    else
        cr_div = ETH_MACMIIAR_CR_Div62;


    int phy_addr;
    for(phy_addr = 1; phy_addr <= 32; phy_addr++)
    {
        ETH->MACMIIAR = (phy_addr << 11) | cr_div;

        if (phy_read_reg(PHY_REG_ID1) == PHY_IDENTIFIER1 &&
            phy_read_reg(PHY_REG_ID2) == PHY_IDENTIFIER2)
            break;
    }


    if(phy_addr > 32)
    {
        LWIP_DEBUGF(NETIF_DEBUG | LWIP_DBG_STATE, ("stif: no phy found!\n"));
        return ERR_IF;
    }

    LWIP_DEBUGF(NETIF_DEBUG | LWIP_DBG_STATE, ("stif: phy address %d\n", phy_addr));

    int status = phy_read_reg(PHY_REG_BASIC_STATUS);
    if (status & PHY_BSR_LINK_UP)
        phy_link_status = LINK_UP;

    return phy_addr;
}

static err_t mac_init(void)
{
    mac_reset();
    phy_setup_access();

    ETH->MACCR |= (ETH_MACCR_FES |
                   ETH_MACCR_ROD |
                   ETH_MACCR_IPCO |
                   ETH_MACCR_DM);

    ETH->MACFFR = 0;
    ETH->MACFCR = 0;

    ETH->DMAOMR = (ETH_DMAOMR_DTCEFD |
                   ETH_DMAOMR_RSF |
                   ETH_DMAOMR_TSF |
                   ETH_DMAOMR_OSF);

    ETH->DMABMR = (ETH_DMABMR_AAB |
                   ETH_DMABMR_FB |
                   ETH_DMABMR_RTPR_2_1 |
                   (32 << 17) |  //RX Burst Length
                   (32 << 8)  |  //TX Burst Length
                   ETH_DMABMR_USP |
                   ETH_DMABMR_EDE);

    return ERR_OK;
}

static void init_tx_dma_desc(void)
{
    for (int i = 0; i < STIF_NUM_TX_DMA_DESC; i++) {
        tx_dma_desc[i].Status = ETH_DMATxDesc_TCH | ETH_DMATxDesc_CIC_TCPUDPICMP_Full;
        tx_dma_desc[i].pbuf = NULL;
        tx_dma_desc[i].Buffer2NextDescAddr = &tx_dma_desc[i+1];
    }

    tx_dma_desc[STIF_NUM_TX_DMA_DESC-1].Buffer2NextDescAddr = &tx_dma_desc[0];

    ETH->DMATDLAR = (uint32_t) tx_dma_desc;
    tx_cur_dma_desc = &tx_dma_desc[0];
}


static void init_rx_dma_desc(void)
{
    for (int i = 0; i < STIF_NUM_RX_DMA_DESC; i++) {
        rx_dma_desc[i].Status = ETH_DMARxDesc_OWN;
        rx_dma_desc[i].ControlBufferSize = ETH_DMARxDesc_RCH | PBUF_POOL_BUFSIZE;
        rx_dma_desc[i].pbuf = pbuf_alloc(PBUF_RAW, PBUF_POOL_BUFSIZE, PBUF_POOL);
        rx_dma_desc[i].Buffer1Addr = rx_dma_desc[i].pbuf->payload;
        rx_dma_desc[i].Buffer2NextDescAddr = &rx_dma_desc[i+1];
    }

    rx_dma_desc[STIF_NUM_RX_DMA_DESC-1].Buffer2NextDescAddr = &rx_dma_desc[0];
    ETH->DMARDLAR = (uint32_t) rx_dma_desc;
    rx_cur_dma_desc = &rx_dma_desc[0];
}

#if LWIP_IGMP

static err_t mac_filter(struct netif *netif, ip_addr_t *group, u8_t action)
{
    uint32_t macaddr_low = 0x5E0001 | ((ip4_addr2(group) & 0x7F) << 24);
    uint32_t macaddr_high = ip4_addr3(group) | (ip4_addr4(group) << 8);

    LWIP_DEBUGF(NETIF_DEBUG | LWIP_DBG_STATE,
                ("stif: adding mac filter for %08lx%04lx\n",
               htonl(macaddr_low), htonl(macaddr_high)));

    __IO uint32_t *mach = &ETH->MACA1HR;
    __IO uint32_t *macl = &ETH->MACA1LR;
    int slot;

    for (slot = 0; slot < 3; slot++) {
        if (macl[slot*2] != macaddr_low)
            continue;
        if ((mach[slot*2] & 0xFFFF) == macaddr_high)
            break;
    }

    if (slot != 3) {
        if (action == IGMP_ADD_MAC_FILTER)
            mach[slot*2] |= ETH_MACA1HR_AE;
        else if (action == IGMP_DEL_MAC_FILTER)
            mach[slot*2] &= ~ETH_MACA1HR_AE;

        return ERR_OK;
    }

    if (action == IGMP_DEL_MAC_FILTER)
        return ERR_OK;

    for (slot = 0; slot < 3; slot++) {
        if (mach[slot*2] & ETH_MACA1HR_AE)
            continue;

        macl[slot*2] = macaddr_low;
        mach[slot*2] = macaddr_high | ETH_MACA1HR_AE;
        return ERR_OK;
    }

    LWIP_DEBUGF(NETIF_DEBUG | LWIP_DBG_STATE,
                ("stif: all filter slots used up; enabling pass-all-multiplex\n"));

    ETH->MACFFR |= ETH_MACFFR_PAM;

    return ERR_OK;
}

#endif

static void read_hwaddr_from_otp(struct netif *netif)
{
    memcpy(netif->hwaddr, (void *) 0x1FFF7800, ETHARP_HWADDR_LEN);

    if (memcmp(netif->hwaddr, "\xFF\xFF\xFF\xFF\xFF\xFF", 6) != 0)
        return;

    //Fall back on data from the Unique Device ID Registers
    netif->hwaddr[0] = 0x32;
    netif->hwaddr[1] = 0xCC;
    netif->hwaddr[2] = 0xDD;
    memcpy(&netif->hwaddr[3], (void *) 0x1FFF7A10, 3);
}

__attribute__((__interrupt__))
static void eth_interrupt(void)
{
    //Just used to wakeup from sleep, reception is handled in the main loop.
    ETH->DMASR = ETH_DMASR_ERS | ETH_DMASR_RS | ETH_DMASR_NIS;
}

static void low_level_init(struct netif *netif)
{
    /* set MAC hardware address length */
    netif->hwaddr_len = ETHARP_HWADDR_LEN;

    read_hwaddr_from_otp(netif);

    ETH->MACA0HR = ((netif->hwaddr[5] << 8) |
                    (netif->hwaddr[4] << 0));
    ETH->MACA0LR = ((netif->hwaddr[3] << 24) |
                    (netif->hwaddr[2] << 16) |
                    (netif->hwaddr[1] << 8) |
                    (netif->hwaddr[0] << 0));

    /* maximum transfer unit */
    netif->mtu = 1500;

    netif->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP;

    #if LWIP_IGMP
    netif->flags |= NETIF_FLAG_IGMP;
    netif_set_igmp_mac_filter(netif, mac_filter);
    #endif

    init_tx_dma_desc();
    init_rx_dma_desc();

    /* Enable MAC and DMA transmission and reception */
    ETH->MACCR |= (ETH_MACCR_TE | ETH_MACCR_RE);
    ETH->DMAOMR |= ETH_DMAOMR_FTF;
    ETH->DMAOMR |= ETH_DMAOMR_ST | ETH_DMAOMR_SR;

    ETH->DMAIER = ETH_DMAIER_ERIE | ETH_DMAIER_RIE | ETH_DMAIER_NISE;

    int_register(ETH_IRQn, eth_interrupt);
    int_enable(ETH_IRQn);

}


static void prepare_tx_descr(struct pbuf *p, int first, int last)
{
    while (tx_cur_dma_desc->Status & ETH_DMATxDesc_OWN);

    if (tx_cur_dma_desc->pbuf != NULL)
        pbuf_free(tx_cur_dma_desc->pbuf);

    pbuf_ref(p);
    tx_cur_dma_desc->pbuf = p;

    tx_cur_dma_desc->Status &= ~(ETH_DMATxDesc_FS | ETH_DMATxDesc_LS);
    if (first)
        tx_cur_dma_desc->Status |= ETH_DMATxDesc_FS;
    if (last)
        tx_cur_dma_desc->Status |= ETH_DMATxDesc_LS;

    tx_cur_dma_desc->Buffer1Addr = p->payload;
    tx_cur_dma_desc->ControlBufferSize = p->len;
    tx_cur_dma_desc->Status |= ETH_DMATxDesc_OWN;

    tx_cur_dma_desc = tx_cur_dma_desc->Buffer2NextDescAddr;
}

static err_t low_level_output(struct netif *netif, struct pbuf *p)
{
    struct pbuf *q;

    for(q = p; q != NULL; q = q->next)
        prepare_tx_descr(q, q == p, q->next == NULL);

    if (ETH->DMASR & ETH_DMASR_TBUS) {
        ETH->DMASR = ETH_DMASR_TBUS;
        ETH->DMATPDR = 0;
    }

    return ERR_OK;
}

static void setup_speed_and_duplex(void)
{
    int mode = phy_read_reg(PHY_REG_CONTROLLER) & PHY_CTRL_MODE;

    int regval = ETH->MACCR;
    regval &= ~(ETH_MACCR_DM | ETH_MACCR_FES);

    switch (mode) {
    case PHY_CTRL_MODE_10BASET_HALFDUPLX:
        LWIP_DEBUGF(NETIF_DEBUG | LWIP_DBG_STATE,
                    ("stif_input: speed 10Mb/s half duplex\n"));
        break;
    case PHY_CTRL_MODE_100BASETX_HALFDUPLX:
        LWIP_DEBUGF(NETIF_DEBUG | LWIP_DBG_STATE,
                    ("stif_input: speed 100Mb/s half duplex\n"));
        regval |= ETH_MACCR_FES;
        break;
    case PHY_CTRL_MODE_10BASET_FULLDUPLX:
        LWIP_DEBUGF(NETIF_DEBUG | LWIP_DBG_STATE,
                    ("stif_input: speed 10Mb/s full duplex\n"));
        regval |= ETH_MACCR_DM;
        break;
    case PHY_CTRL_MODE_100BASETX_FULLDUPLX:
        LWIP_DEBUGF(NETIF_DEBUG | LWIP_DBG_STATE,
                    ("stif_input: speed 100Mb/s full duplex\n"));
        regval |= ETH_MACCR_FES;
        regval |= ETH_MACCR_DM;
        break;
    default:
        LWIP_DEBUGF(NETIF_DEBUG | LWIP_DBG_STATE,
                    ("stif_input: ERROR: unknown phy operating mode!\n"));
    }

    ETH->MACCR = regval;
}

__attribute__((__interrupt__))
static void phy_interrupt(void)
{
    hw_eth_rmii_mdint_irq_clear();
    int status = phy_read_reg(PHY_REG_INTERRUPT);

    if (status & PHY_IF_LINK_UP)
        phy_link_status = LINK_UP;


    if (status & PHY_IF_LINK_DOWN)
        phy_link_status = LINK_DOWN;

}

static void init_phy_interrupt(void)
{
    int_register(ETH_RMII_MDINT_IRQ, phy_interrupt);
    int_enable(ETH_RMII_MDINT_IRQ);

    phy_write_reg(PHY_REG_INTERRUPT,
                  (PHY_IEN_LINK_UP |
                   PHY_IEN_LINK_DOWN));
}

err_t stif_init(struct netif *netif)
{
    err_t ret;

    LWIP_ASSERT("netif != NULL", (netif != NULL));

#if LWIP_NETIF_HOSTNAME
    /* Initialize interface hostname */
    netif->hostname = "lwip";
#endif /* LWIP_NETIF_HOSTNAME */

    netif->name[0] = 's';
    netif->name[1] = 't';
    netif->output = etharp_output;
    netif->linkoutput = low_level_output;

    if ((ret = mac_init()) != ERR_OK)
        return ret;

    init_phy_interrupt();

    /* initialize the hardware */
    low_level_init(netif);

    return ERR_OK;
}

static int recv_rxdma_buffer(struct netif *netif)
{
    static struct pbuf *first;

    if (rx_cur_dma_desc->Status & ETH_DMARxDesc_OWN)
        return 0;

    if (rx_cur_dma_desc->pbuf == NULL)
        return 1;

    int frame_length = (rx_cur_dma_desc->Status & ETH_DMARxDesc_FL) >> 16;

    if (rx_cur_dma_desc->Status & ETH_DMARxDesc_LS)
        frame_length -=4; // Subtract off the CRC

    rx_cur_dma_desc->pbuf->tot_len = frame_length;
    rx_cur_dma_desc->pbuf->len = frame_length;

    if (rx_cur_dma_desc->Status & ETH_DMARxDesc_FS)
        first = rx_cur_dma_desc->pbuf;
    else
        pbuf_cat(first, rx_cur_dma_desc->pbuf);

    if (rx_cur_dma_desc->Status & ETH_DMARxDesc_LS) {
        if (netif->input(first, netif) != ERR_OK)
        {
            LWIP_DEBUGF(NETIF_DEBUG, ("stif_input: IP input error\n"));
            pbuf_free(first);
        }
    }

    rx_cur_dma_desc->pbuf = NULL;
    rx_cur_dma_desc = rx_cur_dma_desc->Buffer2NextDescAddr;

    return 1;
}

static int realloc_rxdma_buffers(void)
{
    static struct dma_desc *cur_desc = rx_dma_desc;

    if (cur_desc->Status & ETH_DMARxDesc_OWN) {
        cur_desc = rx_cur_dma_desc;
        return 0;
    }

    if (cur_desc->pbuf != NULL)
        return 0;

    cur_desc->pbuf = pbuf_alloc(PBUF_RAW, PBUF_POOL_BUFSIZE, PBUF_POOL);
    if (cur_desc->pbuf == NULL)
        return 1;

    cur_desc->Buffer1Addr = cur_desc->pbuf->payload;
    cur_desc->Status = ETH_DMARxDesc_OWN;

    if (ETH->DMASR & ETH_DMASR_RBUS) {
        ETH->DMASR = ETH_DMASR_RBUS;
        ETH->DMARPDR = 0;
    }

    cur_desc = cur_desc->Buffer2NextDescAddr;

    return 1;
}

static int clean_finished_tx_buffers(void)
{
    //The transmit pbufs normally would get freed when the DMA
    //  descriptor gets to the end of the loop. However, we lazily
    //  can free them early to not waste so much memory.
    int ret = 0;
    static struct dma_desc *cur_desc = tx_dma_desc;

    if (!(cur_desc->Status & ETH_DMATxDesc_OWN)) {
        if (cur_desc->pbuf != NULL)  {
            ret = 1;
            pbuf_free(cur_desc->pbuf);
            cur_desc->pbuf = NULL;
        }
    }

    cur_desc = cur_desc->Buffer2NextDescAddr;
    return ret;
}

int stif_loop(struct netif *netif)
{
    if (phy_link_status == LINK_UP) {
        LWIP_DEBUGF(NETIF_DEBUG | LWIP_DBG_STATE, ("stif_input: link up\n"));
        setup_speed_and_duplex();
        netif_set_link_up(netif);
        phy_link_status = NO_CHANGE;
    } else if (phy_link_status == LINK_DOWN) {
        LWIP_DEBUGF(NETIF_DEBUG | LWIP_DBG_STATE, ("stif_input: link down\n"));
        netif_set_link_down(netif);
        netif_set_down(netif);
        phy_link_status = NO_CHANGE;
    }

    int ret = recv_rxdma_buffer(netif);
    ret |= clean_finished_tx_buffers();
    ret |= realloc_rxdma_buffers();

    return ret;
}
