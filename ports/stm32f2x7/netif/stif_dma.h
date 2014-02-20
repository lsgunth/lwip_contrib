/****************************************************************//**
 *
 * @File stif_dma.h
 *
 * @author   Logan Gunthorpe <logang@deltatee.com>
 *
 * @brief    Definitions for the ethernet DMA descriptors
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



#ifndef __STIF_DMA_H__
#define __STIF_DMA_H__

#include <stdint.h>

struct dma_desc {
    volatile uint32_t   Status;
    uint32_t   ControlBufferSize;
    void *     Buffer1Addr;
    void *     Buffer2NextDescAddr;
    uint32_t   ExtendedStatus;
    uint32_t   Reserved1;
    uint32_t   TimeStampLow;
    uint32_t   TimeStampHigh;
    struct pbuf *pbuf;
};

#define ETH_DMATxDesc_OWN                     ((uint32_t)0x80000000)
#define ETH_DMATxDesc_IC                      ((uint32_t)0x40000000)
#define ETH_DMATxDesc_LS                      ((uint32_t)0x20000000)
#define ETH_DMATxDesc_FS                      ((uint32_t)0x10000000)
#define ETH_DMATxDesc_DC                      ((uint32_t)0x08000000)
#define ETH_DMATxDesc_DP                      ((uint32_t)0x04000000)
#define ETH_DMATxDesc_TTSE                    ((uint32_t)0x02000000)
#define ETH_DMATxDesc_CIC                     ((uint32_t)0x00C00000)
#define ETH_DMATxDesc_CIC_ByPass              ((uint32_t)0x00000000)
#define ETH_DMATxDesc_CIC_IPV4Header          ((uint32_t)0x00400000)
#define ETH_DMATxDesc_CIC_TCPUDPICMP_Segment  ((uint32_t)0x00800000)
#define ETH_DMATxDesc_CIC_TCPUDPICMP_Full     ((uint32_t)0x00C00000)
#define ETH_DMATxDesc_TER                     ((uint32_t)0x00200000)
#define ETH_DMATxDesc_TCH                     ((uint32_t)0x00100000)
#define ETH_DMATxDesc_TTSS                    ((uint32_t)0x00020000)
#define ETH_DMATxDesc_IHE                     ((uint32_t)0x00010000)
#define ETH_DMATxDesc_ES                      ((uint32_t)0x00008000)
#define ETH_DMATxDesc_JT                      ((uint32_t)0x00004000)
#define ETH_DMATxDesc_FF                      ((uint32_t)0x00002000)
#define ETH_DMATxDesc_PCE                     ((uint32_t)0x00001000)
#define ETH_DMATxDesc_LCA                     ((uint32_t)0x00000800)
#define ETH_DMATxDesc_NC                      ((uint32_t)0x00000400)
#define ETH_DMATxDesc_LCO                     ((uint32_t)0x00000200)
#define ETH_DMATxDesc_EC                      ((uint32_t)0x00000100)
#define ETH_DMATxDesc_VF                      ((uint32_t)0x00000080)
#define ETH_DMATxDesc_CC                      ((uint32_t)0x00000078)
#define ETH_DMATxDesc_ED                      ((uint32_t)0x00000004)
#define ETH_DMATxDesc_UF                      ((uint32_t)0x00000002)
#define ETH_DMATxDesc_DB                      ((uint32_t)0x00000001)

#define ETH_DMARxDesc_OWN         ((uint32_t)0x80000000)
#define ETH_DMARxDesc_AFM         ((uint32_t)0x40000000)
#define ETH_DMARxDesc_FL          ((uint32_t)0x3FFF0000)
#define ETH_DMARxDesc_ES          ((uint32_t)0x00008000)
#define ETH_DMARxDesc_DE          ((uint32_t)0x00004000)
#define ETH_DMARxDesc_SAF         ((uint32_t)0x00002000)
#define ETH_DMARxDesc_LE          ((uint32_t)0x00001000)
#define ETH_DMARxDesc_OE          ((uint32_t)0x00000800)
#define ETH_DMARxDesc_VLAN        ((uint32_t)0x00000400)
#define ETH_DMARxDesc_FS          ((uint32_t)0x00000200)
#define ETH_DMARxDesc_LS          ((uint32_t)0x00000100)
#define ETH_DMARxDesc_IPV4HCE     ((uint32_t)0x00000080)
#define ETH_DMARxDesc_LC          ((uint32_t)0x00000040)
#define ETH_DMARxDesc_FT          ((uint32_t)0x00000020)
#define ETH_DMARxDesc_RWT         ((uint32_t)0x00000010)
#define ETH_DMARxDesc_RE          ((uint32_t)0x00000008)
#define ETH_DMARxDesc_DBE         ((uint32_t)0x00000004)
#define ETH_DMARxDesc_CE          ((uint32_t)0x00000002)
#define ETH_DMARxDesc_MAMPCE      ((uint32_t)0x00000001)

#define ETH_DMARxDesc_DIC   ((uint32_t)0x80000000)
#define ETH_DMARxDesc_RBS2  ((uint32_t)0x1FFF0000)
#define ETH_DMARxDesc_RER   ((uint32_t)0x00008000)
#define ETH_DMARxDesc_RCH   ((uint32_t)0x00004000)
#define ETH_DMARxDesc_RBS1  ((uint32_t)0x00001FFF)


#endif
