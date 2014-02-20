/****************************************************************//**
 *
 * @File phy_ks8721.c
 *
 * @author   Logan Gunthorpe <logang@deltatee.com>
 *
 * @brief    Definitions for the KS8721 PHY
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


#ifndef __PHY_KS8721_H__
#define __PHY_KS8721_H__

#define PHY_KS8721

#define PHY_REG_BASIC_CTRL           0x0
#define PHY_REG_BASIC_STATUS         0x1
#define PHY_REG_ID1                  0x2
#define PHY_REG_ID2                  0x3
#define PHY_REG_AUTO_NEG_AD          0x4
#define PHY_REG_LINK_PARTNER         0x5
#define PHY_REG_AUTO_NEG_EXP         0x6
#define PHY_REG_AUTO_NEG_NP          0x7
#define PHY_REG_LINK_PARTNER_ABILITY 0x8
#define PHY_REG_RXER_COUNTER         0x15
#define PHY_REG_INTERRUPT            0x1b
#define PHY_REG_CONTROLLER           0x1f

#define PHY_IDENTIFIER1              0x0022
#define PHY_IDENTIFIER2              0x1619

#define PHY_BCR_RESET                (1 << 15)
#define PHY_BCR_LOOPBACK             (1 << 14)
#define PHY_BCR_100BASETX            (1 << 13)
#define PHY_BCR_AUTO_NEG             (1 << 12)
#define PHY_BCR_POWER_DOWN           (1 << 11)
#define PHY_BCR_ISOLATE              (1 << 10)
#define PHY_BCR_RESTART_AUTO_NEG     (1 << 9)
#define PHY_BCR_FULL_DUPLEX          (1 << 8)
#define PHY_BCR_COLLISION_TEST       (1 << 7)
#define PHY_BCR_DISABLE_TX           (1 << 0)

#define PHY_BSR_T4                   (1 << 15)
#define PHY_BSR_100BASETX_FULLDPLX   (1 << 14)
#define PHY_BSR_100BASETX_HALFDPLX   (1 << 13)
#define PHY_BSR_10BASET_FULLDPLX     (1 << 12)
#define PHY_BSR_10BASET_HALFDPLX     (1 << 11)
#define PHY_BSR_NO_PREAMBLE          (1 << 6)
#define PHY_BSR_AUTO_NEG_COMPLETE    (1 << 5)
#define PHY_BSR_REMOTE_FAULT         (1 << 4)
#define PHY_BSR_AUTO_NEG_ABILITY     (1 << 3)
#define PHY_BSR_LINK_UP              (1 << 2)
#define PHY_BSR_JABBER_DETECT        (1 << 2)
#define PHY_BSR_EXT_CAP              (1 << 0)

#define PHY_BSR_100BASETX            (PHY_BSR_100BASETX_FULLDPLX | \
                                      PHY_BSR_100BASETX_HALFDPLX)
#define PHY_BSR_FULLDPLX             (PHY_BSR_100BASETX_FULLDPLX | \
                                      PHY_BSR_10BASET_FULLDPLX)

#define PHY_LINK_PARTNER_NEXT_PAGE            (1 << 15)
#define PHY_LINK_PARTNER_ACK                  (1 << 14)
#define PHY_LINK_PARTNER_REMOTE_FAULT         (1 << 13)
#define PHY_LINK_PARTNER_PAUSE                (3 << 11)
#define PHY_LINK_PARTNER_100BASET4            (1 << 9)
#define PHY_LINK_PARTNER_100BASETX_FULLDPLX   (1 << 8)
#define PHY_LINK_PARTNER_100BASETX_HALFDPLX   (1 << 7)
#define PHY_LINK_PARTNER_10BASET_FULLDPLX    (1 << 6)
#define PHY_LINK_PARTNER_10BASET_HALFDPLX    (1 << 5)

#define PHY_LINK_PARTNER_100BASETX   (PHY_LINK_PARTNER_100BASETX_FULLDPLX | \
                                      PHY_LINK_PARTNER_100BASETX_HALFDPLX)
#define PHY_LINK_PARTNER_FULLDPLX    (PHY_LINK_PARTNER_100BASETX_FULLDPLX | \
                                      PHY_LINK_PARTNER_10BASET_FULLDPLX)


#define PHY_IEN_JABBER               (1 << 15)
#define PHY_IEN_RECV_ERR             (1 << 14)
#define PHY_IEN_PAGE_RECVD           (1 << 13)
#define PHY_IEN_PARALLEL_FAULT       (1 << 12)
#define PHY_IEN_LINK_PARTNER_ACK     (1 << 11)
#define PHY_IEN_LINK_DOWN            (1 << 10)
#define PHY_IEN_REMOTE_FAULT         (1 << 9)
#define PHY_IEN_LINK_UP              (1 << 8)

#define PHY_IF_JABBER               (1 << 7)
#define PHY_IF_RECV_ERR             (1 << 6)
#define PHY_IF_PAGE_RECVD           (1 << 5)
#define PHY_IF_PARALLEL_FAULT       (1 << 4)
#define PHY_IF_LINK_PARTNER_ACK     (1 << 3)
#define PHY_IF_LINK_DOWN            (1 << 2)
#define PHY_IF_REMOTE_FAULT         (1 << 1)
#define PHY_IF_LINK_UP              (1 << 0)

#define PHY_CTRL_PAIRSWAP_DIS       (1 << 13)
#define PHY_CTRL_ENERGY_DETECT      (1 << 12)
#define PHY_CTRL_FORCE_LINK         (1 << 11)
#define PHY_CTRL_POWER_SAVING       (1 << 10)
#define PHY_CTRL_INTERRUPT_LEVEL    (1 << 9)
#define PHY_CTRL_ENABLE_JABBER      (1 << 8)
#define PHY_CTRL_AUTONEG_COMPLETE   (1 << 7)
#define PHY_CTRL_ENABLE_PAUSE       (1 << 6)
#define PHY_CTRL_PHY_ISOLATE        (1 << 5)
#define PHY_CTRL_MODE               (7 << 2)
#define PHY_CTRL_MODE_AUTONEG             (0 << 2)
#define PHY_CTRL_MODE_10BASET_HALFDUPLX   (1 << 2)
#define PHY_CTRL_MODE_100BASETX_HALFDUPLX (2 << 2)
#define PHY_CTRL_MODE_10BASET_FULLDUPLX   (5 << 2)
#define PHY_CTRL_MODE_100BASETX_FULLDUPLX (6 << 2)
#define PHY_CTRL_MODE_PHY_MII_ISOLATE     (7 << 2)






#endif
